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
#include "platform/windows/process_elevation.h"
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

// Reads token information whose size varies, such as TOKEN_USER, into storage
// aligned for it. Returns std::nullopt if GetTokenInformation fails;
// GetLastError() then says why.
std::optional<std::vector<uint64_t>> readTokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS infoClass)
{
    DWORD size = 0;
    if (!::GetTokenInformation(token, infoClass, nullptr, 0, &size) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }

    std::vector<uint64_t> buffer((size + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    if (!::GetTokenInformation(token, infoClass, buffer.data(), size, &size)) {
        return std::nullopt;
    }
    return buffer;
}

// The user SID in a TOKEN_USER that readTokenInformation() read. It points
// into the buffer, so it is valid as long as the buffer is.
PSID userSid(const std::vector<uint64_t>& tokenUser)
{
    TOKEN_USER header{};
    std::memcpy(&header, tokenUser.data(), sizeof(header));
    return header.User.Sid;
}

// The integrity level in a TOKEN_MANDATORY_LABEL that readTokenInformation()
// read: the last subauthority of its SID, a SECURITY_MANDATORY_*_RID value.
// std::nullopt if the SID has no subauthority.
std::optional<DWORD> integrityRid(const std::vector<uint64_t>& mandatoryLabel)
{
    TOKEN_MANDATORY_LABEL header{};
    std::memcpy(&header, mandatoryLabel.data(), sizeof(header));
    const UCHAR subAuthorityCount = *::GetSidSubAuthorityCount(header.Label.Sid);
    if (subAuthorityCount == 0) {
        return std::nullopt;
    }
    return *::GetSidSubAuthority(header.Label.Sid, subAuthorityCount - 1u);
}

// Returns why the process must not be terminated if the current user does not
// own it, or std::nullopt if it does. A process's owner is the user of its
// token, the user Task Manager shows for it.
std::optional<ProcessTerminationResult> checkOwner(HANDLE handle, HANDLE token, const domain::ProcessInfo& process)
{
    const auto currentUser = readTokenInformation(::GetCurrentProcessToken(), TokenUser);
    if (!currentUser) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, ProcessTerminationResult::Failed,
                             std::format("reading this process's user failed with error {}", error));
    }

    const auto processUser = readTokenInformation(token, TokenUser);
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

// Returns why the process must not be terminated if it runs elevated, or
// std::nullopt if it does not. The monitor ends only unelevated processes,
// even when it runs elevated itself (DC-90).
std::optional<ProcessTerminationResult> checkNotElevated(HANDLE handle, HANDLE token,
                                                         const domain::ProcessInfo& process)
{
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    if (!::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(handle, error),
                             std::format("GetTokenInformation(TokenElevation) failed with error {}", error));
    }

    const auto mandatoryLabel = readTokenInformation(token, TokenIntegrityLevel);
    if (!mandatoryLabel) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(handle, error),
                             std::format("GetTokenInformation(TokenIntegrityLevel) failed with error {}", error));
    }
    const auto rid = integrityRid(*mandatoryLabel);
    if (!rid) {
        return notTerminated(process, ProcessTerminationResult::Failed, "its integrity level SID has no subauthority");
    }

    if (runsElevated(elevation, *rid)) {
        return notTerminated(process, ProcessTerminationResult::Elevated,
                             std::format("it runs elevated (elevated token: {}, integrity level 0x{:X})",
                                         elevation.TokenIsElevated != 0, *rid));
    }
    return std::nullopt;
}

// Returns why the process must not be terminated, from its token: it belongs
// to another user, or it runs elevated. std::nullopt if neither.
std::optional<ProcessTerminationResult> checkToken(HANDLE handle, const domain::ProcessInfo& process)
{
    HANDLE rawToken = nullptr;
    if (!::OpenProcessToken(handle, TOKEN_QUERY, &rawToken)) {
        const DWORD error = ::GetLastError();
        return notTerminated(process, resultForError(handle, error),
                             std::format("OpenProcessToken failed with error {}", error));
    }
    const ScopedHandle token(rawToken);

    if (const auto ownerFailure = checkOwner(handle, token.get(), process)) {
        return ownerFailure;
    }
    return checkNotElevated(handle, token.get(), process);
}

} // namespace

ProcessTerminationResult terminateOwnedProcess(const domain::ProcessInfo& process)
{
    // Checked before any OpenProcess call (DC-74). A process without a name
    // is refused too, except the Idle process, which is handled next.
    if (isNeverOpenedProcess(process)) {
        return notTerminated(process, ProcessTerminationResult::NeverOpened,
                             process.imageName.empty() ? "it has no image name, and could be a process the monitor "
                                                         "never opens"
                                                       : "the monitor never opens it");
    }
    if (process.pid == kIdleProcessId) {
        return notTerminated(process, ProcessTerminationResult::NotOwnedByCurrentUser,
                             "the System Idle Process belongs to the system");
    }

    const CheckedHandle checked = openChecked(process, kCheckAccess);
    if (checked.failure) {
        return *checked.failure;
    }
    if (const auto tokenFailure = checkToken(checked.handle.get(), process)) {
        return *tokenFailure;
    }

    // The open handle keeps Windows from reusing the PID, so this reaches the
    // process whose token was checked; openChecked() confirms it again.
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
