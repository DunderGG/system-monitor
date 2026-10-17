#include <cstddef>
#include <optional>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QObject>

#include "domain/health_status.h"
#include "domain/system_snapshot.h"
#include "support/snapshots.h"
#include "support/text_contrast.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/dashboard_formatting.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/resource_card.h"
#include "ui/text_tone.h"

using namespace sysmon::domain;
using sysmon::tests::populatedSnapshot;
using sysmon::ui::connectivityText;
using sysmon::ui::DashboardView;
using sysmon::ui::formatPercent;
using sysmon::ui::formatUptime;
using sysmon::ui::healthLevelText;
using sysmon::ui::kNotAvailable;
using sysmon::ui::ResourceCard;

namespace
{

// Counts the palette and font changes that restyling a status label sends it.
class RestyleCounter : public QObject
{
public:
    int count = 0;

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override
    {
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
            ++count;
        }
        return false;
    }
};

// Returns the card's label that currently shows text, or nullptr.
QLabel* findLabel(const ResourceCard& card, const QString& text)
{
    for (auto* label : card.findChildren<QLabel*>()) {
        if (label->text() == text) {
            return label;
        }
    }
    return nullptr;
}

} // namespace

TEST(ResourceCard, Construction_ShowsPlaceholderAndUnknownStatus)
{
    ResourceCard card("CPU");

    EXPECT_EQ(card.title(), "CPU");
    EXPECT_EQ(card.valueText(), "--");
    EXPECT_EQ(card.status(), HealthLevel::Unknown);
    EXPECT_EQ(card.statusText(), healthLevelText(HealthLevel::Unknown));
    ASSERT_NE(card.sparklineSlot(), nullptr);
}

TEST(ResourceCard, SetStatusNullopt_HidesStatus)
{
    ResourceCard card("Uptime");

    card.setStatus(std::nullopt);

    EXPECT_FALSE(card.status().has_value());
    EXPECT_TRUE(card.statusText().isEmpty());
}

TEST(ResourceCard, SetValueWithMarkup_ShownAsPlainText)
{
    ResourceCard card("Disk");

    card.setValue("<b>75%</b>", "<img height=3000> 1 GB free");

    EXPECT_EQ(card.valueText(), "<b>75%</b>");
    EXPECT_EQ(card.detailText(), "<img height=3000> 1 GB free");
    for (const QString& text : {QString("<b>75%</b>"), QString("<img height=3000> 1 GB free")}) {
        const QLabel* label = findLabel(card, text);
        ASSERT_NE(label, nullptr) << text.toStdString();
        EXPECT_EQ(label->textFormat(), Qt::PlainText) << text.toStdString();
    }
}

TEST(ResourceCard, SetStatus_SameLevelAgain_DoesNotRestyle)
{
    ResourceCard card("Memory");
    card.setStatus(HealthLevel::Warning);
    auto* statusLabel = findLabel(card, healthLevelText(HealthLevel::Warning));
    ASSERT_NE(statusLabel, nullptr);
    RestyleCounter counter;
    statusLabel->installEventFilter(&counter);

    card.setStatus(HealthLevel::Warning);
    card.setStatus(HealthLevel::Warning);
    const int restylesForSameLevel = counter.count;
    card.setStatus(HealthLevel::Critical);

    EXPECT_EQ(restylesForSameLevel, 0);
    EXPECT_GT(counter.count, 0);
    EXPECT_EQ(card.statusText(), healthLevelText(HealthLevel::Critical));
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
    const SystemSnapshot snapshot = populatedSnapshot();

    dashboard.updateSnapshot(snapshot);

    // The details are the dashboard's own wording around formatted values.
    EXPECT_EQ(dashboard.cpuCard()->valueText(), formatPercent(snapshot.cpu->totalUsagePercent));
    EXPECT_EQ(dashboard.cpuCard()->detailText(), "4 cores");
    EXPECT_EQ(dashboard.memoryCard()->valueText(), formatPercent(snapshot.memory->usagePercent));
    EXPECT_EQ(dashboard.memoryCard()->detailText(), "8.0 GiB of 16.0 GiB used");
    EXPECT_EQ(dashboard.diskCard()->valueText(), formatPercent(75.0f));
    EXPECT_EQ(dashboard.diskCard()->detailText(), "C:\\ 250 B free (fullest of 2 volumes)");
    EXPECT_EQ(dashboard.networkCard()->valueText(), "In 2.0 KiB/s\nOut 1.0 KiB/s");
    EXPECT_EQ(dashboard.networkCard()->detailText(), connectivityText(snapshot.connectivity));
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), formatUptime(*snapshot.uptime));
}

TEST(DashboardView, UpdateSnapshot_ShowsHealthOnCardsAndOverall)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(populatedSnapshot());

    EXPECT_EQ(dashboard.cpuCard()->status(), HealthLevel::Healthy);
    EXPECT_EQ(dashboard.memoryCard()->status(), HealthLevel::Warning);
    EXPECT_EQ(dashboard.memoryCard()->statusText(), healthLevelText(HealthLevel::Warning));
    EXPECT_EQ(dashboard.overallStatusText(), "System status: Warning");
    EXPECT_FALSE(dashboard.uptimeCard()->status().has_value());
}

TEST(DashboardView, UpdateSnapshot_MissingData_ShowsNotAvailable)
{
    DashboardView dashboard;

    dashboard.updateSnapshot(SystemSnapshot{});

    EXPECT_EQ(dashboard.cpuCard()->valueText(), kNotAvailable);
    EXPECT_EQ(dashboard.memoryCard()->valueText(), kNotAvailable);
    EXPECT_EQ(dashboard.diskCard()->valueText(), kNotAvailable);
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), kNotAvailable);
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

    EXPECT_EQ(dashboard.networkCard()->valueText(), kNotAvailable);
    EXPECT_EQ(dashboard.networkCard()->detailText(), connectivityText(snapshot.connectivity));
    EXPECT_FALSE(dashboard.networkSparkline()->samples().back().has_value());
}

TEST(DashboardView, UpdateSnapshot_ThroughputNotYetAvailable_ShowsNotAvailable)
{
    DashboardView dashboard;
    auto snapshot = populatedSnapshot();
    (*snapshot.networks)[0].inBytesPerSec.reset();

    dashboard.updateSnapshot(snapshot);

    EXPECT_EQ(dashboard.networkCard()->valueText(), kNotAvailable);
    EXPECT_EQ(dashboard.networkCard()->detailText(), connectivityText(snapshot.connectivity));
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

TEST(DashboardView, CardStatuses_ReadableInBothColourSchemes)
{
    for (const auto scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        const sysmon::tests::ScopedColorScheme colorScheme(scheme);
        sysmon::ui::MainWindow mainWindow;
        mainWindow.setAttribute(Qt::WA_DontShowOnScreen);
        mainWindow.resize(1024, 720);
        mainWindow.show();
        SystemSnapshot snapshot = populatedSnapshot();
        snapshot.health = SystemHealth{
            .cpu = HealthLevel::Healthy,
            .memory = HealthLevel::Warning,
            .disk = HealthLevel::Critical,
            .network = HealthLevel::Unknown,
            .overall = HealthLevel::Critical,
        };

        mainWindow.onSnapshotReady(snapshot);
        QCoreApplication::processEvents();

        const DashboardView* dashboard = mainWindow.dashboardView();
        for (const ResourceCard* card :
             {dashboard->cpuCard(), dashboard->memoryCard(), dashboard->diskCard(), dashboard->networkCard()}) {
            EXPECT_GE(sysmon::tests::renderedTextContrast(mainWindow, *card->statusLabel()),
                      sysmon::tests::kMinimumTextContrast)
                << (scheme == Qt::ColorScheme::Dark ? "dark: " : "light: ") << card->statusText().toStdString();
        }
        EXPECT_EQ(dashboard->networkCard()->statusLabel()->tone(), sysmon::ui::TextTone::Dimmed);
        EXPECT_TRUE(dashboard->networkCard()->statusLabel()->font().italic());
        EXPECT_FALSE(dashboard->diskCard()->statusLabel()->font().italic());
    }
}
