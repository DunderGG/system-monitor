#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "domain/disk_sample.h"
#include "platform/windows/disk_collector.h"

using namespace sysmon::platform;

TEST(DiskCollector, CalculateDiskSample_NormalDrive_CalculatesUsage)
{
    constexpr uint64_t k1000GB = 1000ULL * 1000 * 1000 * 1000;
    constexpr uint64_t k250GB = 250ULL * 1000 * 1000 * 1000;

    const auto sample = calculateDiskSample("C:\\", k1000GB, k250GB);

    EXPECT_EQ(sample.volumeName, "C:\\");
    EXPECT_EQ(sample.totalBytes, k1000GB);
    EXPECT_EQ(sample.freeBytes, k250GB);
    EXPECT_FLOAT_EQ(sample.usagePercent, 75.0f);
}

TEST(DiskCollector, CalculateDiskSample_ZeroTotal_ReturnsZeroUsage)
{
    const auto sample = calculateDiskSample("Z:\\", 0, 0);

    EXPECT_EQ(sample.volumeName, "Z:\\");
    EXPECT_EQ(sample.totalBytes, 0u);
    EXPECT_EQ(sample.freeBytes, 0u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 0.0f);
}

TEST(DiskCollector, CalculateDiskSample_FreeExceedsTotal_ClampsUsageToZero)
{
    const auto sample = calculateDiskSample("C:\\", 1000, 1200);

    EXPECT_EQ(sample.totalBytes, 1000u);
    EXPECT_EQ(sample.freeBytes, 1000u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 0.0f);
}

TEST(DiskCollector, CalculateDiskSample_FullyFull_Returns100Percent)
{
    const auto sample = calculateDiskSample("C:\\", 1000, 0);

    EXPECT_EQ(sample.totalBytes, 1000u);
    EXPECT_EQ(sample.freeBytes, 0u);
    EXPECT_FLOAT_EQ(sample.usagePercent, 100.0f);
}

TEST(DiskCollector, Collect_MultipleFixedDrives_ReturnsAllSamples)
{
    auto enumerator = []() -> std::vector<std::string> {
        return {"C:\\", "D:\\"};
    };

    auto reader = [](const std::string &volume, DiskSpaceData &data) {
        if (volume == "C:\\") {
            data.totalBytes = 500'000'000'000ULL;
            data.freeBytes = 250'000'000'000ULL;
            return true;
        }
        if (volume == "D:\\") {
            data.totalBytes = 1'000'000'000'000ULL;
            data.freeBytes = 100'000'000'000ULL;
            return true;
        }
        return false;
    };

    DiskCollector collector(enumerator, reader);
    const auto samples = collector.collect();

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].volumeName, "C:\\");
    EXPECT_FLOAT_EQ(samples[0].usagePercent, 50.0f);

    EXPECT_EQ(samples[1].volumeName, "D:\\");
    EXPECT_FLOAT_EQ(samples[1].usagePercent, 90.0f);
}

TEST(DiskCollector, Collect_DriveReadFails_SkipsFailedVolume)
{
    auto enumerator = []() -> std::vector<std::string> {
        return {"C:\\", "E:\\"};
    };

    auto reader = [](const std::string &volume, DiskSpaceData &data) {
        if (volume == "C:\\") {
            data.totalBytes = 500'000'000'000ULL;
            data.freeBytes = 250'000'000'000ULL;
            return true;
        }
        return false; // E:\ is inaccessible
    };

    DiskCollector collector(enumerator, reader);
    const auto samples = collector.collect();

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].volumeName, "C:\\");
    EXPECT_FLOAT_EQ(samples[0].usagePercent, 50.0f);
}

TEST(DiskCollector, Collect_EmptyDrives_ReturnsEmpty)
{
    auto enumerator = []() -> std::vector<std::string> {
        return {};
    };
    auto reader = [](const std::string &, DiskSpaceData &) {
        return true;
    };

    DiskCollector collector(enumerator, reader);
    const auto samples = collector.collect();

    EXPECT_TRUE(samples.empty());
}

