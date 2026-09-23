#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <Windows.h>

#include "domain/cpu_sample.h"
#include "platform/windows/cpu_collector.h"

using namespace sysmon::platform;

TEST(CpuCollectorIntegration, RealHostSampling_ProducesSensibleMetrics)
{
    CpuCollector collector;

    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    EXPECT_EQ(collector.coreCount(), static_cast<int>(expectedCores));

    // Sleep to ensure measurable monotonic time and CPU activity elapse
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    const auto sample = collector.collect();

    EXPECT_EQ(sample.coreCount, static_cast<int>(expectedCores));
    EXPECT_GE(sample.totalUsagePercent, 0.0f);
    EXPECT_EQ(sample.coreUsagePercents.size(), static_cast<std::size_t>(sample.coreCount));
    for (float coreUsage : sample.coreUsagePercents) {
        EXPECT_GE(coreUsage, 0.0f);
        EXPECT_LE(coreUsage, 100.0f);
    }
}

