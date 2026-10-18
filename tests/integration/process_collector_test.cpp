#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/current_process.h"
#include "platform/windows/process_collector.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/utf8_conversion.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

const ProcessInfo* findPid(const std::vector<ProcessInfo>& processes, uint32_t pid)
{
    const auto it = std::ranges::find(processes, pid, &ProcessInfo::pid);
    return it != processes.end() ? &*it : nullptr;
}

std::string thisExecutableName()
{
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return wideToUtf8(std::filesystem::path(path).filename().wstring());
}

int64_t thisProcessCreationFileTime()
{
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!::GetProcessTimes(::GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        return 0;
    }
    return static_cast<int64_t>((static_cast<uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime);
}

} // namespace

TEST(ProcessCollectorIntegration, RealHost_ListsThisProcessWithItsIdentityAndCounters)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* self = findPid(*processes, ::GetCurrentProcessId());
    ASSERT_NE(self, nullptr);
    EXPECT_EQ(self->imageName, thisExecutableName());
    EXPECT_EQ(self->creationTime, fileTimeToSystemClock(thisProcessCreationFileTime()));
    EXPECT_LE(self->creationTime, std::chrono::system_clock::now());
    EXPECT_GT(self->creationTime, std::chrono::system_clock::now() - std::chrono::hours{24});
    EXPECT_NE(self->parentPid, self->pid);
    EXPECT_GE(self->threadCount, 1u);
    EXPECT_GE(self->handleCount, 1u);
    EXPECT_GT(self->workingSetBytes, 0u);
    EXPECT_GT(self->privateWorkingSetBytes, 0u);
    // The private working set is part of the working set.
    EXPECT_LE(self->privateWorkingSetBytes, self->workingSetBytes);
    EXPECT_GT(self->privateBytes, 0u);
}

TEST(ProcessCollectorIntegration, RealHost_CurrentProcessIdentity_MatchesThisProcessInTheList)
{
    ProcessCollector collector;

    const auto identity = currentProcessIdentity();
    const auto processes = collector.collect();

    ASSERT_TRUE(identity.has_value());
    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* self = findPid(*processes, ::GetCurrentProcessId());
    ASSERT_NE(self, nullptr);
    EXPECT_EQ(self->identity(), *identity);
}

TEST(ProcessCollectorIntegration, RealHost_ThisProcess_HasPathAndCommandLine)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* self = findPid(*processes, ::GetCurrentProcessId());
    ASSERT_NE(self, nullptr);
    EXPECT_EQ(self->detailsStatus, ProcessDetailsStatus::Available);
    ASSERT_TRUE(self->imagePath.has_value());
    EXPECT_TRUE(self->imagePath->ends_with(thisExecutableName()));
    EXPECT_EQ(self->commandLine, wideToUtf8(::GetCommandLineW()));
}

TEST(ProcessCollectorIntegration, RealHost_CsrssAndLsass_NotQueried)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    int csrssCount = 0;
    for (const auto& process : *processes) {
        if (process.imageName == "csrss.exe" || process.imageName == "lsass.exe") {
            csrssCount += process.imageName == "csrss.exe" ? 1 : 0;
            EXPECT_EQ(process.detailsStatus, ProcessDetailsStatus::NotQueried) << process.imageName;
            EXPECT_FALSE(process.imagePath.has_value()) << process.imageName;
            EXPECT_GE(process.threadCount, 1u) << process.imageName;
        }
    }
    // Every Windows session has a csrss.exe.
    EXPECT_GE(csrssCount, 1);
}

TEST(ProcessCollectorIntegration, RealHost_BusyThread_ReportsCpuUsageForThisProcess)
{
    ProcessCollector collector;
    const auto first = collector.collect();
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(
        std::ranges::none_of(*first, [](const ProcessInfo& process) { return process.cpuUsagePercent.has_value(); }));

    // Keep this thread busy so the process accumulates CPU time.
    const auto busyUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds{300};
    while (std::chrono::steady_clock::now() < busyUntil) {
    }
    const auto second = collector.collect();

    ASSERT_TRUE(second.has_value());
    const ProcessInfo* self = findPid(*second, ::GetCurrentProcessId());
    ASSERT_NE(self, nullptr);
    ASSERT_TRUE(self->cpuUsagePercent.has_value());
    EXPECT_GT(*self->cpuUsagePercent, 0.0f);
    for (const auto& process : *second) {
        if (process.cpuUsagePercent) {
            EXPECT_GE(*process.cpuUsagePercent, 0.0f) << process.imageName;
            EXPECT_LE(*process.cpuUsagePercent, 100.0f) << process.imageName;
        }
    }
}

TEST(ProcessCollectorIntegration, RealHost_FileWritten_ReportsIoWriteRateForThisProcess)
{
    ProcessCollector collector;
    const auto first = collector.collect();
    ASSERT_TRUE(first.has_value());

    // Write 1 MiB to a file so the process's write transfer count grows.
    const auto path =
        std::filesystem::temp_directory_path() / ("sysmon_io_rate_test_" + std::to_string(::GetCurrentProcessId()));
    {
        std::ofstream file(path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        const std::string block(1024 * 1024, 'x');
        file.write(block.data(), static_cast<std::streamsize>(block.size()));
    }
    std::filesystem::remove(path);
    const auto second = collector.collect();

    ASSERT_TRUE(second.has_value());
    const ProcessInfo* self = findPid(*second, ::GetCurrentProcessId());
    ASSERT_NE(self, nullptr);
    ASSERT_TRUE(self->ioWriteBytesPerSec.has_value());
    EXPECT_GT(*self->ioWriteBytesPerSec, 0u);
    EXPECT_TRUE(self->ioReadBytesPerSec.has_value());
}

TEST(ProcessCollectorIntegration, RealHost_ListsIdleAndSystemProcesses)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* idle = findPid(*processes, 0);
    ASSERT_NE(idle, nullptr);
    EXPECT_TRUE(idle->imageName.empty());
    const ProcessInfo* system = findPid(*processes, 4);
    ASSERT_NE(system, nullptr);
    EXPECT_EQ(system->imageName, "System");
}

TEST(ProcessCollectorIntegration, RealHost_PidsAreUnique)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    std::set<uint32_t> pids;
    for (const auto& process : *processes) {
        EXPECT_TRUE(pids.insert(process.pid).second) << "duplicate PID " << process.pid;
    }
    EXPECT_GE(pids.size(), 3u);
}

TEST(ProcessCollectorIntegration, RealHost_PrivateWorkingSets_AddUpToLessThanPhysicalMemory)
{
    ProcessCollector collector;
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(MEMORYSTATUSEX);
    ASSERT_TRUE(::GlobalMemoryStatusEx(&memory));

    const auto processes = collector.collect();

    // No page is in two processes' private working sets, so unlike the full
    // working sets, which count a shared page once per process, they fit in
    // physical memory. The Memory column shows them for that reason (DC-79).
    ASSERT_TRUE(processes.has_value());
    uint64_t privateTotal = 0;
    for (const auto& process : *processes) {
        privateTotal += process.privateWorkingSetBytes;
    }
    EXPECT_GT(privateTotal, 0u);
    EXPECT_LT(privateTotal, memory.ullTotalPhys);
}
