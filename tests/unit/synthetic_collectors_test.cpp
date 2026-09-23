#include <cmath>
#include <concepts>
#include <cstdint>

#include <gtest/gtest.h>

#include "domain/cpu_sample.h"
#include "domain/memory_sample.h"
#include "monitoring/collector.h"
#include "monitoring/synthetic_cpu_collector.h"
#include "monitoring/synthetic_memory_collector.h"

using sysmon::monitoring::Collector;
using sysmon::monitoring::SyntheticCpuCollector;
using sysmon::monitoring::SyntheticMemoryCollector;

static_assert(Collector<SyntheticCpuCollector, sysmon::domain::CpuSample>,
              "SyntheticCpuCollector must satisfy Collector concept");
static_assert(Collector<SyntheticMemoryCollector, sysmon::domain::MemorySample>,
              "SyntheticMemoryCollector must satisfy Collector concept");

TEST(SyntheticCpuCollector, Collect_ReturnsValidUsageAndCores)
{
    SyntheticCpuCollector collector(8, 40.0f);

    const auto sample = collector.collect();

    EXPECT_EQ(sample.coreCount, 8);
    ASSERT_EQ(sample.coreUsagePercents.size(), 8u);
    EXPECT_GE(sample.totalUsagePercent, 0.0f);
    EXPECT_LE(sample.totalUsagePercent, 100.0f);

    for (const float coreUsage : sample.coreUsagePercents) {
        EXPECT_GE(coreUsage, 0.0f);
        EXPECT_LE(coreUsage, 100.0f);
    }
}

TEST(SyntheticCpuCollector, ConsecutiveCollects_VaryUsage)
{
    SyntheticCpuCollector collector(4, 50.0f);

    const auto sample1 = collector.collect();
    const auto sample2 = collector.collect();

    EXPECT_EQ(collector.step(), 2u);
    EXPECT_NE(sample1.totalUsagePercent, sample2.totalUsagePercent);
}

TEST(SyntheticCpuCollector, CustomConfiguration_ReflectsInSample)
{
    SyntheticCpuCollector collector(2, 25.0f);

    collector.setCoreCount(6);
    collector.setBaseUsagePercent(60.0f);

    const auto sample = collector.collect();

    EXPECT_EQ(sample.coreCount, 6);
    EXPECT_EQ(sample.coreUsagePercents.size(), 6u);
}

TEST(SyntheticMemoryCollector, Collect_ReturnsValidMemoryMetrics)
{
    constexpr uint64_t kTotalBytes = 16ULL * 1024 * 1024 * 1024;
    constexpr uint64_t kCommitLimit = 24ULL * 1024 * 1024 * 1024;

    SyntheticMemoryCollector collector(kTotalBytes, kCommitLimit, 50.0f);

    const auto sample = collector.collect();

    EXPECT_EQ(sample.totalBytes, kTotalBytes);
    EXPECT_EQ(sample.commitLimit, kCommitLimit);
    EXPECT_LE(sample.availableBytes, sample.totalBytes);
    EXPECT_GE(sample.usagePercent, 0.0f);
    EXPECT_LE(sample.usagePercent, 100.0f);
    EXPECT_LE(sample.commitCurrent, sample.commitLimit);

    const uint64_t usedBytes = sample.totalBytes - sample.availableBytes;
    const double derivedPercent = (static_cast<double>(usedBytes) / static_cast<double>(sample.totalBytes)) * 100.0;
    EXPECT_NEAR(sample.usagePercent, derivedPercent, 0.5);
}

TEST(SyntheticMemoryCollector, ConsecutiveCollects_VaryWithinBounds)
{
    SyntheticMemoryCollector collector;

    const auto sample1 = collector.collect();
    const auto sample2 = collector.collect();

    EXPECT_EQ(collector.step(), 2u);
    EXPECT_NE(sample1.usagePercent, sample2.usagePercent);
    EXPECT_GE(sample2.usagePercent, 0.0f);
    EXPECT_LE(sample2.usagePercent, 100.0f);
}
