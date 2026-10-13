#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(ProcessCollector, Collect_ListRead_ReturnsProcesses)
{
    ProcessCollector collector([] {
        return std::vector<ProcessInfo>{
            ProcessInfo{.pid = 4, .imageName = "System"},
            ProcessInfo{.pid = 1234, .parentPid = 4, .imageName = "app.exe"},
        };
    });

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 2u);
    EXPECT_EQ((*processes)[1].pid, 1234u);
    EXPECT_EQ((*processes)[1].imageName, "app.exe");
}

TEST(ProcessCollector, Collect_WithDetailsReader_ReadsEachProcessOnce)
{
    int detailReads = 0;
    ProcessCollector collector(
        [] {
            return std::vector<ProcessInfo>{
                ProcessInfo{.pid = 1234, .imageName = "app.exe"},
                ProcessInfo{.pid = 5678, .imageName = "tool.exe"},
            };
        },
        [&detailReads](const ProcessInfo& process) {
            ++detailReads;
            return ProcessDetails{
                .imagePath = "C:\\Apps\\" + process.imageName,
                .commandLine = process.imageName,
                .status = ProcessDetailsStatus::Available,
            };
        });

    (void)collector.collect();
    const auto processes = collector.collect();

    EXPECT_EQ(detailReads, 2);
    ASSERT_TRUE(processes.has_value());
    EXPECT_EQ((*processes)[1].imagePath, "C:\\Apps\\tool.exe");
    EXPECT_EQ((*processes)[1].detailsStatus, ProcessDetailsStatus::Available);
}

TEST(ProcessCollector, Collect_LsassAndCsrss_NeverPassedToDetailsReader)
{
    std::vector<std::string> readNames;
    ProcessCollector collector(
        [] {
            return std::vector<ProcessInfo>{
                ProcessInfo{.pid = 700, .imageName = "csrss.exe"},
                ProcessInfo{.pid = 900, .imageName = "lsass.exe"},
                ProcessInfo{.pid = 1234, .imageName = "app.exe"},
            };
        },
        [&readNames](const ProcessInfo& process) {
            readNames.push_back(process.imageName);
            return ProcessDetails{.imagePath = "C:\\Apps\\" + process.imageName,
                                  .commandLine = process.imageName,
                                  .status = ProcessDetailsStatus::Available};
        });

    const auto processes = collector.collect();

    EXPECT_EQ(readNames, std::vector<std::string>{"app.exe"});
    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 3u);
    for (std::size_t i = 0; i < 2; ++i) {
        EXPECT_EQ((*processes)[i].detailsStatus, ProcessDetailsStatus::NotQueried);
        EXPECT_FALSE((*processes)[i].imagePath.has_value());
    }
    EXPECT_EQ((*processes)[2].detailsStatus, ProcessDetailsStatus::Available);
}

TEST(ProcessCollector, Collect_NoDetailsReader_DetailsNotQueried)
{
    ProcessCollector collector([] { return std::vector<ProcessInfo>{ProcessInfo{.pid = 1234}}; });

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    EXPECT_FALSE((*processes)[0].imagePath.has_value());
    EXPECT_EQ((*processes)[0].detailsStatus, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessCollector, Collect_ListReadFails_ReturnsNullopt)
{
    ProcessCollector collector([]() -> std::optional<std::vector<ProcessInfo>> { return std::nullopt; });

    EXPECT_FALSE(collector.collect().has_value());
}

TEST(ProcessCollector, Collect_NoReader_ReturnsNullopt)
{
    ProcessCollector collector(nullptr);

    EXPECT_FALSE(collector.collect().has_value());
}

namespace
{

const auto kStart = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

ProcessInfo withCpuTimes(uint32_t pid, int createdSeconds, uint64_t userMs, uint64_t kernelMs)
{
    return ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{createdSeconds}},
        .cpuUserTimeMs = userMs,
        .cpuKernelTimeMs = kernelMs,
    };
}

} // namespace

TEST(ProcessCollector, CalculateProcessCpuUsage_FirstSighting_NoUsageAndBaselineRecorded)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> processes{withCpuTimes(100, 1, 300, 200)};

    calculateProcessCpuUsage(processes, baselines, kStart, 4);

    EXPECT_FALSE(processes[0].cpuUsagePercent.has_value());
    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->second.cpuTimeMs, 500u);
    EXPECT_EQ(baselines.begin()->second.timestamp, kStart);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_SecondRefresh_ShareOfAllProcessors)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    // 600 ms user + 400 ms kernel over 2 s on 2 processors (4000 ms capacity).
    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 900, 600)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{2}, 2);

    ASSERT_TRUE(second[0].cpuUsagePercent.has_value());
    EXPECT_FLOAT_EQ(*second[0].cpuUsagePercent, 25.0f);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_IdleProcess_ZeroNotMissing)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_EQ(second[0].cpuUsagePercent, 0.0f);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_PidReusedWithNewCreationTime_StartsOver)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 50, 900, 600)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->first.creationTime, second[0].creationTime);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_ProcessExited_BaselineDropped)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0), withCpuTimes(200, 2, 0, 0)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(200, 2, 10, 0)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{2}, 2);

    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->first.pid, 200u);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_NoElapsedTime_NoUsage)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 400, 200)};
    calculateProcessCpuUsage(second, baselines, kStart, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

TEST(ProcessCollector, CalculateProcessCpuUsage_CpuTimeWentBackwards_NoUsage)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessCpuUsage(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 100, 200)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

TEST(ProcessCollector, CalculateProcessCpuUsage_MoreThanCapacity_ClampedTo100)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0)};
    calculateProcessCpuUsage(first, baselines, kStart, 1);

    // Tick-granular accounting can credit slightly more than the elapsed time.
    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 1016, 0)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{1}, 1);

    EXPECT_EQ(second[0].cpuUsagePercent, 100.0f);
}

TEST(ProcessCollector, CalculateProcessCpuUsage_NoProcessorCount_NoUsage)
{
    ProcessCpuBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0)};
    calculateProcessCpuUsage(first, baselines, kStart, 0);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 500, 0)};
    calculateProcessCpuUsage(second, baselines, kStart + std::chrono::seconds{1}, 0);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

TEST(ProcessCollector, Collect_FailedReadBetweenRefreshes_MeasuresOverLongerInterval)
{
    std::vector<std::optional<std::vector<ProcessInfo>>> reads{
        std::vector<ProcessInfo>{withCpuTimes(100, 1, 0, 0)},
        std::nullopt,
        std::vector<ProcessInfo>{withCpuTimes(100, 1, 2000, 0)},
    };
    std::size_t readIndex = 0;
    auto now = kStart;
    ProcessCollector collector([&] { return reads[readIndex++]; }, nullptr, [&now] { return now; }, 1);

    const auto first = collector.collect();
    now += std::chrono::seconds{2};
    const auto failed = collector.collect();
    now += std::chrono::seconds{2};
    const auto third = collector.collect();

    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE((*first)[0].cpuUsagePercent.has_value());
    EXPECT_FALSE(failed.has_value());
    ASSERT_TRUE(third.has_value());
    // 2000 ms of CPU time over the 4 s since the first read, on one processor.
    EXPECT_EQ((*third)[0].cpuUsagePercent, 50.0f);
}
