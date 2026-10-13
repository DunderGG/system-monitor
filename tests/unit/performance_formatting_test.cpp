#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "ui/performance_formatting.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

NetworkSample adapter(std::string name, OperationalStatus status, bool isHardware)
{
    return NetworkSample{
        .adapterName = "{" + name + "}",
        .friendlyName = std::move(name),
        .inBytesPerSec = 2048,
        .outBytesPerSec = 1024,
        .operationalStatus = status,
        .isHardwareInterface = isHardware,
        .hasIpInterface = true,
    };
}

// A hidden virtual adapter that Windows keeps Up without binding IP to it, as
// it does for the WAN Miniports.
NetworkSample wanMiniport(std::string name)
{
    return NetworkSample{
        .adapterName = name,
        .friendlyName = std::move(name),
        .inBytesPerSec = 0,
        .outBytesPerSec = 0,
        .operationalStatus = OperationalStatus::Up,
        .isHardwareInterface = false,
        .hasIpInterface = false,
    };
}

} // namespace

TEST(PerformanceFormatting, FormatChartPercent_RoundsToWholePercent)
{
    EXPECT_EQ(formatChartPercent(0.0f), "0%");
    EXPECT_EQ(formatChartPercent(42.6f), "43%");
    EXPECT_EQ(formatChartPercent(100.0f), "100%");
}

TEST(PerformanceFormatting, FormatChartByteRate_UsesBinaryUnitsAndClampsNegative)
{
    EXPECT_EQ(formatChartByteRate(1536.0f), "1.5 KiB/s");
    EXPECT_EQ(formatChartByteRate(-5.0f), "0 B/s");
}

TEST(PerformanceFormatting, ChartByteRateAxisTemplates_WidestTextPerUnit)
{
    EXPECT_EQ(chartByteRateAxisTemplates(), (QStringList{"1023 B/s", "1024.0 KiB/s", "1024.0 MiB/s", "1024.0 GiB/s"}));
    // A value just below a unit boundary rounds up to "1024.0" in the unit below.
    EXPECT_EQ(formatChartByteRate(1'048'575.0f), "1024.0 KiB/s");
}

TEST(PerformanceFormatting, FormatBytesOfTotal_UsesTotalsUnitForBoth)
{
    EXPECT_EQ(formatBytesOfTotal(8 * kGiB, 16 * kGiB), "8.0/16.0 GiB");
    EXPECT_EQ(formatBytesOfTotal(512ULL * 1024ULL * 1024ULL, 16 * kGiB), "0.5/16.0 GiB");
    EXPECT_EQ(formatBytesOfTotal(100, 1000), "100/1000 B");
}

TEST(PerformanceFormatting, CommitPercent_PercentOfLimit)
{
    const MemorySample memory{.commitLimit = 40 * kGiB, .commitCurrent = 10 * kGiB};

    EXPECT_EQ(commitPercent(memory), 25.0f);
}

TEST(PerformanceFormatting, CommitPercent_ZeroLimit_Nullopt)
{
    EXPECT_FALSE(commitPercent(MemorySample{}).has_value());
}

TEST(PerformanceFormatting, AdapterDisplayName_FallsBackToDescriptionThenAdapterName)
{
    NetworkSample sample{.adapterName = "{GUID}", .friendlyName = "Wi-Fi", .description = "Intel Wireless"};
    EXPECT_EQ(adapterDisplayName(sample), "Wi-Fi");

    sample.friendlyName.clear();
    EXPECT_EQ(adapterDisplayName(sample), "Intel Wireless");

    sample.description.clear();
    EXPECT_EQ(adapterDisplayName(sample), "{GUID}");
}

TEST(PerformanceFormatting, ChartedAdapters_UpOnlyHardwareFirstThenByName)
{
    const std::vector<NetworkSample> networks{
        adapter("vEthernet", OperationalStatus::Up, false),
        adapter("Wi-Fi", OperationalStatus::Up, true),
        adapter("Bluetooth", OperationalStatus::Down, true),
        adapter("Ethernet", OperationalStatus::Up, true),
    };

    const auto charted = chartedAdapters(networks);

    ASSERT_EQ(charted.size(), 3u);
    EXPECT_EQ(charted[0]->friendlyName, "Ethernet");
    EXPECT_EQ(charted[1]->friendlyName, "Wi-Fi");
    EXPECT_EQ(charted[2]->friendlyName, "vEthernet");
}

TEST(PerformanceFormatting, ChartedAdapters_NamesSortedIgnoringCase)
{
    const std::vector<NetworkSample> networks{
        adapter("wi-fi", OperationalStatus::Up, true),
        adapter("Ethernet", OperationalStatus::Up, true),
        adapter("bridge", OperationalStatus::Up, true),
    };

    const auto charted = chartedAdapters(networks);

    ASSERT_EQ(charted.size(), 3u);
    EXPECT_EQ(charted[0]->friendlyName, "bridge");
    EXPECT_EQ(charted[1]->friendlyName, "Ethernet");
    EXPECT_EQ(charted[2]->friendlyName, "wi-fi");
}

TEST(PerformanceFormatting, ChartedAdapters_VirtualWithoutIpInterface_NotCharted)
{
    NetworkSample vpn = adapter("Mullvad", OperationalStatus::Up, false);
    vpn.description = "Mullvad Tunnel";
    vpn.ipAddresses = {"10.64.12.34"};
    const std::vector<NetworkSample> networks{
        adapter("Ethernet", OperationalStatus::Up, true),
        wanMiniport("Local Area Connection* 7"),
        wanMiniport("Local Area Connection* 8"),
        vpn,
    };

    const auto charted = chartedAdapters(networks);

    ASSERT_EQ(charted.size(), 2u);
    EXPECT_EQ(charted[0]->friendlyName, "Ethernet");
    EXPECT_EQ(charted[1]->friendlyName, "Mullvad");
}

TEST(PerformanceFormatting, ChartedAdapters_HardwareWithoutIpInterface_StillCharted)
{
    // Adapter details are unavailable (GetAdaptersAddresses failed), or IP is
    // unbound from a physical adapter: its link can still carry traffic.
    std::vector<NetworkSample> networks{adapter("Ethernet", OperationalStatus::Up, true)};
    networks[0].hasIpInterface = false;

    const auto charted = chartedAdapters(networks);

    ASSERT_EQ(charted.size(), 1u);
    EXPECT_EQ(charted[0]->friendlyName, "Ethernet");
}

TEST(PerformanceFormatting, Summaries_MissingData_NotAvailable)
{
    EXPECT_EQ(cpuSummary(std::nullopt), "N/A");
    EXPECT_EQ(memorySummary(std::nullopt), "N/A");
    EXPECT_EQ(diskSummary(std::nullopt), "N/A");
    EXPECT_EQ(networkSummary(std::nullopt), "N/A");
}

TEST(PerformanceFormatting, CpuSummary_TotalPercent)
{
    EXPECT_EQ(cpuSummary(CpuSample{.totalUsagePercent = 12.5f}), "12.5%");
}

TEST(PerformanceFormatting, MemorySummary_UsedOfTotalAndPercent)
{
    const MemorySample memory{.totalBytes = 16 * kGiB, .availableBytes = 8 * kGiB, .usagePercent = 50.0f};

    EXPECT_EQ(memorySummary(memory), "8.0/16.0 GiB (50.0%)");
}

TEST(PerformanceFormatting, DiskSummary_FullestVolumeOrNoVolumes)
{
    const std::vector<DiskSample> disks{
        DiskSample{.volumeName = "C:\\", .usagePercent = 40.0f},
        DiskSample{.volumeName = "D:\\", .usagePercent = 75.0f},
    };

    EXPECT_EQ(diskSummary(disks), "D:\\ 75.0% used");
    EXPECT_EQ(diskSummary(std::vector<DiskSample>{}), "No volumes");
}

TEST(PerformanceFormatting, NetworkSummary_ReceiveAndSendOnTwoLines)
{
    const std::vector<NetworkSample> networks{adapter("Ethernet", OperationalStatus::Up, true)};

    EXPECT_EQ(networkSummary(networks), "Receive 2.0 KiB/s\nSend 1.0 KiB/s");
}

TEST(PerformanceFormatting, NetworkSummary_NoActiveAdapter_SaysSo)
{
    const std::vector<NetworkSample> networks{adapter("Ethernet", OperationalStatus::Down, true)};

    EXPECT_EQ(networkSummary(networks), "No active adapter");
}
