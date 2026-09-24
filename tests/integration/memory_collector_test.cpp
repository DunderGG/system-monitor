#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>

#include "domain/memory_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/memory_collector.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;

TEST(MemoryCollectorIntegration, RealHostSampling_ProducesSensibleMetrics)
{
    MemoryCollector collector;
    const auto sample = collector.collect();

    // Physical RAM must be greater than zero on any functioning PC
    EXPECT_GT(sample.totalBytes, 0u);
    EXPECT_GT(sample.availableBytes, 0u);
    EXPECT_LE(sample.availableBytes, sample.totalBytes);

    EXPECT_FALSE(std::isnan(sample.usagePercent));
    EXPECT_FALSE(std::isinf(sample.usagePercent));
    EXPECT_GE(sample.usagePercent, 0.0f);
    EXPECT_LE(sample.usagePercent, 100.0f);

    // Commit charge invariants
    EXPECT_GT(sample.commitLimit, 0u);
    EXPECT_GT(sample.commitCurrent, 0u);
    EXPECT_LE(sample.commitCurrent, sample.commitLimit);
}

TEST(MemoryCollectorIntegration, ConsecutiveSamples_RemainStable)
{
    MemoryCollector collector;

    constexpr int kSampleCount = 3;
    for (int i = 0; i < kSampleCount; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        const auto sample = collector.collect();

        EXPECT_GT(sample.totalBytes, 0u);
        EXPECT_GT(sample.availableBytes, 0u);
        EXPECT_LE(sample.availableBytes, sample.totalBytes);
        EXPECT_GE(sample.usagePercent, 0.0f);
        EXPECT_LE(sample.usagePercent, 100.0f);
        EXPECT_FALSE(std::isnan(sample.usagePercent));
    }
}

TEST(MemoryCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealMetrics)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setMemoryCollector(std::make_unique<MemoryCollector>());

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
    while (snapshotCount.load() < 2 && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    EXPECT_GE(snapshotCount.load(), 1);
    {
        std::lock_guard lock(snapshotMutex);
        EXPECT_GT(lastSnapshot.memory.totalBytes, 0u);
        EXPECT_GT(lastSnapshot.memory.availableBytes, 0u);
        EXPECT_GE(lastSnapshot.memory.usagePercent, 0.0f);
        EXPECT_LE(lastSnapshot.memory.usagePercent, 100.0f);
    }
}

