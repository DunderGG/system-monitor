#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include "domain/health_status.h"
#include "domain/system_snapshot.h"
#include "ui/dashboard_view.h"
#include "ui/resource_card.h"

using namespace sysmon::domain;
using sysmon::ui::DashboardView;
using sysmon::ui::ResourceCard;

namespace
{

SystemSnapshot populatedSnapshot()
{
    SystemSnapshot snapshot;
    snapshot.cpu = CpuSample{.totalUsagePercent = 42.5f, .coreCount = 8};
    snapshot.memory = MemorySample{
        .totalBytes = 34'359'738'368ULL,     // 32 GiB
        .availableBytes = 17'179'869'184ULL, // 16 GiB
        .usagePercent = 50.0f,
    };
    snapshot.disks = {
        DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .freeBytes = 250, .usagePercent = 75.0f},
        DiskSample{.volumeName = "D:\\", .totalBytes = 1000, .freeBytes = 900, .usagePercent = 10.0f},
    };
    snapshot.networks = {NetworkSample{
        .adapterName = "Ethernet",
        .inBytesPerSec = 2048,
        .outBytesPerSec = 1024,
        .operationalStatus = OperationalStatus::Up,
    }};
    snapshot.connectivity = {.level = ConnectivityLevel::InternetAccess, .isMetered = false};
    snapshot.uptime = std::chrono::hours{4} + std::chrono::minutes{12};
    snapshot.health = SystemHealth{
        .cpu = HealthLevel::Healthy,
        .memory = HealthLevel::Warning,
        .disk = HealthLevel::Healthy,
        .network = HealthLevel::Healthy,
        .overall = HealthLevel::Warning,
    };
    return snapshot;
}

} // namespace

TEST(ResourceCard, Construction_ShowsPlaceholderAndUnknownStatus)
{
    ResourceCard card("CPU");

    EXPECT_EQ(card.title(), "CPU");
    EXPECT_EQ(card.valueText(), "--");
    EXPECT_EQ(card.status(), HealthLevel::Unknown);
    EXPECT_EQ(card.statusText(), "Unknown");
    ASSERT_NE(card.sparklineSlot(), nullptr);
}

TEST(ResourceCard, SetStatusNullopt_HidesStatus)
{
    ResourceCard card("Uptime");

    card.setStatus(std::nullopt);

    EXPECT_FALSE(card.status().has_value());
    EXPECT_TRUE(card.statusText().isEmpty());
}

TEST(DashboardView, Construction_AllCardsShowPlaceholders)
{
    DashboardView dashboard;

    EXPECT_EQ(dashboard.cpuCard()->valueText(), "--");
    EXPECT_EQ(dashboard.memoryCard()->valueText(), "--");
    EXPECT_EQ(dashboard.diskCard()->valueText(), "--");
    EXPECT_EQ(dashboard.networkCard()->valueText(), "--");
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), "--");
    EXPECT_EQ(dashboard.overallStatusText(), "System status: Unknown");
    EXPECT_FALSE(dashboard.uptimeCard()->status().has_value());
}

TEST(DashboardView, UpdateSnapshot_PopulatesAllCards)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(populatedSnapshot());

    EXPECT_EQ(dashboard.cpuCard()->valueText(), "42.5%");
    EXPECT_EQ(dashboard.cpuCard()->detailText(), "8 cores");
    EXPECT_EQ(dashboard.memoryCard()->valueText(), "50.0%");
    EXPECT_EQ(dashboard.memoryCard()->detailText(), "16.0 GiB of 32.0 GiB used");
    EXPECT_EQ(dashboard.diskCard()->valueText(), "75.0%");
    EXPECT_EQ(dashboard.diskCard()->detailText(), "C:\\ 250 B free (fullest of 2 volumes)");
    EXPECT_EQ(dashboard.networkCard()->valueText(), "In 2.0 KiB/s\nOut 1.0 KiB/s");
    EXPECT_EQ(dashboard.networkCard()->detailText(), "Internet access");
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), "4h 12m");
}

TEST(DashboardView, UpdateSnapshot_ShowsHealthOnCardsAndOverall)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(populatedSnapshot());

    EXPECT_EQ(dashboard.cpuCard()->status(), HealthLevel::Healthy);
    EXPECT_EQ(dashboard.memoryCard()->status(), HealthLevel::Warning);
    EXPECT_EQ(dashboard.memoryCard()->statusText(), "Warning");
    EXPECT_EQ(dashboard.overallStatusText(), "System status: Warning");
    EXPECT_FALSE(dashboard.uptimeCard()->status().has_value());
}

TEST(DashboardView, UpdateSnapshot_MissingData_ShowsNotAvailable)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(SystemSnapshot{});

    EXPECT_EQ(dashboard.cpuCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.memoryCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.diskCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.cpuCard()->status(), HealthLevel::Unknown);
}

TEST(DashboardView, UpdateSnapshot_NoActiveAdapter_SaysSo)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();
    snapshot.networks[0].operationalStatus = OperationalStatus::Down;

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), "No active adapter");
}

TEST(DashboardView, UpdateSnapshot_ThroughputNotYetAvailable_ShowsNotAvailable)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();
    snapshot.networks[0].inBytesPerSec.reset();

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.networkCard()->detailText(), "Internet access");
}
