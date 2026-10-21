#include <chrono>
#include <cmath>

#include <gtest/gtest.h>

#include "domain/disk_sample.h"
#include "platform/windows/disk_collector.h"

using namespace verdandi::platform;

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
