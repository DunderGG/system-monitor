#include <chrono>
#include <cstdint>

#include <gtest/gtest.h>

#include "domain/cpu_sample.h"
#include "platform/windows/cpu_collector.h"

using namespace sysmon::platform;

TEST(CpuCollector, CalculateCpuUsage_ZeroElapsed_ReturnsZero)
{
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1100, .kernelTime = 2200, .userTime = 600};

    const float usage = calculateCpuUsage(previous, current, std::chrono::nanoseconds{0});
    EXPECT_FLOAT_EQ(usage, 0.0f);

    const float negativeUsage = calculateCpuUsage(previous, current, std::chrono::nanoseconds{-100});
    EXPECT_FLOAT_EQ(negativeUsage, 0.0f);
}

TEST(CpuCollector, CalculateCpuUsage_ZeroDeltaTotal_ReturnsZero)
{
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 0.0f);
}

TEST(CpuCollector, CalculateCpuUsage_FullyIdle_ReturnsZero)
{
    // In Windows, kernelTime includes idleTime.
    // 100 ticks elapsed in idle mode: deltaKernel = 100, deltaIdle = 100, deltaUser = 0.
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1100, .kernelTime = 2100, .userTime = 500};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 0.0f);
}

TEST(CpuCollector, CalculateCpuUsage_FullyBusyUser_Returns100)
{
    // 100 ticks elapsed purely in user mode: deltaKernel = 0, deltaIdle = 0, deltaUser = 100.
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1000, .kernelTime = 2000, .userTime = 600};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 100.0f);
}

TEST(CpuCollector, CalculateCpuUsage_FullyBusyKernel_Returns100)
{
    // 100 ticks elapsed purely in active kernel mode (non-idle): deltaKernel = 100, deltaIdle = 0, deltaUser = 0.
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1000, .kernelTime = 2100, .userTime = 500};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 100.0f);
}

TEST(CpuCollector, CalculateCpuUsage_FiftyPercentBusy_Returns50)
{
    // deltaKernel = 75 (50 idle + 25 busy), deltaIdle = 50, deltaUser = 25.
    // deltaTotal = 75 + 25 = 100. deltaBusy = 100 - 50 = 50. Usage = 50.0%.
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1050, .kernelTime = 2075, .userTime = 525};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 50.0f);
}

TEST(CpuCollector, CalculateCpuUsage_IdleExceedsKernelDueToJitter_ClampsToZero)
{
    // Clock jitter or multi-core unsynchronized read: deltaIdle appears slightly greater than deltaTotal.
    const SystemTimesData previous{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    const SystemTimesData current{.idleTime = 1110, .kernelTime = 2100, .userTime = 500};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 0.0f);
}

TEST(CpuCollector, CalculateCpuUsage_CounterRolloverOrUnderflow_HandledGracefully)
{
    // Current times smaller than previous (e.g. system reset): should safely produce 0.0f.
    const SystemTimesData previous{.idleTime = 5000, .kernelTime = 8000, .userTime = 3000};
    const SystemTimesData current{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};

    const float usage = calculateCpuUsage(previous, current, std::chrono::milliseconds{1000});
    EXPECT_FLOAT_EQ(usage, 0.0f);
}

TEST(CpuCollector, Collect_InjectedReaders_CalculatesUsageAcrossTicks)
{
    SystemTimesData simulatedTimes{.idleTime = 10'000, .kernelTime = 20'000, .userTime = 5'000};
    std::chrono::steady_clock::time_point simulatedNow{std::chrono::milliseconds{1000}};

    auto timesReader = [&simulatedTimes](SystemTimesData &out) {
        out = simulatedTimes;
        return true;
    };
    auto clockReader = [&simulatedNow]() {
        return simulatedNow;
    };

    CpuCollector collector(timesReader, clockReader, 8);
    EXPECT_EQ(collector.coreCount(), 8);

    // Initial collection immediately after construction with 0 elapsed time
    const auto initialSample = collector.collect();
    EXPECT_FLOAT_EQ(initialSample.totalUsagePercent, 0.0f);
    EXPECT_EQ(initialSample.coreCount, 8);
    EXPECT_TRUE(initialSample.coreUsagePercents.empty());

    // Advance by 1 second: 50% busy
    // deltaKernel = 750, deltaIdle = 500, deltaUser = 250 -> Total = 1000, Busy = 500
    simulatedNow += std::chrono::milliseconds{1000};
    simulatedTimes.idleTime += 500;
    simulatedTimes.kernelTime += 750;
    simulatedTimes.userTime += 250;

    const auto sample1 = collector.collect();
    EXPECT_FLOAT_EQ(sample1.totalUsagePercent, 50.0f);
    EXPECT_EQ(sample1.coreCount, 8);

    // Advance by another second: 100% busy in user mode
    // deltaKernel = 0, deltaIdle = 0, deltaUser = 1000
    simulatedNow += std::chrono::milliseconds{1000};
    simulatedTimes.userTime += 1000;

    const auto sample2 = collector.collect();
    EXPECT_FLOAT_EQ(sample2.totalUsagePercent, 100.0f);
    EXPECT_EQ(sample2.coreCount, 8);
}

TEST(CpuCollector, Collect_ReaderFails_ReturnsZeroUsageGracefully)
{
    bool shouldSucceed = true;
    SystemTimesData simulatedTimes{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    auto clock = std::chrono::steady_clock::now();

    auto timesReader = [&](SystemTimesData &out) {
        if (!shouldSucceed) {
            return false;
        }
        out = simulatedTimes;
        return true;
    };
    auto clockReader = [&]() { return clock; };

    CpuCollector collector(timesReader, clockReader, 4);

    // Initial collect succeeds
    const auto initialSample = collector.collect();
    EXPECT_FLOAT_EQ(initialSample.totalUsagePercent, 0.0f);

    // Reader fails on next tick
    shouldSucceed = false;
    clock += std::chrono::milliseconds{1000};
    const auto sample = collector.collect();

    EXPECT_FLOAT_EQ(sample.totalUsagePercent, 0.0f);
    EXPECT_EQ(sample.coreCount, 4);
}

TEST(CpuCollector, Collect_PopulatesCoreUsagePercents_UsingInjectedCoreReader)
{
    SystemTimesData simulatedTimes{.idleTime = 10'000, .kernelTime = 20'000, .userTime = 5'000};
    std::vector<SystemTimesData> simulatedCoreTimes = {
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
    };
    std::chrono::steady_clock::time_point simulatedNow{std::chrono::milliseconds{1000}};

    auto timesReader = [&](SystemTimesData &out) {
        out = simulatedTimes;
        return true;
    };
    auto coreReader = [&](std::vector<SystemTimesData> &out) {
        out = simulatedCoreTimes;
        return true;
    };
    auto clockReader = [&]() {
        return simulatedNow;
    };

    CpuCollector collector(timesReader, coreReader, clockReader, 4);

    // Initial collection immediately after construction
    const auto initialSample = collector.collect();
    EXPECT_FLOAT_EQ(initialSample.totalUsagePercent, 0.0f);
    ASSERT_EQ(initialSample.coreUsagePercents.size(), 4u);
    for (float usage : initialSample.coreUsagePercents) {
        EXPECT_FLOAT_EQ(usage, 0.0f);
    }

    // Advance by 1 second with different load per core
    simulatedNow += std::chrono::milliseconds{1000};
    simulatedTimes.idleTime += 200;
    simulatedTimes.kernelTime += 300;
    simulatedTimes.userTime += 100;

    // Core 0: 0% busy (100% idle)
    simulatedCoreTimes[0].idleTime += 100;
    simulatedCoreTimes[0].kernelTime += 100;

    // Core 1: 50% busy (deltaTotal = 100, deltaBusy = 50)
    simulatedCoreTimes[1].idleTime += 50;
    simulatedCoreTimes[1].kernelTime += 75;
    simulatedCoreTimes[1].userTime += 25;

    // Core 2: 100% busy user (deltaTotal = 100, deltaBusy = 100)
    simulatedCoreTimes[2].userTime += 100;

    // Core 3: 100% busy kernel (deltaTotal = 100, deltaBusy = 100)
    simulatedCoreTimes[3].kernelTime += 100;

    const auto sample = collector.collect();
    ASSERT_EQ(sample.coreUsagePercents.size(), 4u);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[0], 0.0f);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[1], 50.0f);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[2], 100.0f);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[3], 100.0f);
}

TEST(CpuCollector, Collect_CoreReaderFails_GracefullyRetainsEmptyCoreUsages)
{
    SystemTimesData simulatedTimes{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    auto clock = std::chrono::steady_clock::now();

    auto timesReader = [&](SystemTimesData &out) {
        out = simulatedTimes;
        return true;
    };
    auto coreReader = [](std::vector<SystemTimesData> &) {
        return false;
    };
    auto clockReader = [&]() { return clock; };

    CpuCollector collector(timesReader, coreReader, clockReader, 4);

    const auto sample1 = collector.collect();
    EXPECT_FLOAT_EQ(sample1.totalUsagePercent, 0.0f);
    EXPECT_TRUE(sample1.coreUsagePercents.empty());

    // Advance time and times
    clock += std::chrono::milliseconds{1000};
    simulatedTimes.idleTime += 500;
    simulatedTimes.kernelTime += 750;
    simulatedTimes.userTime += 250;

    const auto sample2 = collector.collect();
    EXPECT_FLOAT_EQ(sample2.totalUsagePercent, 50.0f);
    EXPECT_TRUE(sample2.coreUsagePercents.empty());
}

TEST(CpuCollector, Collect_IndividualCoreJitter_ClampedToZero)
{
    SystemTimesData simulatedTimes{.idleTime = 1000, .kernelTime = 2000, .userTime = 500};
    std::vector<SystemTimesData> simulatedCoreTimes = {
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
        SystemTimesData{.idleTime = 1000, .kernelTime = 2000, .userTime = 500},
    };
    auto clock = std::chrono::steady_clock::now();

    auto timesReader = [&](SystemTimesData &out) {
        out = simulatedTimes;
        return true;
    };
    auto coreReader = [&](std::vector<SystemTimesData> &out) {
        out = simulatedCoreTimes;
        return true;
    };
    auto clockReader = [&]() { return clock; };

    CpuCollector collector(timesReader, coreReader, clockReader, 2);
    const auto initialSample = collector.collect();
    (void)initialSample;

    clock += std::chrono::milliseconds{1000};
    simulatedTimes.kernelTime += 100;

    // Core 0 has jitter where deltaIdle > deltaTotal
    simulatedCoreTimes[0].idleTime += 120;
    simulatedCoreTimes[0].kernelTime += 100;

    // Core 1 has normal 50%
    simulatedCoreTimes[1].idleTime += 50;
    simulatedCoreTimes[1].kernelTime += 75;
    simulatedCoreTimes[1].userTime += 25;

    const auto sample = collector.collect();
    ASSERT_EQ(sample.coreUsagePercents.size(), 2u);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[0], 0.0f);
    EXPECT_FLOAT_EQ(sample.coreUsagePercents[1], 50.0f);
}

