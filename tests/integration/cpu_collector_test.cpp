#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>

#include <Windows.h>

#include "domain/cpu_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/cpu_collector.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;

TEST(CpuCollectorIntegration, RealHostSampling_ProducesSensibleMetrics)
{
    CpuCollector collector;

    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    EXPECT_EQ(collector.coreCount(), static_cast<int>(expectedCores));

    // Sleep to ensure measurable monotonic time and CPU activity elapse
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    const auto sample = collector.collect();

    EXPECT_EQ(sample.coreCount, static_cast<int>(expectedCores));
    EXPECT_FALSE(std::isnan(sample.totalUsagePercent));
    EXPECT_FALSE(std::isinf(sample.totalUsagePercent));
    EXPECT_GE(sample.totalUsagePercent, 0.0f);
    EXPECT_LE(sample.totalUsagePercent, 100.0f);

    EXPECT_EQ(sample.coreUsagePercents.size(), static_cast<std::size_t>(sample.coreCount));
    for (float coreUsage : sample.coreUsagePercents) {
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

        EXPECT_EQ(sample.coreCount, static_cast<int>(expectedCores));
        EXPECT_FALSE(std::isnan(sample.totalUsagePercent));
        EXPECT_FALSE(std::isinf(sample.totalUsagePercent));
        EXPECT_GE(sample.totalUsagePercent, 0.0f);
        EXPECT_LE(sample.totalUsagePercent, 100.0f);

        ASSERT_EQ(sample.coreUsagePercents.size(), static_cast<std::size_t>(expectedCores));
        for (float coreUsage : sample.coreUsagePercents) {
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
    const auto initialSample = collector.collect();
    (void)initialSample;

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
    stopWorkload.store(true, std::memory_order_relaxed);

    EXPECT_EQ(activeSample.coreCount, static_cast<int>(expectedCores));
    EXPECT_FALSE(std::isnan(activeSample.totalUsagePercent));
    EXPECT_GE(activeSample.totalUsagePercent, 0.0f);
    EXPECT_LE(activeSample.totalUsagePercent, 100.0f);

    // With 2 threads burning CPU for 80ms, the system should sense active compute work
    bool anyCoreActive = false;
    for (float coreUsage : activeSample.coreUsagePercents) {
        if (coreUsage > 0.0f) {
            anyCoreActive = true;
            break;
        }
    }

    EXPECT_TRUE(anyCoreActive || activeSample.totalUsagePercent > 0.0f);
}

TEST(CpuCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealMetrics)
{
    const DWORD expectedCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setCpuCollector(std::make_unique<CpuCollector>());

    std::atomic<int> snapshotCount{0};
    SystemSnapshot lastSnapshot;
    std::mutex snapshotMutex;

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady,
                     [&](const SystemSnapshot &snapshot) {
                         std::lock_guard lock(snapshotMutex);
                         lastSnapshot = snapshot;
                         ++snapshotCount;
                     });

    scheduler.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{1000};
    while (snapshotCount.load() < 3 && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    EXPECT_GE(snapshotCount.load(), 1);
    {
        std::lock_guard lock(snapshotMutex);
        EXPECT_EQ(lastSnapshot.cpu.coreCount, static_cast<int>(expectedCores));
        EXPECT_EQ(lastSnapshot.cpu.coreUsagePercents.size(), static_cast<std::size_t>(expectedCores));
        EXPECT_FALSE(std::isnan(lastSnapshot.cpu.totalUsagePercent));
        EXPECT_GE(lastSnapshot.cpu.totalUsagePercent, 0.0f);
        EXPECT_LE(lastSnapshot.cpu.totalUsagePercent, 100.0f);
    }
}
