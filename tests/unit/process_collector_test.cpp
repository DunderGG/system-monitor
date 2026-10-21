#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_collector.h"

using namespace verdandi::domain;
using namespace verdandi::platform;

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

    static_cast<void>(collector.collect());
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

TEST(ProcessCollector, CalculateProcessRates_FirstSighting_NoUsageAndBaselineRecorded)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> processes{withCpuTimes(100, 1, 300, 200)};

    calculateProcessRates(processes, baselines, kStart, 4);

    EXPECT_FALSE(processes[0].cpuUsagePercent.has_value());
    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->second.cpuTimeMs, 500u);
    EXPECT_EQ(baselines.begin()->second.timestamp, kStart);
}

TEST(ProcessCollector, CalculateProcessRates_SecondRefresh_ShareOfAllProcessors)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(first, baselines, kStart, 2);

    // 600 ms user + 400 ms kernel over 2 s on 2 processors (4000 ms capacity).
    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 900, 600)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    ASSERT_TRUE(second[0].cpuUsagePercent.has_value());
    EXPECT_FLOAT_EQ(*second[0].cpuUsagePercent, 25.0f);
}

TEST(ProcessCollector, CalculateProcessRates_IdleProcess_ZeroNotMissing)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_EQ(second[0].cpuUsagePercent, 0.0f);
}

TEST(ProcessCollector, CalculateProcessRates_PidReusedWithNewCreationTime_StartsOver)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 50, 900, 600)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->first.creationTime, second[0].creationTime);
}

TEST(ProcessCollector, CalculateProcessRates_ProcessExited_BaselineDropped)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0), withCpuTimes(200, 2, 0, 0)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(200, 2, 10, 0)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->first.pid, 200u);
}

TEST(ProcessCollector, CalculateProcessRates_NoElapsedTime_NoUsage)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 400, 200)};
    calculateProcessRates(second, baselines, kStart, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

TEST(ProcessCollector, CalculateProcessRates_CpuTimeWentBackwards_NoUsage)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 300, 200)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 100, 200)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

TEST(ProcessCollector, CalculateProcessRates_MoreThanCapacity_ClampedTo100)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0)};
    calculateProcessRates(first, baselines, kStart, 1);

    // Tick-granular accounting can credit slightly more than the elapsed time.
    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 1016, 0)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{1}, 1);

    EXPECT_EQ(second[0].cpuUsagePercent, 100.0f);
}

TEST(ProcessCollector, CalculateProcessRates_NoProcessorCount_NoUsage)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withCpuTimes(100, 1, 0, 0)};
    calculateProcessRates(first, baselines, kStart, 0);

    std::vector<ProcessInfo> second{withCpuTimes(100, 1, 500, 0)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{1}, 0);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
}

namespace
{

ProcessInfo withIoBytes(uint32_t pid, uint64_t readBytes, uint64_t writeBytes)
{
    return ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{1}},
        .ioReadBytes = readBytes,
        .ioWriteBytes = writeBytes,
    };
}

} // namespace

TEST(ProcessCollector, CalculateProcessRates_FirstSighting_NoIoRatesAndBaselineRecorded)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> processes{withIoBytes(100, 4096, 1024)};

    calculateProcessRates(processes, baselines, kStart, 2);

    EXPECT_FALSE(processes[0].ioReadBytesPerSec.has_value());
    EXPECT_FALSE(processes[0].ioWriteBytesPerSec.has_value());
    ASSERT_EQ(baselines.size(), 1u);
    EXPECT_EQ(baselines.begin()->second.ioReadBytes, 4096u);
    EXPECT_EQ(baselines.begin()->second.ioWriteBytes, 1024u);
}

TEST(ProcessCollector, CalculateProcessRates_SecondRefresh_IoBytesPerSecond)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withIoBytes(100, 1000, 0)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withIoBytes(100, 5000, 1001)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_EQ(second[0].ioReadBytesPerSec, 2000u);
    // 500.5 bytes per second rounds to the nearest byte.
    EXPECT_EQ(second[0].ioWriteBytesPerSec, 501u);
}

TEST(ProcessCollector, CalculateProcessRates_IdleIo_ZeroNotMissing)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withIoBytes(100, 1000, 500)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withIoBytes(100, 1000, 500)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_EQ(second[0].ioReadBytesPerSec, 0u);
    EXPECT_EQ(second[0].ioWriteBytesPerSec, 0u);
}

TEST(ProcessCollector, CalculateProcessRates_IoCounterWentBackwards_NoRateForThatCounterOnly)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withIoBytes(100, 1000, 500)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withIoBytes(100, 400, 2500)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{2}, 2);

    EXPECT_FALSE(second[0].ioReadBytesPerSec.has_value());
    EXPECT_EQ(second[0].ioWriteBytesPerSec, 1000u);
}

TEST(ProcessCollector, CalculateProcessRates_NoElapsedTime_NoIoRates)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withIoBytes(100, 1000, 500)};
    calculateProcessRates(first, baselines, kStart, 2);

    std::vector<ProcessInfo> second{withIoBytes(100, 2000, 600)};
    calculateProcessRates(second, baselines, kStart, 2);

    EXPECT_FALSE(second[0].ioReadBytesPerSec.has_value());
    EXPECT_FALSE(second[0].ioWriteBytesPerSec.has_value());
}

TEST(ProcessCollector, CalculateProcessRates_NoProcessorCount_IoRatesStillSet)
{
    ProcessRateBaselines baselines;
    std::vector<ProcessInfo> first{withIoBytes(100, 0, 0)};
    calculateProcessRates(first, baselines, kStart, 0);

    std::vector<ProcessInfo> second{withIoBytes(100, 300, 100)};
    calculateProcessRates(second, baselines, kStart + std::chrono::seconds{1}, 0);

    EXPECT_FALSE(second[0].cpuUsagePercent.has_value());
    EXPECT_EQ(second[0].ioReadBytesPerSec, 300u);
    EXPECT_EQ(second[0].ioWriteBytesPerSec, 100u);
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

TEST(ProcessCollector, ResetBaselines_NextCollectHasNoRatesAndTheOneAfterMeasuresFromIt)
{
    uint64_t userMs = 0;
    auto now = kStart;
    ProcessCollector collector([&] { return std::vector<ProcessInfo>{withCpuTimes(100, 1, userMs, 0)}; }, nullptr,
                               [&now] { return now; }, 1);
    const auto first = collector.collect();

    // CPU time before the reset, as before a sleep; no rate may include it.
    now += std::chrono::hours{8};
    userMs += 2000;
    collector.resetBaselines();
    const auto afterReset = collector.collect();

    now += std::chrono::seconds{2};
    userMs += 500;
    const auto next = collector.collect();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(afterReset.has_value());
    ASSERT_TRUE(next.has_value());
    EXPECT_FALSE((*afterReset)[0].cpuUsagePercent.has_value());
    // 500 ms of CPU time over the 2 s since the reset, on one processor.
    EXPECT_EQ((*next)[0].cpuUsagePercent, 25.0f);
}
