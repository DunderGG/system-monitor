#include "platform/windows/process_details_reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <winternl.h>

#include <spdlog/spdlog.h>

#include "platform/windows/process_access_policy.h"
#include "platform/windows/process_handle.h"
#include "platform/windows/utf8_conversion.h"

namespace sysmon::platform
{

namespace
{

using domain::ProcessDetailsStatus;

constexpr NTSTATUS kStatusSuccess = 0;
constexpr NTSTATUS kStatusBufferOverflow = static_cast<NTSTATUS>(0x80000005L);
constexpr NTSTATUS kStatusInfoLengthMismatch = static_cast<NTSTATUS>(0xC0000004L);
constexpr NTSTATUS kStatusAccessDenied = static_cast<NTSTATUS>(0xC0000022L);
constexpr NTSTATUS kStatusBufferTooSmall = static_cast<NTSTATUS>(0xC0000023L);

// Not in winternl.h's PROCESSINFOCLASS. Returns a UNICODE_STRING followed by
// its characters, and needs only PROCESS_QUERY_LIMITED_INFORMATION.
constexpr ULONG kProcessCommandLineInformation = 60;

// The longest path QueryFullProcessImageNameW can return, plus its null.
constexpr std::size_t kMaxPathBufferCharacters = 32'768;
constexpr std::size_t kInitialCommandLineBufferBytes = 1024;
constexpr int kMaxCommandLineAttempts = 3;

// NtQueryInformationProcess as ntdll.dll exports it, but with the information
// class as a ULONG: winternl.h's PROCESSINFOCLASS cannot hold 60, because its
// enumerators end below 32, and converting a value outside an enumeration's
// range is undefined behaviour. Both are passed as 32-bit values.
using NtQueryInformationProcessFn = NTSTATUS(NTAPI*)(HANDLE process, ULONG informationClass, PVOID information,
                                                     ULONG informationLength, PULONG returnLength);

struct DetailResult
{
    std::optional<std::string> value{};
    ProcessDetailsStatus status{ProcessDetailsStatus::Unavailable};
};

ProcessDetailsStatus statusForError(DWORD error)
{
    return error == ERROR_ACCESS_DENIED ? ProcessDetailsStatus::AccessDenied : ProcessDetailsStatus::Unavailable;
}

ProcessDetailsStatus combine(ProcessDetailsStatus first, ProcessDetailsStatus second)
{
    if (first == ProcessDetailsStatus::AccessDenied || second == ProcessDetailsStatus::AccessDenied) {
        return ProcessDetailsStatus::AccessDenied;
    }
    if (first == ProcessDetailsStatus::Unavailable || second == ProcessDetailsStatus::Unavailable) {
        return ProcessDetailsStatus::Unavailable;
    }
    return ProcessDetailsStatus::Available;
}

NtQueryInformationProcessFn resolveNtQueryInformationProcess()
{
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        spdlog::error("GetModuleHandleW failed for ntdll.dll with error code {}; process command lines unavailable",
                      ::GetLastError());
        return nullptr;
    }

    const auto query =
        reinterpret_cast<NtQueryInformationProcessFn>(::GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (query == nullptr) {
        spdlog::error("GetProcAddress failed for NtQueryInformationProcess in ntdll.dll with error code {}; process "
                      "command lines unavailable",
                      ::GetLastError());
    }
    return query;
}

// Returns the error if the handle's process is not the one with this identity:
// between the process list and OpenProcess, the process can exit and its PID
// be reused, and the new process's details must not be attributed to it.
std::optional<ProcessDetailsStatus> verifyIdentity(HANDLE handle, const domain::ProcessInfo& process)
{
    const auto creationTime = processCreationTime(handle);
    if (!creationTime) {
        const DWORD error = ::GetLastError();
        spdlog::debug("GetProcessTimes failed for PID {} ({}) with error {}", process.pid, process.imageName, error);
        return statusForError(error);
    }

    if (*creationTime != process.creationTime) {
        spdlog::debug("PID {} ({}) was reused by another process before its details were read", process.pid,
                      process.imageName);
        return ProcessDetailsStatus::Unavailable;
    }
    return std::nullopt;
}

DetailResult queryImagePath(HANDLE handle, const domain::ProcessInfo& process)
{
    std::wstring path(MAX_PATH, L'\0');
    while (true) {
        auto size = static_cast<DWORD>(path.size());
        if (::QueryFullProcessImageNameW(handle, 0, path.data(), &size)) {
            path.resize(size);
            return {.value = wideToUtf8(path), .status = ProcessDetailsStatus::Available};
        }

        const DWORD error = ::GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER && path.size() < kMaxPathBufferCharacters) {
            path.resize(std::min(path.size() * 2, kMaxPathBufferCharacters));
            continue;
        }
        spdlog::debug("QueryFullProcessImageNameW failed for PID {} ({}) with error {}", process.pid, process.imageName,
                      error);
        return {.status = statusForError(error)};
    }
}

// Reads the UNICODE_STRING that ProcessCommandLineInformation wrote at the start
// of the buffer. Returns std::nullopt if its characters are not inside the buffer.
std::optional<std::string> commandLineFrom(const std::vector<uint64_t>& buffer, std::size_t bufferBytes)
{
    UNICODE_STRING text{};
    std::memcpy(&text, buffer.data(), sizeof(text));
    if (text.Length == 0) {
        return std::string{};
    }

    const auto bufferStart = reinterpret_cast<std::uintptr_t>(buffer.data());
    const auto textStart = reinterpret_cast<std::uintptr_t>(text.Buffer);
    if (textStart < bufferStart || textStart - bufferStart > bufferBytes ||
        text.Length > bufferBytes - (textStart - bufferStart)) {
        return std::nullopt;
    }

    std::wstring characters(text.Length / sizeof(WCHAR), L'\0');
    std::memcpy(characters.data(), reinterpret_cast<const std::byte*>(buffer.data()) + (textStart - bufferStart),
                characters.size() * sizeof(WCHAR));
    return wideToUtf8(characters);
}

DetailResult queryCommandLine(NtQueryInformationProcessFn query, HANDLE handle, const domain::ProcessInfo& process)
{
    if (query == nullptr) {
        return {};
    }

    // uint64_t storage keeps the UNICODE_STRING at the start aligned.
    std::vector<uint64_t> buffer(kInitialCommandLineBufferBytes / sizeof(uint64_t));
    for (int attempt = 0; attempt < kMaxCommandLineAttempts; ++attempt) {
        const auto bufferBytes = static_cast<ULONG>(buffer.size() * sizeof(uint64_t));
        ULONG returnLength = 0;
        const NTSTATUS status =
            query(handle, kProcessCommandLineInformation, buffer.data(), bufferBytes, &returnLength);

        if (status == kStatusSuccess) {
            auto commandLine = commandLineFrom(buffer, bufferBytes);
            if (!commandLine) {
                spdlog::debug("NtQueryInformationProcess(ProcessCommandLineInformation) returned a command line "
                              "outside its buffer for PID {} ({})",
                              process.pid, process.imageName);
                return {};
            }
            return {.value = std::move(commandLine), .status = ProcessDetailsStatus::Available};
        }

        const bool isTooSmall =
            status == kStatusInfoLengthMismatch || status == kStatusBufferTooSmall || status == kStatusBufferOverflow;
        if (isTooSmall && returnLength > bufferBytes) {
            buffer.resize((returnLength + sizeof(uint64_t) - 1) / sizeof(uint64_t));
            continue;
        }

        spdlog::debug("NtQueryInformationProcess(ProcessCommandLineInformation) failed for PID {} ({}) with status "
                      "0x{:08X}",
                      process.pid, process.imageName, static_cast<uint32_t>(status));
        return {.status = status == kStatusAccessDenied ? ProcessDetailsStatus::AccessDenied
                                                        : ProcessDetailsStatus::Unavailable};
    }

    spdlog::debug("NtQueryInformationProcess(ProcessCommandLineInformation) buffer still too small for PID {} ({})",
                  process.pid, process.imageName);
    return {};
}

ProcessDetails readProcessDetails(NtQueryInformationProcessFn query, const domain::ProcessInfo& process)
{
    // ProcessCollector already filters these out (DC-13). Checking again here,
    // next to OpenProcess, keeps direct callers that pass the listed name, or
    // no name, from opening them too.
    if (isNeverOpenedProcess(process)) {
        return ProcessDetails{};
    }

    ScopedHandle handle(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid));
    if (!handle) {
        const DWORD error = ::GetLastError();
        spdlog::debug("OpenProcess failed for PID {} ({}) with error {}", process.pid, process.imageName, error);
        return ProcessDetails{.status = statusForError(error)};
    }

    if (const auto identityError = verifyIdentity(handle.get(), process)) {
        return ProcessDetails{.status = *identityError};
    }

    DetailResult imagePath = queryImagePath(handle.get(), process);
    DetailResult commandLine = queryCommandLine(query, handle.get(), process);
    return ProcessDetails{
        .imagePath = std::move(imagePath.value),
        .commandLine = std::move(commandLine.value),
        .status = combine(imagePath.status, commandLine.status),
    };
}

} // namespace

ProcessDetailsReader makeWindowsProcessDetailsReader()
{
    const NtQueryInformationProcessFn query = resolveNtQueryInformationProcess();
    return [query](const domain::ProcessInfo& process) { return readProcessDetails(query, process); };
}

} // namespace sysmon::platform
