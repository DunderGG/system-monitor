#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <QLabel>
#include <QString>

#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "domain/system_activity_sample.h"
#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

QString valueOf(const std::vector<Readout>& readouts, const QString& name)
{
    for (const auto& readout : readouts) {
        if (readout.name == name) {
            return readout.value;
        }
    }
    ADD_FAILURE() << "no readout named " << name.toStdString();
    return {};
}

std::vector<QString> namesOf(const std::vector<Readout>& readouts)
{
    std::vector<QString> names;
    for (const auto& readout : readouts) {
        names.push_back(readout.name);
    }
    return names;
}

} // namespace

TEST(PerformanceReadouts, FormatCount_GroupsThousands)
{
    EXPECT_EQ(formatCount(0), "0");
    EXPECT_EQ(formatCount(999), "999");
    EXPECT_EQ(formatCount(1000), "1,000");
    EXPECT_EQ(formatCount(200'157), "200,157");
    EXPECT_EQ(formatCount(1'234'567), "1,234,567");
}

TEST(PerformanceReadouts, FormatClockSpeed_GigahertzFromOneThousandMegahertz)
{
    EXPECT_EQ(formatClockSpeed(4700), "4.70 GHz");
    EXPECT_EQ(formatClockSpeed(1000), "1.00 GHz");
    EXPECT_EQ(formatClockSpeed(800), "800 MHz");
}

TEST(PerformanceReadouts, FormatLinkSpeed_DecimalUnits)
{
    EXPECT_EQ(formatLinkSpeed(2'500'000'000ULL), "2.5 Gbps");
    EXPECT_EQ(formatLinkSpeed(1'000'000'000ULL), "1 Gbps");
    EXPECT_EQ(formatLinkSpeed(100'000'000ULL), "100 Mbps");
    EXPECT_EQ(formatLinkSpeed(866'700'000ULL), "867 Mbps");
    EXPECT_EQ(formatLinkSpeed(9'600ULL), "9.6 Kbps");
    EXPECT_EQ(formatLinkSpeed(300ULL), "300 bps");
}

TEST(PerformanceReadouts, PlaceholderReadouts_KeepsNamesAndBlanksValues)
{
    const auto readouts = placeholderReadouts({{"Handles", "1,000"}, {"Threads", "N/A"}});

    EXPECT_EQ(readouts, (std::vector<Readout>{{"Handles", "--"}, {"Threads", "--"}}));
}

TEST(PerformanceReadouts, CpuReadouts_AllValues)
{
    const CpuSample cpu{.totalUsagePercent = 12.5f, .coreCount = 16, .baseSpeedMhz = 4700};
    const SystemActivitySample activity{.processCount = 367, .threadCount = 8350, .handleCount = 200'157};

    const auto readouts =
        cpuReadouts(cpu, activity, std::chrono::hours{50} + std::chrono::minutes{3} + std::chrono::seconds{5});

    EXPECT_EQ(valueOf(readouts, "Utilization"), "12.5%");
    EXPECT_EQ(valueOf(readouts, "Base speed"), "4.70 GHz");
    EXPECT_EQ(valueOf(readouts, "Logical processors"), "16");
    EXPECT_EQ(valueOf(readouts, "Processes"), "367");
    EXPECT_EQ(valueOf(readouts, "Threads"), "8,350");
    EXPECT_EQ(valueOf(readouts, "Handles"), "200,157");
    EXPECT_EQ(valueOf(readouts, "Up time"), "2d 2h 3m");
}

TEST(PerformanceReadouts, CpuReadouts_MissingData_SameNamesWithNotAvailable)
{
    const CpuSample cpu{.totalUsagePercent = 12.5f, .coreCount = 16};

    const auto full = cpuReadouts(cpu, SystemActivitySample{.processCount = 1}, std::chrono::seconds{1});
    const auto missing = cpuReadouts(std::nullopt, std::nullopt, std::nullopt);
    const auto noSpeed = cpuReadouts(cpu, std::nullopt, std::nullopt);

    EXPECT_EQ(namesOf(missing), namesOf(full));
    for (const auto& readout : missing) {
        EXPECT_EQ(readout.value, "N/A") << readout.name.toStdString();
    }
    EXPECT_EQ(valueOf(noSpeed, "Base speed"), "N/A");
    EXPECT_EQ(valueOf(noSpeed, "Handles"), "N/A");
}

TEST(PerformanceReadouts, MemoryReadouts_AllValues)
{
    const MemorySample memory{
        .totalBytes = 32 * kGiB,
        .availableBytes = 12 * kGiB,
        .usagePercent = 62.5f,
        .commitLimit = 40 * kGiB,
        .commitCurrent = 20 * kGiB,
        .cachedBytes = 9 * kGiB,
        .pagedPoolBytes = kGiB + kGiB / 2,
        .nonPagedPoolBytes = kGiB / 2,
    };

    const auto readouts = memoryReadouts(memory);

    EXPECT_EQ(valueOf(readouts, "In use"), "20.0 GiB (62.5%)");
    EXPECT_EQ(valueOf(readouts, "Available"), "12.0 GiB");
    EXPECT_EQ(valueOf(readouts, "Total"), "32.0 GiB");
    EXPECT_EQ(valueOf(readouts, "Committed"), "20.0/40.0 GiB");
    EXPECT_EQ(valueOf(readouts, "Cached"), "9.0 GiB");
    EXPECT_EQ(valueOf(readouts, "Paged pool"), "1.5 GiB");
    EXPECT_EQ(valueOf(readouts, "Non-paged pool"), "512.0 MiB");
}

TEST(PerformanceReadouts, MemoryReadouts_MissingDetailOrSample_NotAvailable)
{
    const MemorySample memory{.totalBytes = 32 * kGiB, .availableBytes = 12 * kGiB, .usagePercent = 62.5f};

    const auto withoutDetail = memoryReadouts(memory);
    const auto missing = memoryReadouts(std::nullopt);

    EXPECT_EQ(valueOf(withoutDetail, "Cached"), "N/A");
    EXPECT_EQ(valueOf(withoutDetail, "Non-paged pool"), "N/A");
    EXPECT_EQ(namesOf(missing), namesOf(withoutDetail));
    EXPECT_EQ(valueOf(missing, "In use"), "N/A");
}

TEST(PerformanceReadouts, DiskReadouts_OnePerVolumeInNameOrder)
{
    const std::vector<DiskSample> disks{
        DiskSample{.volumeName = "D:\\", .totalBytes = 100 * kGiB, .freeBytes = 90 * kGiB, .usagePercent = 10.0f},
        DiskSample{.volumeName = "C:\\", .totalBytes = 1000 * kGiB, .freeBytes = 250 * kGiB, .usagePercent = 75.0f},
    };

    const auto readouts = diskReadouts(disks);

    ASSERT_EQ(readouts.size(), 2u);
    EXPECT_EQ(readouts[0].name, "C:\\");
    EXPECT_EQ(readouts[0].value, "750.0/1000.0 GiB used (75.0%)\n250.0 GiB free");
    EXPECT_EQ(readouts[1].name, "D:\\");
}

TEST(PerformanceReadouts, DiskReadouts_FailedOrEmpty_SaySo)
{
    EXPECT_EQ(diskReadouts(std::nullopt), (std::vector<Readout>{{"Volumes", "N/A"}}));
    EXPECT_EQ(diskReadouts(std::vector<DiskSample>{}), (std::vector<Readout>{{"Volumes", "None"}}));
}

TEST(PerformanceReadouts, AdapterReadouts_SplitsAddressesAndFormatsRates)
{
    const NetworkSample adapter{
        .adapterName = "{GUID}",
        .friendlyName = "Ethernet",
        .description = "Intel(R) Ethernet Controller I225-V",
        .inBytesTotal = 3 * kGiB,
        .outBytesTotal = 512,
        .inBytesPerSec = 2048,
        .outBytesPerSec = std::nullopt,
        .linkSpeedBps = 2'500'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
        .ipAddresses = {"192.168.1.20", "fe80::1", "10.0.0.5"},
        .dnsServers = {"192.168.1.1"},
        .isHardwareInterface = true,
    };

    const auto readouts = adapterReadouts(adapter);

    EXPECT_EQ(valueOf(readouts, "Receive"), "2.0 KiB/s");
    EXPECT_EQ(valueOf(readouts, "Send"), "N/A");
    EXPECT_EQ(valueOf(readouts, "Link speed"), "2.5 Gbps");
    EXPECT_EQ(valueOf(readouts, "Type"), "Hardware");
    EXPECT_EQ(valueOf(readouts, "Adapter"), "Intel(R) Ethernet Controller I225-V");
    EXPECT_EQ(valueOf(readouts, "IPv4 address"), "192.168.1.20\n10.0.0.5");
    EXPECT_EQ(valueOf(readouts, "IPv6 address"), "fe80::1");
    EXPECT_EQ(valueOf(readouts, "DNS servers"), "192.168.1.1");
    EXPECT_EQ(valueOf(readouts, "Received"), "3.0 GiB");
    EXPECT_EQ(valueOf(readouts, "Sent"), "512 B");
}

TEST(PerformanceReadouts, AdapterReadouts_UnknownLinkSpeedAndNoAddresses)
{
    const NetworkSample adapter{.adapterName = "{GUID}", .isHardwareInterface = false};

    const auto readouts = adapterReadouts(adapter);

    EXPECT_EQ(valueOf(readouts, "Link speed"), "N/A");
    EXPECT_EQ(valueOf(readouts, "Type"), "Virtual");
    EXPECT_EQ(valueOf(readouts, "IPv4 address"), "None");
    EXPECT_EQ(namesOf(adapterReadouts(std::nullopt)), namesOf(readouts));
}

TEST(ReadoutGrid, SetReadouts_ShowsNamesAndValues)
{
    ReadoutGrid grid(3);

    grid.setReadouts({{"Processes", "367"}, {"Threads", "8,350"}});

    EXPECT_EQ(grid.value("Processes"), "367");
    EXPECT_EQ(grid.value("Threads"), "8,350");
    EXPECT_TRUE(grid.value("Handles").isEmpty());
}

TEST(ReadoutGrid, SetReadoutsWithSameNames_UpdatesValuesInPlace)
{
    ReadoutGrid grid(3);
    grid.setReadouts({{"Processes", "367"}});
    const QLabel* valueLabel = nullptr;
    for (const auto* label : grid.findChildren<QLabel*>()) {
        if (label->text() == "367") {
            valueLabel = label;
        }
    }
    ASSERT_NE(valueLabel, nullptr);

    grid.setReadouts({{"Processes", "368"}});

    EXPECT_EQ(grid.value("Processes"), "368");
    EXPECT_EQ(valueLabel->text(), "368"); // The same label, not a rebuilt one.
}

TEST(ReadoutGrid, SetReadoutsWithNewNames_Rebuilds)
{
    ReadoutGrid grid(2);
    grid.setReadouts({{"C:\\", "75%"}});

    grid.setReadouts({{"C:\\", "75%"}, {"D:\\", "10%"}});

    EXPECT_EQ(grid.readouts(), (std::vector<Readout>{{"C:\\", "75%"}, {"D:\\", "10%"}}));
}
