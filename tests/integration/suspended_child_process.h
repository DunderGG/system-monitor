#pragma once

#include <chrono>
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
 * A cmd.exe child created suspended, so it never runs, with a command line
 * unique to this test run. Terminated when destroyed. The test holds handles
 * to it until then, so its PID is not reused while the object exists.
 */
class SuspendedChildProcess
{
public:
    SuspendedChildProcess()
    {
        std::wstring systemDirectory(MAX_PATH, L'\0');
        systemDirectory.resize(::GetSystemDirectoryW(systemDirectory.data(), MAX_PATH));
        m_imagePath = systemDirectory + L"\\cmd.exe";
        m_commandLine =
            L"\"" + m_imagePath + L"\" /c exit sysmon-test-child-" + std::to_wstring(::GetCurrentProcessId());

        STARTUPINFOW startupInfo{.cb = sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = m_commandLine;
        if (::CreateProcessW(m_imagePath.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE,
                             CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo)) {
            m_process.reset(processInfo.hProcess);
            m_thread.reset(processInfo.hThread);
            const auto creationTime = platform::processCreationTime(m_process.get());
            m_identity = domain::ProcessIdentity{
                .pid = processInfo.dwProcessId,
                .creationTime = creationTime.value_or(std::chrono::system_clock::time_point{}),
            };
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

    // Terminates the child and closes the handles, so no handle from this
    // process keeps it in the process list.
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
    std::wstring m_imagePath;
    std::wstring m_commandLine;
    platform::ScopedHandle m_process;
    platform::ScopedHandle m_thread;
    domain::ProcessIdentity m_identity;
};

} // namespace sysmon::tests
