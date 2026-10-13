#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "domain/process_info.h"
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
    EXPECT_GT(self->privateBytes, 0u);
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
