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
