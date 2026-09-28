#include <chrono>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/disk_sample.h"
#include "domain/health_status.h"
#include "domain/network_sample.h"
#include "ui/dashboard_formatting.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

NetworkSample adapter(OperationalStatus status,
                      std::optional<uint64_t> in,
                      std::optional<uint64_t> out,
                      bool isHardwareInterface = true)
{
    return NetworkSample{
        .adapterName = "adapter",
        .inBytesPerSec = in,
        .outBytesPerSec = out,
        .operationalStatus = status,
        .isHardwareInterface = isHardwareInterface,
    };
}

} // namespace

TEST(DashboardFormatting, FormatPercent_OneDecimal)
{
    EXPECT_EQ(formatPercent(42.54f), "42.5%");
    EXPECT_EQ(formatPercent(0.0f), "0.0%");
    EXPECT_EQ(formatPercent(100.0f), "100.0%");
}

TEST(DashboardFormatting, FormatBytes_BelowOneKiB_UsesBytes)
{
    EXPECT_EQ(formatBytes(0), "0 B");
    EXPECT_EQ(formatBytes(1023), "1023 B");
}

TEST(DashboardFormatting, FormatBytes_LargerValues_UseBinaryUnits)
{
    EXPECT_EQ(formatBytes(1024), "1.0 KiB");
    EXPECT_EQ(formatBytes(1536), "1.5 KiB");
    EXPECT_EQ(formatBytes(16ULL * 1024 * 1024 * 1024), "16.0 GiB");
    EXPECT_EQ(formatBytes(2ULL * 1024 * 1024 * 1024 * 1024), "2.0 TiB");
}

TEST(DashboardFormatting, FormatByteRate_AppendsPerSecond)
{
    EXPECT_EQ(formatByteRate(0), "0 B/s");
    EXPECT_EQ(formatByteRate(1'258'291), "1.2 MiB/s");
}

TEST(DashboardFormatting, FormatUptime_UnderOneHour_MinutesAndSeconds)
{
    EXPECT_EQ(formatUptime(std::chrono::seconds{45}), "0m 45s");
    EXPECT_EQ(formatUptime(std::chrono::minutes{12} + std::chrono::seconds{5}), "12m 5s");
}

TEST(DashboardFormatting, FormatUptime_UnderOneDay_HoursAndMinutes)
{
    EXPECT_EQ(formatUptime(std::chrono::hours{4} + std::chrono::minutes{12} + std::chrono::seconds{59}), "4h 12m");
}

TEST(DashboardFormatting, FormatUptime_OneDayOrMore_DaysHoursMinutes)
{
    EXPECT_EQ(formatUptime(std::chrono::hours{3 * 24 + 4} + std::chrono::minutes{12}), "3d 4h 12m");
}

TEST(DashboardFormatting, HealthLevelText_AllLevels)
{
    EXPECT_EQ(healthLevelText(HealthLevel::Unknown), "Unknown");
    EXPECT_EQ(healthLevelText(HealthLevel::Healthy), "Healthy");
    EXPECT_EQ(healthLevelText(HealthLevel::Warning), "Warning");
    EXPECT_EQ(healthLevelText(HealthLevel::Critical), "Critical");
}

TEST(DashboardFormatting, ConnectivityText_DescribesLevel)
{
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess}), "Internet access");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::None}), "No connectivity");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::Unknown}), "Connectivity unknown");
}

TEST(DashboardFormatting, ConnectivityText_MeteredOnlyWhenKnown)
{
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess, .isMetered = true}),
              "Internet access, metered");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess, .isMetered = false}), "Internet access");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess, .isMetered = std::nullopt}),
              "Internet access");
}

TEST(DashboardFormatting, SumActiveThroughput_SumsOnlyUpAdapters)
{
    const std::vector<NetworkSample> networks = {
        adapter(OperationalStatus::Up, 1'000, 100),
        adapter(OperationalStatus::Up, 2'000, 200),
        adapter(OperationalStatus::Down, std::nullopt, std::nullopt),
    };

    const auto totals = sumActiveThroughput(networks);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->inBytesPerSec, 3'000u);
    EXPECT_EQ(totals->outBytesPerSec, 300u);
}

TEST(DashboardFormatting, SumActiveThroughput_UpAdapterWithoutRate_ReturnsNullopt)
{
    const std::vector<NetworkSample> networks = {
        adapter(OperationalStatus::Up, 1'000, 100),
        adapter(OperationalStatus::Up, std::nullopt, 200),
    };

    EXPECT_FALSE(sumActiveThroughput(networks).has_value());
}

TEST(DashboardFormatting, SumActiveThroughput_NoUpAdapters_ReturnsNullopt)
{
    const std::vector<NetworkSample> networks = {adapter(OperationalStatus::Down, 0, 0)};

    EXPECT_FALSE(sumActiveThroughput(networks).has_value());
    EXPECT_FALSE(hasActiveAdapter(networks));
}

TEST(DashboardFormatting, SumActiveThroughput_VirtualAdapter_NotCountedTwice)
{
    // A VPN tunnel's traffic also crosses the physical adapter it runs over.
    const std::vector<NetworkSample> networks = {
        adapter(OperationalStatus::Up, 1'000, 100),
        adapter(OperationalStatus::Up, 900, 90, false),
    };

    const auto totals = sumActiveThroughput(networks);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->inBytesPerSec, 1'000u);
    EXPECT_EQ(totals->outBytesPerSec, 100u);
}

TEST(DashboardFormatting, SumActiveThroughput_VirtualAdapterWithoutRate_Ignored)
{
    const std::vector<NetworkSample> networks = {
        adapter(OperationalStatus::Up, 1'000, 100),
        adapter(OperationalStatus::Up, std::nullopt, std::nullopt, false),
    };

    EXPECT_TRUE(sumActiveThroughput(networks).has_value());
}

TEST(DashboardFormatting, HasActiveAdapter_OnlyVirtualAdapterUp_ReturnsFalse)
{
    const std::vector<NetworkSample> networks = {
        adapter(OperationalStatus::Down, 0, 0),
        adapter(OperationalStatus::Up, 0, 0, false),
    };

    EXPECT_FALSE(hasActiveAdapter(networks));
    EXPECT_FALSE(sumActiveThroughput(networks).has_value());
}

TEST(DashboardFormatting, FullestVolume_ReturnsHighestUsage)
{
    const std::vector<DiskSample> disks = {
        DiskSample{.volumeName = "C:\\", .usagePercent = 40.0f},
        DiskSample{.volumeName = "D:\\", .usagePercent = 85.0f},
        DiskSample{.volumeName = "E:\\", .usagePercent = 60.0f},
    };

    const auto *fullest = fullestVolume(disks);

    ASSERT_NE(fullest, nullptr);
    EXPECT_EQ(fullest->volumeName, "D:\\");
}

TEST(DashboardFormatting, FullestVolume_NoVolumes_ReturnsNullptr)
{
    EXPECT_EQ(fullestVolume({}), nullptr);
}
