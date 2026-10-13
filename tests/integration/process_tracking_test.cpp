#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>
#include <QObject>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/process_access_policy.h"
#include "platform/windows/process_collector.h"
#include "platform/windows/process_details_reader.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/system_process_information_reader.h"
#include "platform/windows/utf8_conversion.h"

using namespace sysmon::domain;
using namespace sysmon::platform;
using sysmon::monitoring::SamplingScheduler;

namespace
{

struct HandleCloser
{
    void operator()(HANDLE handle) const noexcept
    {
        if (handle != nullptr) {
            ::CloseHandle(handle);
        }
    }
};

using ScopedHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

// A printable, ordered form of a process identity for set comparisons.
using IdentityKey = std::pair<uint32_t, int64_t>;

IdentityKey keyOf(const ProcessIdentity& identity)
{
    return {identity.pid, identity.creationTime.time_since_epoch().count()};
}

std::chrono::system_clock::time_point creationTimeOf(HANDLE process)
{
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    ::GetProcessTimes(process, &creation, &exit, &kernel, &user);
    return fileTimeToSystemClock(
        static_cast<int64_t>((static_cast<uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime));
}

// A cmd.exe child created suspended, so it never runs, with a command line
// unique to this test run. Terminated when destroyed.
class SuspendedChildProcess
{
public:
    SuspendedChildProcess()
    {
        std::wstring systemDirectory(MAX_PATH, L'\0');
        systemDirectory.resize(::GetSystemDirectoryW(systemDirectory.data(), MAX_PATH));
        m_imagePath = systemDirectory + L"\\cmd.exe";
        m_commandLine =
            L"\"" + m_imagePath + L"\" /c exit sysmon-process-tracking-" + std::to_wstring(::GetCurrentProcessId());

        STARTUPINFOW startupInfo{.cb = sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = m_commandLine;
        if (::CreateProcessW(m_imagePath.c_str(), mutableCommandLine.data(), nullptr, nullptr, FALSE,
                             CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo)) {
            m_process.reset(processInfo.hProcess);
            m_thread.reset(processInfo.hThread);
            m_identity =
                ProcessIdentity{.pid = processInfo.dwProcessId, .creationTime = creationTimeOf(m_process.get())};
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

    [[nodiscard]] const ProcessIdentity& identity() const
    {
        return m_identity;
    }

    [[nodiscard]] std::string imagePath() const
    {
        return wideToUtf8(m_imagePath);
    }

    [[nodiscard]] std::string commandLine() const
    {
        return wideToUtf8(m_commandLine);
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
    ScopedHandle m_process;
    ScopedHandle m_thread;
    ProcessIdentity m_identity;
};

std::string lowercaseAscii(std::string text)
{
    std::ranges::transform(text, text.begin(), [](char character) {
        return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a') : character;
    });
    return text;
}

const ProcessInfo* findIdentity(const std::vector<ProcessInfo>& processes, const ProcessIdentity& identity)
{
    const auto it = std::ranges::find_if(
        processes, [&identity](const ProcessInfo& process) { return process.identity() == identity; });
    return it != processes.end() ? &*it : nullptr;
}

std::optional<std::vector<ProcessInfo>> readProcessList(SystemProcessInformationReader& reader)
{
    const auto bytes = reader.read();
    if (!bytes) {
        return std::nullopt;
    }
    return parseSystemProcessInformation(*bytes);
}

bool isElevated()
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    const ScopedHandle scopedToken(token);
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    if (!::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

} // namespace

TEST(ProcessTrackingIntegration, RealHost_NewChildProcess_ListedWithIdentityAndDetails)
{
    ProcessCollector collector;
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* listed = findIdentity(*processes, child.identity());
    ASSERT_NE(listed, nullptr);
    EXPECT_EQ(listed->parentPid, ::GetCurrentProcessId());
    EXPECT_EQ(listed->imageName, "cmd.exe");
    EXPECT_EQ(listed->detailsStatus, ProcessDetailsStatus::Available);
    ASSERT_TRUE(listed->imagePath.has_value());
    // Windows paths ignore case, and GetSystemDirectoryW may return "C:\WINDOWS\system32".
    EXPECT_EQ(lowercaseAscii(*listed->imagePath), lowercaseAscii(child.imagePath()));
    EXPECT_EQ(listed->commandLine, child.commandLine());
    EXPECT_GE(listed->threadCount, 1u);
}

TEST(ProcessTrackingIntegration, RealHost_ChildProcessExits_DisappearsFromList)
{
    ProcessCollector collector;
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());
    const auto before = collector.collect();
    ASSERT_TRUE(before.has_value());
    ASSERT_NE(findIdentity(*before, child.identity()), nullptr);

    child.terminate();

    // Another process (an antivirus scanner, say) may hold a handle to the
    // exited child for a moment, which keeps it listed until it closes.
    bool isGone = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!isGone && std::chrono::steady_clock::now() < deadline) {
        const auto after = collector.collect();
        ASSERT_TRUE(after.has_value());
        isGone = findIdentity(*after, child.identity()) == nullptr;
        if (!isGone) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    }
    EXPECT_TRUE(isGone);
}

TEST(ProcessTrackingIntegration, RealHost_SecondCollect_ReadsDetailsOnlyForNewProcesses)
{
    SystemProcessInformationReader listReader;
    const ProcessDetailsReader windowsReader = makeWindowsProcessDetailsReader();
    std::set<IdentityKey> reads;
    ProcessCollector collector([&listReader] { return readProcessList(listReader); },
                               [&reads, &windowsReader](const ProcessInfo& process) {
                                   reads.insert(keyOf(process.identity()));
                                   return windowsReader(process);
                               });

    const auto first = collector.collect();
    ASSERT_TRUE(first.has_value());
    std::set<IdentityKey> firstIdentities;
    std::set<IdentityKey> expectedFirstReads;
    for (const auto& process : *first) {
        firstIdentities.insert(keyOf(process.identity()));
        if (!isNeverOpenedProcess(process.imageName)) {
            expectedFirstReads.insert(keyOf(process.identity()));
        }
    }
    EXPECT_EQ(reads, expectedFirstReads);

    // Guarantees at least one process the first list did not have.
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());
    reads.clear();
    const auto second = collector.collect();

    ASSERT_TRUE(second.has_value());
    std::set<IdentityKey> expectedSecondReads;
    for (const auto& process : *second) {
        const IdentityKey key = keyOf(process.identity());
        if (!firstIdentities.contains(key) && !isNeverOpenedProcess(process.imageName)) {
            expectedSecondReads.insert(key);
        }
    }
    EXPECT_EQ(reads, expectedSecondReads);
    EXPECT_TRUE(reads.contains(keyOf(child.identity())));
}

TEST(ProcessTrackingIntegration, RealHost_DetailsStatus_MatchesDetailsRead)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    for (const auto& process : *processes) {
        const bool hasBoth = process.imagePath.has_value() && process.commandLine.has_value();
        switch (process.detailsStatus) {
            case ProcessDetailsStatus::Available:
                EXPECT_TRUE(hasBoth) << process.imageName << " (PID " << process.pid << ")";
                break;
            case ProcessDetailsStatus::NotQueried:
                EXPECT_TRUE(isNeverOpenedProcess(process.imageName)) << process.imageName;
                EXPECT_FALSE(process.imagePath.has_value() || process.commandLine.has_value()) << process.imageName;
                break;
            case ProcessDetailsStatus::AccessDenied:
            case ProcessDetailsStatus::Unavailable:
                EXPECT_FALSE(hasBoth) << process.imageName << " (PID " << process.pid << ")";
                break;
        }
    }
}

TEST(ProcessTrackingIntegration, RealHost_Unelevated_DeniedProcessesStayListedWithTheirMetrics)
{
    if (isElevated()) {
        GTEST_SKIP() << "Running elevated; processes may not deny access";
    }
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    int deniedCount = 0;
    for (const auto& process : *processes) {
        if (process.detailsStatus != ProcessDetailsStatus::AccessDenied) {
            continue;
        }
        ++deniedCount;
        // Everything from the process list is still there.
        EXPECT_FALSE(process.imageName.empty()) << "PID " << process.pid;
        EXPECT_GT(process.creationTime, fileTimeToSystemClock(0)) << process.imageName;
        EXPECT_GT(process.workingSetBytes + process.privateBytes, 0u) << process.imageName;
    }
    // Services running as SYSTEM refuse an unelevated caller.
    EXPECT_GE(deniedCount, 1);
}

TEST(ProcessTrackingIntegration, RealHost_SchedulerWithRealCollector_SnapshotsCarryProcessList)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setSlowInterval(std::chrono::milliseconds{200});
    scheduler.setProcessCollector(std::make_unique<ProcessCollector>());

    std::mutex mutex;
    std::optional<SystemSnapshot> snapshotWithProcesses;
    std::atomic<bool> found{false};
    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, [&](const SystemSnapshot& snapshot) {
        if (snapshot.processes.has_value() && !found.load()) {
            std::lock_guard lock(mutex);
            snapshotWithProcesses = snapshot;
            found = true;
        }
    });

    scheduler.start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!found.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    scheduler.stop();

    std::lock_guard lock(mutex);
    ASSERT_TRUE(snapshotWithProcesses.has_value());
    const auto& processes = *snapshotWithProcesses->processes;
    const auto self = std::ranges::find(processes, static_cast<uint32_t>(::GetCurrentProcessId()), &ProcessInfo::pid);
    ASSERT_NE(self, processes.end());
    EXPECT_EQ(self->detailsStatus, ProcessDetailsStatus::Available);
}
