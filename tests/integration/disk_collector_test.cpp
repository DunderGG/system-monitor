#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>

#include "domain/disk_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/disk_collector.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;

TEST(DiskCollectorIntegration, RealHostSampling_DetectsFixedDrivesWithSensibleMetrics)
{
    DiskCollector collector;
    const auto samples = collector.collect();

    // Any working Windows installation has at least one fixed drive (typically C:\)
    EXPECT_FALSE(samples.empty());

    for (const auto &disk : samples) {
        EXPECT_FALSE(disk.volumeName.empty());
        EXPECT_GT(disk.totalBytes, 0u);
        EXPECT_LE(disk.freeBytes, disk.totalBytes);

        EXPECT_FALSE(std::isnan(disk.usagePercent));
        EXPECT_FALSE(std::isinf(disk.usagePercent));
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);
    }
}

TEST(DiskCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealDisks)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setDiskCollector(std::make_unique<DiskCollector>());

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
        EXPECT_FALSE(lastSnapshot.disks.empty());
        for (const auto &disk : lastSnapshot.disks) {
            EXPECT_FALSE(disk.volumeName.empty());
            EXPECT_GT(disk.totalBytes, 0u);
            EXPECT_GE(disk.usagePercent, 0.0f);
            EXPECT_LE(disk.usagePercent, 100.0f);
        }
    }
}

