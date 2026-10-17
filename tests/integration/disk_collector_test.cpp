#include <chrono>
#include <cmath>
#include <memory>

#include <gtest/gtest.h>

#include "domain/disk_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/disk_collector.h"
#include "support/scheduler_harness.h"

using namespace sysmon::platform;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::waitForSnapshot;

TEST(DiskCollectorIntegration, RealHostSampling_DetectsFixedDrivesWithSensibleMetrics)
{
    DiskCollector collector;
    const auto samples = collector.collect();

    // Any working Windows installation has at least one fixed drive (typically C:\)
    ASSERT_TRUE(samples.has_value());
    EXPECT_FALSE(samples->empty());

    for (const auto& disk : *samples) {
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

    const auto snapshot = waitForSnapshot(scheduler, [](const SystemSnapshot&) { return true; });

    ASSERT_TRUE(snapshot.has_value());
    ASSERT_TRUE(snapshot->disks.has_value());
    EXPECT_FALSE(snapshot->disks->empty());
    for (const auto& disk : *snapshot->disks) {
        EXPECT_FALSE(disk.volumeName.empty());
        EXPECT_GT(disk.totalBytes, 0u);
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);
    }
}
