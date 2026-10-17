#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "domain/cpu_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/cpu_collector.h"
#include "support/scheduler_harness.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::waitForSnapshot;

TEST(CpuCollectorIntegration, RealHostSampling_ProducesSensibleMetrics)
{
    CpuCollector collector;

    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    EXPECT_EQ(collector.coreCount(), static_cast<int>(expectedCores));

    // Sleep to ensure measurable monotonic time and CPU activity elapse
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    const auto sample = collector.collect();
    ASSERT_TRUE(sample.has_value());

    EXPECT_EQ(sample->coreCount, static_cast<int>(expectedCores));
    EXPECT_FALSE(std::isnan(sample->totalUsagePercent));
    EXPECT_FALSE(std::isinf(sample->totalUsagePercent));
    EXPECT_GE(sample->totalUsagePercent, 0.0f);
    EXPECT_LE(sample->totalUsagePercent, 100.0f);

    EXPECT_EQ(sample->coreUsagePercents.size(), static_cast<std::size_t>(sample->coreCount));
    for (float coreUsage : sample->coreUsagePercents) {
        EXPECT_FALSE(std::isnan(coreUsage));
        EXPECT_FALSE(std::isinf(coreUsage));
        EXPECT_GE(coreUsage, 0.0f);
        EXPECT_LE(coreUsage, 100.0f);
    }
}

TEST(CpuCollectorIntegration, ConsecutiveSamplesOverTime_MaintainsInvariantsAndUpdatesBaselines)
{
    CpuCollector collector;
    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    constexpr int kSampleCount = 4;
    for (int i = 0; i < kSampleCount; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        const auto sample = collector.collect();
        ASSERT_TRUE(sample.has_value());

        EXPECT_EQ(sample->coreCount, static_cast<int>(expectedCores));
        EXPECT_FALSE(std::isnan(sample->totalUsagePercent));
        EXPECT_FALSE(std::isinf(sample->totalUsagePercent));
        EXPECT_GE(sample->totalUsagePercent, 0.0f);
        EXPECT_LE(sample->totalUsagePercent, 100.0f);

        ASSERT_EQ(sample->coreUsagePercents.size(), static_cast<std::size_t>(expectedCores));
        for (float coreUsage : sample->coreUsagePercents) {
            EXPECT_FALSE(std::isnan(coreUsage));
            EXPECT_FALSE(std::isinf(coreUsage));
            EXPECT_GE(coreUsage, 0.0f);
            EXPECT_LE(coreUsage, 100.0f);
        }
    }
}

TEST(CpuCollectorIntegration, LoadGeneration_DetectsActiveWorkload)
{
    CpuCollector collector;
    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    // Initial collect to anchor baseline
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    // No time has elapsed since the constructor baseline, so this may or may not carry a value.
    [[maybe_unused]] const auto baselineSample = collector.collect();

    // Generate intensive synthetic workload on 2 background worker threads
    std::atomic<bool> stopWorkload{false};
    std::atomic<uint64_t> workloadSink{0};
    std::vector<std::jthread> workers;
    workers.reserve(2);

    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&stopWorkload, &workloadSink]() {
            uint64_t val = 0x123456789ABCDEF0ULL;
            while (!stopWorkload.load(std::memory_order_relaxed)) {
                val = (val * 6364136223846793005ULL) + 1442695040888963407ULL;
            }
            workloadSink.fetch_add(val, std::memory_order_relaxed);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{80});
    const auto activeSample = collector.collect();
    ASSERT_TRUE(activeSample.has_value());
    stopWorkload.store(true, std::memory_order_relaxed);

    EXPECT_EQ(activeSample->coreCount, static_cast<int>(expectedCores));
    EXPECT_FALSE(std::isnan(activeSample->totalUsagePercent));
    EXPECT_GE(activeSample->totalUsagePercent, 0.0f);
    EXPECT_LE(activeSample->totalUsagePercent, 100.0f);

    // With 2 threads burning CPU for 80ms, the system should sense active compute work
    bool anyCoreActive = false;
    for (float coreUsage : activeSample->coreUsagePercents) {
        if (coreUsage > 0.0f) {
            anyCoreActive = true;
            break;
        }
    }

    EXPECT_TRUE(anyCoreActive || activeSample->totalUsagePercent > 0.0f);
}

TEST(CpuCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealMetrics)
{
    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setCpuCollector(std::make_unique<CpuCollector>());

    // The first tick may only establish the CPU baseline.
    const auto snapshot =
        waitForSnapshot(scheduler, [](const SystemSnapshot& candidate) { return candidate.cpu.has_value(); });

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->cpu->coreCount, static_cast<int>(expectedCores));
    EXPECT_EQ(snapshot->cpu->coreUsagePercents.size(), static_cast<std::size_t>(expectedCores));
    EXPECT_FALSE(std::isnan(snapshot->cpu->totalUsagePercent));
    EXPECT_GE(snapshot->cpu->totalUsagePercent, 0.0f);
    EXPECT_LE(snapshot->cpu->totalUsagePercent, 100.0f);
}

TEST(CpuCollectorIntegration, RealHost_BaseSpeedIsPlausibleWhenReported)
{
    CpuCollector collector;
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    const auto sample = collector.collect();

    ASSERT_TRUE(sample.has_value());
    // Some virtual machines report no rated speed; the collector must then say so rather than report 0.
    if (sample->baseSpeedMhz) {
        EXPECT_GT(*sample->baseSpeedMhz, 100u);
        EXPECT_LT(*sample->baseSpeedMhz, 20'000u);
    }
}
