#include <chrono>
#include <cstddef>
#include <optional>
#include <utility>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QObject>
#include <QString>

#include "domain/health_status.h"
#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "support/snapshots.h"
#include "support/text_contrast.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/dashboard_formatting.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/resource_card.h"
#include "ui/text_tone.h"

using namespace verdandi::domain;
using verdandi::tests::populatedSnapshot;
using verdandi::ui::connectivityText;
using verdandi::ui::DashboardView;
using verdandi::ui::formatBytes;
using verdandi::ui::formatCoreCount;
using verdandi::ui::formatNetworkRate;
using verdandi::ui::formatPercent;
using verdandi::ui::formatUptime;
using verdandi::ui::formatUptimeStart;
using verdandi::ui::healthLevelText;
using verdandi::ui::kNotAvailable;
using verdandi::ui::kUptimeToolTip;
using verdandi::ui::ResourceCard;
using verdandi::ui::usedMemoryBytes;

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

// Returns the row and column of card in the dashboard's grid of cards, or
// std::nullopt when the grid does not hold it.
std::optional<std::pair<int, int>> gridCell(const DashboardView& dashboard, ResourceCard* card)
{
    const QLayout* layout = dashboard.layout();
    for (int item = 0; item < layout->count(); ++item) {
        auto* grid = qobject_cast<QGridLayout*>(layout->itemAt(item)->layout());
        const int index = grid != nullptr ? grid->indexOf(card) : -1;
        if (index >= 0) {
            int row = 0;
            int column = 0;
            int rowSpan = 0;
            int columnSpan = 0;
            grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
            return std::pair{row, column};
        }
    }
    return std::nullopt;
}

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
    EXPECT_EQ(dashboard.cpuCard()->detailText(), formatCoreCount(snapshot.cpu->coreCount));
    EXPECT_EQ(dashboard.memoryCard()->valueText(), formatPercent(snapshot.memory->usagePercent));
    EXPECT_EQ(dashboard.memoryCard()->detailText(), "8.0 GiB of 16.0 GiB used");
    EXPECT_EQ(dashboard.diskCard()->valueText(), formatPercent(75.0f));
    EXPECT_EQ(dashboard.diskCard()->detailText(), "C:\\ 250 B free (fullest of 2 volumes)");
    EXPECT_EQ(dashboard.networkCard()->valueText(), "In 2.0 KiB/s\nOut 1.0 KiB/s");
    EXPECT_EQ(dashboard.networkCard()->detailText(), connectivityText(snapshot.connectivity));
    EXPECT_EQ(dashboard.uptimeCard()->valueText(), formatUptime(*snapshot.uptime));
}

TEST(DashboardView, UpdateSnapshot_UptimeCardSaysWhenTheCountStarted)
{
    DashboardView dashboard;
    const SystemSnapshot snapshot = populatedSnapshot();

    const auto before = std::chrono::system_clock::now();
    dashboard.updateSnapshot(snapshot);
    const auto after = std::chrono::system_clock::now();

    // The count started the uptime before the update, during which a minute may turn.
    const QString detail = dashboard.uptimeCard()->detailText();
    EXPECT_TRUE(detail == formatUptimeStart(before - *snapshot.uptime) ||
                detail == formatUptimeStart(after - *snapshot.uptime))
        << detail.toStdString();
    EXPECT_EQ(dashboard.uptimeCard()->toolTip(), kUptimeToolTip);
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
    EXPECT_TRUE(dashboard.uptimeCard()->detailText().isEmpty()); // No start to show.
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

TEST(DashboardView, SetPanels_CardsShownFillTheGridInOrderFromItsFirstCell)
{
    DashboardView dashboard;
    EXPECT_EQ(gridCell(dashboard, dashboard.cpuCard()), (std::pair{0, 0}));
    EXPECT_EQ(gridCell(dashboard, dashboard.uptimeCard()), (std::pair{1, 1}));
    constexpr DashboardPanels kPanels{.cpu = false, .memory = true, .disk = false, .network = true, .uptime = true};

    dashboard.setPanels(kPanels);

    EXPECT_EQ(dashboard.panels(), kPanels);
    EXPECT_TRUE(dashboard.cpuCard()->isHidden());
    EXPECT_TRUE(dashboard.diskCard()->isHidden());
    EXPECT_EQ(gridCell(dashboard, dashboard.cpuCard()), std::nullopt);
    EXPECT_EQ(gridCell(dashboard, dashboard.diskCard()), std::nullopt);
    for (const ResourceCard* card : {dashboard.memoryCard(), dashboard.networkCard(), dashboard.uptimeCard()}) {
        EXPECT_FALSE(card->isHidden()) << card->title().toStdString();
    }
    EXPECT_EQ(gridCell(dashboard, dashboard.memoryCard()), (std::pair{0, 0}));
    EXPECT_EQ(gridCell(dashboard, dashboard.networkCard()), (std::pair{0, 1}));
    EXPECT_EQ(gridCell(dashboard, dashboard.uptimeCard()), (std::pair{0, 2}));
    EXPECT_TRUE(dashboard.noCardsLabel()->isHidden());
}

TEST(DashboardView, SetPanels_EveryCardOff_SaysWhereToTurnThemOnUntilOneIs)
{
    DashboardView dashboard;
    EXPECT_TRUE(dashboard.noCardsLabel()->isHidden());

    dashboard.setPanels({.cpu = false, .memory = false, .disk = false, .network = false, .uptime = false});

    EXPECT_FALSE(dashboard.noCardsLabel()->isHidden());
    for (const ResourceCard* card : {dashboard.cpuCard(), dashboard.memoryCard(), dashboard.diskCard(),
                                     dashboard.networkCard(), dashboard.uptimeCard()}) {
        EXPECT_TRUE(card->isHidden()) << card->title().toStdString();
    }

    dashboard.setPanels({.cpu = false, .memory = false, .disk = false, .network = false, .uptime = true});

    EXPECT_TRUE(dashboard.noCardsLabel()->isHidden());
    EXPECT_EQ(gridCell(dashboard, dashboard.uptimeCard()), (std::pair{0, 0}));
}

TEST(DashboardView, SetPanels_HiddenCardKeepsRecording)
{
    DashboardView dashboard;
    dashboard.setPanels({.cpu = false});
    const SystemSnapshot snapshot = populatedSnapshot();
    dashboard.updateSnapshot(snapshot);
    dashboard.updateSnapshot(snapshot);

    dashboard.setPanels({});

    EXPECT_FALSE(dashboard.cpuCard()->isHidden());
    EXPECT_EQ(dashboard.cpuSparkline()->samples().size(), 2u);
    EXPECT_EQ(dashboard.cpuCard()->valueText(), formatPercent(snapshot.cpu->totalUsagePercent));
}

TEST(DashboardView, SetDisplayUnits_ShowsTheLatestSizesAndRatesInThemAtOnce)
{
    DashboardView dashboard;
    const SystemSnapshot snapshot = populatedSnapshot();
    dashboard.updateSnapshot(snapshot);
    constexpr DisplayUnits kUnits{.bytes = ByteUnits::Decimal, .networkRates = NetworkRateUnits::BitsPerSecond};

    dashboard.setDisplayUnits(kUnits);

    EXPECT_EQ(dashboard.displayUnits(), kUnits);
    EXPECT_EQ(dashboard.memoryCard()->detailText(),
              formatBytes(usedMemoryBytes(*snapshot.memory), kUnits.bytes) + " of " +
                  formatBytes(snapshot.memory->totalBytes, kUnits.bytes) + " used");
    EXPECT_EQ(dashboard.diskCard()->detailText(),
              "C:\\ " + formatBytes(250, kUnits.bytes) + " free (fullest of 2 volumes)");
    EXPECT_EQ(dashboard.networkCard()->valueText(),
              "In " + formatNetworkRate(2048, kUnits) + "\nOut " + formatNetworkRate(1024, kUnits));
    // The units change what is shown, not what is recorded.
    EXPECT_EQ(dashboard.networkSparkline()->samples().size(), 1u);
}

TEST(DashboardView, SetDisplayUnits_BeforeTheFirstSnapshot_KeepsThePlaceholders)
{
    DashboardView dashboard;

    dashboard.setDisplayUnits({.bytes = ByteUnits::Decimal});

    EXPECT_EQ(dashboard.memoryCard()->valueText(), "--");
    EXPECT_EQ(dashboard.diskCard()->valueText(), "--");
    EXPECT_EQ(dashboard.networkCard()->valueText(), "--");
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
        const verdandi::tests::ScopedColorScheme colorScheme(scheme);
        verdandi::ui::MainWindow mainWindow;
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
            EXPECT_GE(verdandi::tests::renderedTextContrast(mainWindow, *card->statusLabel()),
                      verdandi::tests::kMinimumTextContrast)
                << (scheme == Qt::ColorScheme::Dark ? "dark: " : "light: ") << card->statusText().toStdString();
        }
        EXPECT_EQ(dashboard->networkCard()->statusLabel()->tone(), verdandi::ui::TextTone::Dimmed);
        EXPECT_TRUE(dashboard->networkCard()->statusLabel()->font().italic());
        EXPECT_FALSE(dashboard->diskCard()->statusLabel()->font().italic());
    }
}
