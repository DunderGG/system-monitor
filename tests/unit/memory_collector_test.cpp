#include <cstdint>

#include <gtest/gtest.h>

#include "domain/memory_sample.h"
#include "platform/windows/memory_collector.h"

using namespace sysmon::platform;

TEST(MemoryCollector, CalculateMemorySample_NormalValues_CalculatesUsageAndCommit)
{
    constexpr uint64_t k16GiB = 16ULL * 1024 * 1024 * 1024;
    constexpr uint64_t k4GiB = 4ULL * 1024 * 1024 * 1024;
    constexpr uint64_t k24GiB = 24ULL * 1024 * 1024 * 1024;
    constexpr uint64_t k8GiB = 8ULL * 1024 * 1024 * 1024;

    const MemoryStatusData data{
        .totalPhys = k16GiB,
        .availPhys = k4GiB,
        .totalPageFile = k24GiB,
        .availPageFile = k8GiB,
    };

    const auto sample = calculateMemorySample(data);

    EXPECT_EQ(sample.totalBytes, k16GiB);
    EXPECT_EQ(sample.availableBytes, k4GiB);
    EXPECT_FLOAT_EQ(sample.usagePercent, 75.0f);
    EXPECT_EQ(sample.commitLimit, k24GiB);
    EXPECT_EQ(sample.commitCurrent, 16ULL * 1024 * 1024 * 1024);
}

TEST(MemoryCollector, CalculateMemorySample_ZeroTotal_ReturnsZeroPercent)
{
    const MemoryStatusData data{
        .totalPhys = 0,
        .availPhys = 0,
        .totalPageFile = 0,
        .availPageFile = 0,
    };

    const auto sample = calculateMemorySample(data);

    EXPECT_EQ(sample.totalBytes, 0u);
    EXPECT_EQ(sample.availableBytes, 0u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 0.0f);
    EXPECT_EQ(sample.commitLimit, 0u);
    EXPECT_EQ(sample.commitCurrent, 0u);
}

TEST(MemoryCollector, CalculateMemorySample_AvailExceedsTotal_ClampsUsageToZero)
{
    constexpr uint64_t k16GiB = 16ULL * 1024 * 1024 * 1024;
    constexpr uint64_t k20GiB = 20ULL * 1024 * 1024 * 1024;

    const MemoryStatusData data{
        .totalPhys = k16GiB,
        .availPhys = k20GiB,
        .totalPageFile = k16GiB,
        .availPageFile = k20GiB,
    };

    const auto sample = calculateMemorySample(data);

    EXPECT_EQ(sample.totalBytes, k16GiB);
    EXPECT_EQ(sample.availableBytes, k16GiB);
    EXPECT_FLOAT_EQ(sample.usagePercent, 0.0f);
    EXPECT_EQ(sample.commitCurrent, 0u);
}

TEST(MemoryCollector, CalculateMemorySample_FullyUsed_Returns100Percent)
{
    constexpr uint64_t k16GiB = 16ULL * 1024 * 1024 * 1024;

    const MemoryStatusData data{
        .totalPhys = k16GiB,
        .availPhys = 0,
        .totalPageFile = k16GiB,
        .availPageFile = 0,
    };

    const auto sample = calculateMemorySample(data);

    EXPECT_EQ(sample.totalBytes, k16GiB);
    EXPECT_EQ(sample.availableBytes, 0u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 100.0f);
    EXPECT_EQ(sample.commitCurrent, k16GiB);
}

TEST(MemoryCollector, Collect_InjectedReader_ReturnsCalculatedSample)
{
    constexpr uint64_t k32GiB = 32ULL * 1024 * 1024 * 1024;
    constexpr uint64_t k16GiB = 16ULL * 1024 * 1024 * 1024;

    auto reader = [](MemoryStatusData &data) {
        data.totalPhys = k32GiB;
        data.availPhys = k16GiB;
        data.totalPageFile = k32GiB;
        data.availPageFile = k16GiB;
        return true;
    };

    MemoryCollector collector(reader);
    const auto sample = collector.collect();

    EXPECT_EQ(sample.totalBytes, k32GiB);
    EXPECT_EQ(sample.availableBytes, k16GiB);
    EXPECT_FLOAT_EQ(sample.usagePercent, 50.0f);
}

TEST(MemoryCollector, Collect_ReaderFails_ReturnsDefaultSample)
{
    auto reader = [](MemoryStatusData &) {
        return false;
    };

    MemoryCollector collector(reader);
    const auto sample = collector.collect();

    EXPECT_EQ(sample.totalBytes, 0u);
    EXPECT_EQ(sample.availableBytes, 0u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 0.0f);
}

