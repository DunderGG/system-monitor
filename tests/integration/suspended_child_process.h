#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <windows.h>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "platform/windows/process_handle.h"
#include "platform/windows/utf8_conversion.h"

namespace sysmon::tests
{

/**
 * Asks SuspendedChildProcess for a child that does not run elevated, even
 * when the test does, at the given integrity level: a well-known label SID
 * type, such as WinMediumLabelSid, no higher than this process's level.
 */
struct UnelevatedToken
{
    WELL_KNOWN_SID_TYPE integrityLevel = WinMediumLabelSid;
};

/**
 * A cmd.exe child created suspended, so it never runs, with a command line
 * unique to this test run. Terminated when destroyed. The test holds handles
 * to it until then, so its PID is not reused while the object exists.
 */
class SuspendedChildProcess
{
public:
    /** Creates the child with this process's token, so it is elevated if the test is. */
    SuspendedChildProcess()
    {
        start(nullptr);
    }

    /**
     * Creates the child with a copy of this process's token that is filtered
     * as UAC filters an administrator's token (LUA_TOKEN), keeps no privilege
     * but SeChangeNotifyPrivilege, and has the requested integrity level.
     * started() is false if that token could not be made.
     */
    explicit SuspendedChildProcess(UnelevatedToken token)
    {
        if (const platform::ScopedHandle childToken = unelevatedToken(token.integrityLevel)) {
            start(childToken.get());
        }
    }

    ~SuspendedChildProcess()
    {
        terminate();
    }

    SuspendedChildProcess(const SuspendedChildProcess&) = delete;
    SuspendedChildProcess& operator=(const SuspendedChildProcess&) = delete;
    SuspendedChildProcess(SuspendedChildProcess&&) = delete;
    SuspendedChildProcess& operator=(SuspendedChildProcess&&) = delete;

    [[nodiscard]] bool started() const
    {
        return m_process != nullptr;
    }

    [[nodiscard]] const domain::ProcessIdentity& identity() const
    {
        return m_identity;
    }

    /** The child as the process list names it: its identity and image name. */
    [[nodiscard]] domain::ProcessInfo processInfo() const
    {
        return domain::ProcessInfo{
            .pid = m_identity.pid,
            .creationTime = m_identity.creationTime,
            .imageName = "cmd.exe",
        };
    }

    [[nodiscard]] std::string imagePath() const
    {
        return platform::wideToUtf8(m_imagePath);
    }

    [[nodiscard]] std::string commandLine() const
    {
        return platform::wideToUtf8(m_commandLine);
    }

    /**
     * Waits up to `timeout` for the child to exit and returns its exit code,
     * or std::nullopt if it is still running (or was already terminated with
     * terminate(), which closes the handles).
     */
    [[nodiscard]] std::optional<uint32_t> waitForExitCode(std::chrono::milliseconds timeout) const
    {
        if (!m_process ||
            ::WaitForSingleObject(m_process.get(), static_cast<DWORD>(timeout.count())) != WAIT_OBJECT_0) {
            return std::nullopt;
        }
        DWORD exitCode = 0;
        if (!::GetExitCodeProcess(m_process.get(), &exitCode)) {
            return std::nullopt;
        }
        return exitCode;
    }

    // Terminates the child, waits for it to exit, and closes the handles.
    void terminate()
    {
        if (m_process) {
            ::TerminateProcess(m_process.get(), 0);
            ::WaitForSingleObject(m_process.get(), 5000);
        }
        m_thread.reset();
        m_process.reset();
    }

private:
    // Creates the child with `token`, or with this process's token if it is null.
    void start(HANDLE token)
    {
        std::wstring systemDirectory(MAX_PATH, L'\0');
        systemDirectory.resize(::GetSystemDirectoryW(systemDirectory.data(), MAX_PATH));
        m_imagePath = systemDirectory + L"\\cmd.exe";
        m_commandLine =
            L"\"" + m_imagePath + L"\" /c exit sysmon-test-child-" + std::to_wstring(::GetCurrentProcessId());

        STARTUPINFOW startupInfo{.cb = sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = m_commandLine;
        constexpr DWORD kCreationFlags = CREATE_SUSPENDED | CREATE_NO_WINDOW;
        // A filtered copy of this process's own token needs no privilege to assign.
        const BOOL created =
            token ? ::CreateProcessAsUserW(token, m_imagePath.c_str(), mutableCommandLine.data(), nullptr, nullptr,
                                           FALSE, kCreationFlags, nullptr, nullptr, &startupInfo, &processInfo)
                  : ::CreateProcessW(m_imagePath.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE,
                                     kCreationFlags, nullptr, nullptr, &startupInfo, &processInfo);
        if (created) {
            m_process.reset(processInfo.hProcess);
            m_thread.reset(processInfo.hThread);
            const auto creationTime = platform::processCreationTime(m_process.get());
            m_identity = domain::ProcessIdentity{
                .pid = processInfo.dwProcessId,
                .creationTime = creationTime.value_or(std::chrono::system_clock::time_point{}),
            };
        }
    }

    // A filtered, unelevated copy of this process's primary token at the given
    // integrity level, or null if a call fails.
    static platform::ScopedHandle unelevatedToken(WELL_KNOWN_SID_TYPE integrityLevel)
    {
        HANDLE rawProcessToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(),
                                TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT,
                                &rawProcessToken)) {
            return {};
        }
        const platform::ScopedHandle processToken(rawProcessToken);

        // The new token's handle has the access of the one it was made from.
        HANDLE rawFilteredToken = nullptr;
        if (!::CreateRestrictedToken(processToken.get(), LUA_TOKEN | DISABLE_MAX_PRIVILEGE, 0, nullptr, 0, nullptr, 0,
                                     nullptr, &rawFilteredToken)) {
            return {};
        }
        platform::ScopedHandle filteredToken(rawFilteredToken);

        alignas(SID) std::array<std::byte, SECURITY_MAX_SID_SIZE> labelSid{};
        DWORD labelSidSize = static_cast<DWORD>(labelSid.size());
        if (!::CreateWellKnownSid(integrityLevel, nullptr, labelSid.data(), &labelSidSize)) {
            return {};
        }
        TOKEN_MANDATORY_LABEL label{.Label = {.Sid = labelSid.data(), .Attributes = SE_GROUP_INTEGRITY}};
        if (!::SetTokenInformation(filteredToken.get(), TokenIntegrityLevel, &label,
                                   static_cast<DWORD>(sizeof(label)) + labelSidSize)) {
            return {};
        }
        return filteredToken;
    }

    std::wstring m_imagePath;
    std::wstring m_commandLine;
    platform::ScopedHandle m_process;
    platform::ScopedHandle m_thread;
    domain::ProcessIdentity m_identity;
};

} // namespace sysmon::tests
