#include <chrono>
#include <cstddef>
#include <optional>

#include <gtest/gtest.h>
#include <QEvent>
#include <QLabel>
#include <QObject>

#include "domain/health_status.h"
#include "domain/system_snapshot.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/dashboard_view.h"
#include "ui/resource_card.h"

using namespace sysmon::domain;
using sysmon::ui::DashboardView;
using sysmon::ui::ResourceCard;

namespace
{

// Counts QEvent::StyleChange events, which setStyleSheet() sends to a widget.
class StyleChangeCounter : public QObject
{
public:
    int count = 0;

protected:
    bool eventFilter(QObject * /*watched*/, QEvent *event) override
    {
        if (event->type() == QEvent::StyleChange) {
            ++count;
        }
        return false;
    }
};

// Returns the card's label that currently shows text, or nullptr.
QLabel *findLabel(const ResourceCard &card, const QString &text)
{
    for (auto *label : card.findChildren<QLabel *>()) {
        if (label->text() == text) {
            return label;
        }
    }
    return nullptr;
}

SystemSnapshot populatedSnapshot()
{
    SystemSnapshot snapshot;
    snapshot.cpu = CpuSample{.totalUsagePercent = 42.5f, .coreCount = 8};
    snapshot.memory = MemorySample{
        .totalBytes = 34'359'738'368ULL,     // 32 GiB
        .availableBytes = 17'179'869'184ULL, // 16 GiB
        .usagePercent = 50.0f,
    };
    snapshot.disks = std::vector<DiskSample>{
        DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .freeBytes = 250, .usagePercent = 75.0f},
        DiskSample{.volumeName = "D:\\", .totalBytes = 1000, .freeBytes = 900, .usagePercent = 10.0f},
    };
    snapshot.networks = std::vector<NetworkSample>{NetworkSample{
        .adapterName = "Ethernet",
        .inBytesPerSec = 2048,
        .outBytesPerSec = 1024,
        .operationalStatus = OperationalStatus::Up,
        .isHardwareInterface = true,
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

TEST(ResourceCard, SetStatus_SameLevelAgain_DoesNotRestyle)
{
    ResourceCard card("Memory");
    card.setStatus(HealthLevel::Warning);
    auto *statusLabel = findLabel(card, "Warning");
    ASSERT_NE(statusLabel, nullptr);
    StyleChangeCounter counter;
    statusLabel->installEventFilter(&counter);

    card.setStatus(HealthLevel::Warning);
    card.setStatus(HealthLevel::Warning);
    const int restylesForSameLevel = counter.count;
    card.setStatus(HealthLevel::Critical);

    EXPECT_EQ(restylesForSameLevel, 0);
    EXPECT_GT(counter.count, 0);
    EXPECT_EQ(card.statusText(), "Critical");
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
    (*snapshot.networks)[0].operationalStatus = OperationalStatus::Down;

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), "No active adapter");
}

TEST(DashboardView, UpdateSnapshot_NoNetworkData_ShowsNotAvailableNotNoAdapter)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();
    snapshot.networks.reset(); // e.g. GetIfTable2 failed

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.networkCard()->detailText(), "Internet access");
    EXPECT_FALSE(dashboard.networkSparkline()->samples().back().has_value());
}

TEST(DashboardView, UpdateSnapshot_ThroughputNotYetAvailable_ShowsNotAvailable)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();
    (*snapshot.networks)[0].inBytesPerSec.reset();

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), "N/A");
    EXPECT_EQ(dashboard.networkCard()->detailText(), "Internet access");
}

TEST(DashboardView, UpdateSnapshot_RecordsSamplesInSparklines)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(populatedSnapshot());

    ASSERT_EQ(dashboard.cpuSparkline()->samples().size(), 1u);
    EXPECT_EQ(dashboard.cpuSparkline()->samples()[0], 42.5f);
    EXPECT_EQ(dashboard.memorySparkline()->samples()[0], 50.0f);
    EXPECT_EQ(dashboard.diskSparkline()->samples()[0], 75.0f);
    EXPECT_EQ(dashboard.networkSparkline()->samples()[0], 3072.0f); // 2048 in + 1024 out
}

TEST(DashboardView, UpdateSnapshot_MissingData_RecordsGapNotZero)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(populatedSnapshot());
    dashboard.updateSnapshot(SystemSnapshot{});

    const auto cpuSamples = dashboard.cpuSparkline()->samples();
    ASSERT_EQ(cpuSamples.size(), 2u);
    EXPECT_TRUE(cpuSamples[0].has_value());
    EXPECT_FALSE(cpuSamples[1].has_value());
    EXPECT_FALSE(dashboard.networkSparkline()->samples()[1].has_value());
}

TEST(DashboardView, UpdateSnapshot_HistoryCappedAtCapacity)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();

    for (std::size_t i = 0; i < DashboardView::kHistoryCapacity + 10; ++i) {
        snapshot.cpu->totalUsagePercent = static_cast<float>(i);
        dashboard.updateSnapshot(snapshot);
    }

    const auto samples = dashboard.cpuSparkline()->samples();
    ASSERT_EQ(samples.size(), DashboardView::kHistoryCapacity);
    EXPECT_EQ(samples.back(), static_cast<float>(DashboardView::kHistoryCapacity + 9));
}

TEST(DashboardView, Sparklines_PercentFixedRangeNetworkAutoRange)
{
    DashboardView dashboard;

    EXPECT_FALSE(dashboard.cpuSparkline()->isAutoRange());
    EXPECT_FLOAT_EQ(dashboard.cpuSparkline()->effectiveRange().max, 100.0f);
    EXPECT_TRUE(dashboard.networkSparkline()->isAutoRange());
}
