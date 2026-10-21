#include <chrono>
#include <cmath>
#include <thread>

#include <gtest/gtest.h>

#include "domain/memory_sample.h"
#include "platform/windows/memory_collector.h"

using namespace verdandi::platform;

TEST(MemoryCollectorIntegration, RealHostSampling_ProducesSensibleMetrics)
{
    MemoryCollector collector;
    const auto sample = collector.collect();
    ASSERT_TRUE(sample.has_value());

    // Physical RAM must be greater than zero on any functioning PC
    EXPECT_GT(sample->totalBytes, 0u);
    EXPECT_GT(sample->availableBytes, 0u);
    EXPECT_LE(sample->availableBytes, sample->totalBytes);

    EXPECT_FALSE(std::isnan(sample->usagePercent));
    EXPECT_FALSE(std::isinf(sample->usagePercent));
    EXPECT_GE(sample->usagePercent, 0.0f);
    EXPECT_LE(sample->usagePercent, 100.0f);

    // Commit charge invariants
    EXPECT_GT(sample->commitLimit, 0u);
    EXPECT_GT(sample->commitCurrent, 0u);
    EXPECT_LE(sample->commitCurrent, sample->commitLimit);
}

TEST(MemoryCollectorIntegration, ConsecutiveSamples_RemainStable)
{
    MemoryCollector collector;

    constexpr int kSampleCount = 3;
    for (int i = 0; i < kSampleCount; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        const auto sample = collector.collect();
        ASSERT_TRUE(sample.has_value());

        EXPECT_GT(sample->totalBytes, 0u);
        EXPECT_GT(sample->availableBytes, 0u);
        EXPECT_LE(sample->availableBytes, sample->totalBytes);
        EXPECT_GE(sample->usagePercent, 0.0f);
        EXPECT_LE(sample->usagePercent, 100.0f);
        EXPECT_FALSE(std::isnan(sample->usagePercent));
    }
}

TEST(MemoryCollectorIntegration, RealHost_CacheAndPoolSizesArePlausible)
{
    MemoryCollector collector;

    const auto sample = collector.collect();

    ASSERT_TRUE(sample.has_value());
    ASSERT_TRUE(sample->systemCacheBytes.has_value());
    ASSERT_TRUE(sample->pagedPoolBytes.has_value());
    ASSERT_TRUE(sample->nonPagedPoolBytes.has_value());
    // The kernel always has some pool memory; the cache and pools fit in RAM
    // plus the commit limit, however they are counted.
    EXPECT_GT(*sample->pagedPoolBytes, 0u);
    EXPECT_GT(*sample->nonPagedPoolBytes, 0u);
    EXPECT_LE(*sample->systemCacheBytes, sample->totalBytes);
    EXPECT_LE(*sample->nonPagedPoolBytes, sample->totalBytes);
}
