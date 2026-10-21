#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/disk_sample.h"
#include "domain/health_status.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "domain/settings.h"
#include "support/network_samples.h"
#include "ui/dashboard_formatting.h"

using namespace verdandi::domain;
using namespace verdandi::ui;
using namespace verdandi::tests;

TEST(DashboardFormatting, UsedMemoryBytes_TotalMinusAvailableNeverNegative)
{
    EXPECT_EQ(usedMemoryBytes(MemorySample{.totalBytes = 16, .availableBytes = 6}), 10u);
    EXPECT_EQ(usedMemoryBytes(MemorySample{.totalBytes = 16, .availableBytes = 20}), 0u);
}

TEST(DashboardFormatting, NotAvailable_IsNA)
{
    EXPECT_EQ(kNotAvailable, "N/A");
}

TEST(DashboardFormatting, FormatPercent_OneDecimal)
{
    EXPECT_EQ(formatPercent(42.54f), "42.5%");
    EXPECT_EQ(formatPercent(0.0f), "0.0%");
    EXPECT_EQ(formatPercent(100.0f), "100.0%");
}

TEST(DashboardFormatting, FormatCount_GroupsThousands)
{
    EXPECT_EQ(formatCount(0), "0");
    EXPECT_EQ(formatCount(999), "999");
    EXPECT_EQ(formatCount(1000), "1,000");
    EXPECT_EQ(formatCount(200'157), "200,157");
    EXPECT_EQ(formatCount(1'234'567), "1,234,567");
}

TEST(DashboardFormatting, ByteUnitFor_LargestUnitNotAboveTheCount)
{
    struct Case
    {
        const char* name;
        uint64_t bytes;
        ByteUnits units;
        uint64_t expectedSize;
        const char* expectedName;
    };
    const std::vector<Case> cases{
        {"below a KiB", 1023, ByteUnits::Binary, 1, "B"},
        {"a KiB", 1024, ByteUnits::Binary, 1024, "KiB"},
        {"just below a MiB", 1'048'575, ByteUnits::Binary, 1024, "KiB"},
        {"16 GiB", 16ULL << 30, ByteUnits::Binary, 1ULL << 30, "GiB"},
        {"beyond the largest", 2048ULL << 50, ByteUnits::Binary, 1ULL << 50, "PiB"},
        {"below a KB", 999, ByteUnits::Decimal, 1, "B"},
        {"a KB", 1000, ByteUnits::Decimal, 1000, "KB"},
        {"16 GiB in decimal units", 16ULL << 30, ByteUnits::Decimal, 1'000'000'000, "GB"},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        const ByteUnit unit = byteUnitFor(testCase.bytes, testCase.units);
        EXPECT_EQ(unit.bytes, testCase.expectedSize);
        EXPECT_EQ(unit.name, testCase.expectedName);
    }
}

TEST(DashboardFormatting, FormatBytes_WholeBytesOrOneDecimalOfTheUnit)
{
    struct Case
    {
        const char* name;
        uint64_t bytes;
        ByteUnits units;
        const char* expected;
    };
    const std::vector<Case> cases{
        {"zero", 0, ByteUnits::Binary, "0 B"},
        {"below a KiB", 1023, ByteUnits::Binary, "1023 B"},
        {"a KiB", 1024, ByteUnits::Binary, "1.0 KiB"},
        {"a KiB and a half", 1536, ByteUnits::Binary, "1.5 KiB"},
        {"just below a MiB rounds up", 1'048'575, ByteUnits::Binary, "1024.0 KiB"},
        {"16 GiB", 16ULL << 30, ByteUnits::Binary, "16.0 GiB"},
        {"2 TiB", 2ULL << 40, ByteUnits::Binary, "2.0 TiB"},
        {"beyond the largest unit", 2048ULL << 50, ByteUnits::Binary, "2048.0 PiB"},
        {"below a KB", 999, ByteUnits::Decimal, "999 B"},
        {"a KB and a half", 1500, ByteUnits::Decimal, "1.5 KB"},
        {"a KiB in decimal units", 1024, ByteUnits::Decimal, "1.0 KB"},
        {"16 GiB in decimal units", 16ULL << 30, ByteUnits::Decimal, "17.2 GB"},
        {"2 TB", 2'000'000'000'000, ByteUnits::Decimal, "2.0 TB"},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        EXPECT_EQ(formatBytes(testCase.bytes, testCase.units), testCase.expected);
    }
}

TEST(DashboardFormatting, FormatByteRate_AppendsPerSecond)
{
    EXPECT_EQ(formatByteRate(0, ByteUnits::Binary), "0 B/s");
    EXPECT_EQ(formatByteRate(1'258'291, ByteUnits::Binary), "1.2 MiB/s");
    EXPECT_EQ(formatByteRate(1'258'291, ByteUnits::Decimal), "1.3 MB/s");
}

TEST(DashboardFormatting, FormatNetworkRate_BytesInTheByteUnitsOrDecimalBits)
{
    constexpr DisplayUnits kBinaryBytes{.bytes = ByteUnits::Binary, .networkRates = NetworkRateUnits::BytesPerSecond};
    constexpr DisplayUnits kDecimalBytes{.bytes = ByteUnits::Decimal, .networkRates = NetworkRateUnits::BytesPerSecond};
    constexpr DisplayUnits kBinaryBits{.bytes = ByteUnits::Binary, .networkRates = NetworkRateUnits::BitsPerSecond};
    constexpr DisplayUnits kDecimalBits{.bytes = ByteUnits::Decimal, .networkRates = NetworkRateUnits::BitsPerSecond};
    struct Case
    {
        const char* name;
        uint64_t bytesPerSecond;
        DisplayUnits units;
        const char* expected;
    };
    const std::vector<Case> cases{
        {"bytes in binary units", 2048, kBinaryBytes, "2.0 KiB/s"},
        {"bytes in decimal units", 2048, kDecimalBytes, "2.0 KB/s"},
        {"no bits", 0, kBinaryBits, "0 bps"},
        {"bits below a Kbps", 100, kBinaryBits, "800 bps"},
        {"a Kbps", 125, kBinaryBits, "1.0 Kbps"},
        {"bits whatever the byte units", 1'262'500, kBinaryBits, "10.1 Mbps"},
        {"bits with decimal byte units", 1'262'500, kDecimalBits, "10.1 Mbps"},
        {"a Tbps", 125'000'000'000, kBinaryBits, "1.0 Tbps"},
        {"beyond any link saturates", std::numeric_limits<uint64_t>::max(), kBinaryBits, "18446.7 Pbps"},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        EXPECT_EQ(formatNetworkRate(testCase.bytesPerSecond, testCase.units), testCase.expected);
    }
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

TEST(DashboardFormatting, ConnectivityText_RoamingOnlyWhenKnown)
{
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess, .isMetered = true, .isRoaming = true}),
              "Internet access, metered, roaming");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::InternetAccess, .isMetered = false, .isRoaming = false}),
              "Internet access");
    EXPECT_EQ(connectivityText({.level = ConnectivityLevel::Unknown, .isRoaming = std::nullopt}),
              "Connectivity unknown");
}

TEST(DashboardFormatting, ConnectivityLevelText_UnknownIsNotOffline)
{
    EXPECT_EQ(connectivityLevelText(ConnectivityLevel::Unknown), "Unknown");
    EXPECT_EQ(connectivityLevelText(ConnectivityLevel::None), "No connectivity");
    EXPECT_EQ(connectivityLevelText(ConnectivityLevel::LocalAccess), "Local network only");
    EXPECT_EQ(connectivityLevelText(ConnectivityLevel::ConstrainedInternetAccess), "Limited internet (captive portal)");
}

TEST(DashboardFormatting, SumActiveThroughput_SumsOnlyUpAdapters)
{
    const std::vector<NetworkSample> networks = {
        withRates(adapter(1, "Ethernet"), 1'000, 100),
        withRates(adapter(2, "Wi-Fi"), 2'000, 200),
        withRates(adapter(3, "Ethernet 2", OperationalStatus::Down), std::nullopt, std::nullopt),
    };

    const auto totals = sumActiveThroughput(networks);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->inBytesPerSec, 3'000u);
    EXPECT_EQ(totals->outBytesPerSec, 300u);
}

TEST(DashboardFormatting, SumActiveThroughput_UpAdapterWithoutRate_ReturnsNullopt)
{
    const std::vector<NetworkSample> networks = {
        withRates(adapter(1, "Ethernet"), 1'000, 100),
        withRates(adapter(2, "Wi-Fi"), std::nullopt, 200),
    };

    EXPECT_FALSE(sumActiveThroughput(networks).has_value());
}

TEST(DashboardFormatting, SumActiveThroughput_NoUpAdapters_ReturnsNullopt)
{
    const std::vector<NetworkSample> networks = {withRates(adapter(1, "Ethernet", OperationalStatus::Down), 0, 0)};

    EXPECT_FALSE(sumActiveThroughput(networks).has_value());
    EXPECT_FALSE(hasActiveAdapter(networks));
}

TEST(DashboardFormatting, SumActiveThroughput_VirtualAdapter_NotCountedTwice)
{
    // A VPN tunnel's traffic also crosses the physical adapter it runs over.
    const std::vector<NetworkSample> networks = {
        withRates(adapter(1, "Ethernet"), 1'000, 100),
        withRates(adapter(2, "Mullvad", OperationalStatus::Up, kVirtualAdapter), 900, 90),
    };

    const auto totals = sumActiveThroughput(networks);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->inBytesPerSec, 1'000u);
    EXPECT_EQ(totals->outBytesPerSec, 100u);
}

TEST(DashboardFormatting, SumActiveThroughput_VirtualAdapterWithoutRate_Ignored)
{
    const std::vector<NetworkSample> networks = {
        withRates(adapter(1, "Ethernet"), 1'000, 100),
        withRates(adapter(2, "Mullvad", OperationalStatus::Up, kVirtualAdapter), std::nullopt, std::nullopt),
    };

    EXPECT_TRUE(sumActiveThroughput(networks).has_value());
}

TEST(DashboardFormatting, HasActiveAdapter_OnlyVirtualAdapterUp_ReturnsFalse)
{
    const std::vector<NetworkSample> networks = {
        withRates(adapter(1, "Ethernet", OperationalStatus::Down), 0, 0),
        withRates(adapter(2, "Mullvad", OperationalStatus::Up, kVirtualAdapter), 0, 0),
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

    const auto* fullest = fullestVolume(disks);

    ASSERT_NE(fullest, nullptr);
    EXPECT_EQ(fullest->volumeName, "D:\\");
}

TEST(DashboardFormatting, FullestVolume_NoVolumes_ReturnsNullptr)
{
    EXPECT_EQ(fullestVolume({}), nullptr);
}
