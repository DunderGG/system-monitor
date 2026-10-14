#include "platform/windows/process_terminator.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

#include <spdlog/spdlog.h>

#include "platform/windows/process_access_policy.h"
#include "platform/windows/process_handle.h"

namespace sysmon::platform
{

namespace
{

using domain::ProcessTerminationResult;

// The System Idle Process is not a real process, and OpenProcess rejects its
// PID as if no process had it.
constexpr uint32_t kIdleProcessId = 0;

// Enough to check the identity, the owner, and whether the process has exited.
constexpr DWORD kCheckAccess = PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
// Requested only once the current user is known to own the process.
constexpr DWORD kTerminateAccess = kCheckAccess | PROCESS_TERMINATE;

// A non-zero exit code tells the parent, and anything waiting on the process,
// that it did not finish normally.
constexpr UINT kTerminatedExitCode = 1;

// Logs why the process was not terminated, at the level architecture.md gives
// the outcome, and returns the result.
ProcessTerminationResult notTerminated(const domain::ProcessInfo& process, ProcessTerminationResult result,
                                       std::string_view reason)
{
    if (result == ProcessTerminationResult::AlreadyExited) {
        spdlog::info("Did not terminate {} (PID {}): {}", process.imageName, process.pid, reason);
    } else if (result == ProcessTerminationResult::Failed) {
        spdlog::error("Could not terminate {} (PID {}): {}", process.imageName, process.pid, reason);
    } else {
        spdlog::warn("Did not terminate {} (PID {}): {}", process.imageName, process.pid, reason);
    }
    return result;
}

bool hasExited(HANDLE process)
{
    return ::WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
}

ProcessTerminationResult resultForOpenError(DWORD error)
{
    switch (error) {
        // OpenProcess's error for a PID that no process has.
        case ERROR_INVALID_PARAMETER:
            return ProcessTerminationResult::AlreadyExited;
        case ERROR_ACCESS_DENIED:
            return ProcessTerminationResult::AccessDenied;
        default:
            return ProcessTerminationResult::Failed;
    }
}

// The result for a failed call on an open process handle. TerminateProcess
// fails with ERROR_ACCESS_DENIED for a process that has exited, and other
// calls can fail once it has, so that is checked first.
ProcessTerminationResult resultForError(HANDLE process, DWORD error)
{
    if (hasExited(process)) {
        return ProcessTerminationResult::AlreadyExited;
    }
    return error == ERROR_ACCESS_DENIED ? ProcessTerminationResult::AccessDenied : ProcessTerminationResult::Failed;
}

// A handle to the process an identity names, or why there is none.
struct CheckedHandle
{
    ScopedHandle handle;
    std::optional<ProcessTerminationResult> failure;
};

// Opens the process with `access` and checks that it is the process with this
// identity and has not exited.
CheckedHandle openChecked(const domain::ProcessInfo& process, DWORD access)
{
    ScopedHandle handle(::OpenProcess(access, FALSE, process.pid));
    if (!handle) {
        const DWORD error = ::GetLastError();
        return {.failure = notTerminated(process, resultForOpenError(error),
                                         std::format("OpenProcess failed with error {}", error))};
    }

    const auto creationTime = processCreationTime(handle.get());
    if (!creationTime) {
        const DWORD error = ::GetLastError();
        return {.failure = notTerminated(process, resultForError(handle.get(), error),
                                         std::format("GetProcessTimes failed with error {}", error))};
    }
    if (*creationTime != process.creationTime) {
        return {.failure = notTerminated(process, ProcessTerminationResult::AlreadyExited,
                                         "it exited and another process now has its PID")};
    }
    if (hasExited(handle.get())) {
        return {.failure = notTerminated(process, ProcessTerminationResult::AlreadyExited, "it has already exited")};
    }
    return {.handle = std::move(handle)};
}

// Reads the token's TOKEN_USER into storage aligned for it. Returns
// std::nullopt if GetTokenInformation fails; GetLastError() then says why.
std::optional<std::vector<uint64_t>> readTokenUser(HANDLE token)
{
    DWORD size = 0;
    if (!::GetTokenInformation(token, TokenUser, nullptr, 0, &size) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }

    std::vector<uint64_t> buffer((size + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    if (!::GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
        return std::nullopt;
    }
    return buffer;
}

// The user SID in a TOKEN_USER that readTokenUser() read. It points into the
// buffer, so it is valid as long as the buffer is.
PSID userSid(const std::vector<uint64_t>& tokenUser)
{
    TOKEN_USER header{};
    std::memcpy(&header, tokenUser.data(), sizeof(header));
    return header.User.Sid;
}

// Returns why the process must not be terminated if the current user does not
// own it, or std::nullopt if it does. A process's owner is the user of its
// token, the user Task Manager shows for it.
std::optional<ProcessTerminationResult> checkOwner(HANDLE handle, const domain::ProcessInfo& process)
{
    const auto currentUser = readTokenUser(::GetCurrentProcessToken());
    if (!currentUser) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, ProcessTerminationResult::Failed,
                             std::format("reading this process's user failed with error {}", error));
    }

    HANDLE rawToken = nullptr;
    if (!::OpenProcessToken(handle, TOKEN_QUERY, &rawToken)) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(handle, error),
                             std::format("OpenProcessToken failed with error {}", error));
    }
    const ScopedHandle token(rawToken);

    const auto processUser = readTokenUser(token.get());
    if (!processUser) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(handle, error),
                             std::format("GetTokenInformation(TokenUser) failed with error {}", error));
    }

    if (!::EqualSid(userSid(*processUser), userSid(*currentUser))) {
        return notTerminated(process, ProcessTerminationResult::NotOwnedByCurrentUser, "it belongs to another user");
    }
    return std::nullopt;
}

} // namespace

ProcessTerminationResult terminateOwnedProcess(const domain::ProcessInfo& process)
{
    // Checked before any OpenProcess call (DC-74).
    if (isNeverOpenedProcess(process.imageName)) {
        return notTerminated(process, ProcessTerminationResult::NeverOpened, "the monitor never opens it");
    }
    if (process.pid == kIdleProcessId) {
        return notTerminated(process, ProcessTerminationResult::NotOwnedByCurrentUser,
                             "the System Idle Process belongs to the system");
    }

    const CheckedHandle checked = openChecked(process, kCheckAccess);
    if (checked.failure) {
        return *checked.failure;
    }
    if (const auto ownerFailure = checkOwner(checked.handle.get(), process)) {
        return *ownerFailure;
    }

    // The open handle keeps Windows from reusing the PID, so this reaches the
    // process whose owner was checked; openChecked() confirms it again.
    const CheckedHandle terminable = openChecked(process, kTerminateAccess);
    if (terminable.failure) {
        return *terminable.failure;
    }

    if (!::TerminateProcess(terminable.handle.get(), kTerminatedExitCode)) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(terminable.handle.get(), error),
                             std::format("TerminateProcess failed with error {}", error));
    }

    spdlog::info("Terminated {} (PID {})", process.imageName, process.pid);
    return ProcessTerminationResult::Terminated;
}

} // namespace sysmon::platform
