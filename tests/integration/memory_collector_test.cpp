#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "domain/memory_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/memory_collector.h"
#include "support/scheduler_harness.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::waitForSnapshot;

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

TEST(MemoryCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealMetrics)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setMemoryCollector(std::make_unique<MemoryCollector>());

    const auto snapshot = waitForSnapshot(scheduler, [](const SystemSnapshot&) { return true; });

    ASSERT_TRUE(snapshot.has_value());
    ASSERT_TRUE(snapshot->memory.has_value());
    EXPECT_GT(snapshot->memory->totalBytes, 0u);
    EXPECT_GT(snapshot->memory->availableBytes, 0u);
    EXPECT_GE(snapshot->memory->usagePercent, 0.0f);
    EXPECT_LE(snapshot->memory->usagePercent, 100.0f);
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
