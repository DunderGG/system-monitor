#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QPoint>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QTabWidget>

#include "domain/system_snapshot.h"
#include "support/network_samples.h"
#include "support/readouts.h"
#include "support/snapshots.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/cpu_performance_page.h"
#include "ui/dashboard_formatting.h"
#include "ui/disk_performance_page.h"
#include "ui/main_window.h"
#include "ui/memory_performance_page.h"
#include "ui/network_performance_page.h"
#include "ui/performance_formatting.h"
#include "ui/performance_page.h"
#include "ui/performance_readouts.h"
#include "ui/performance_view.h"
#include "ui/readout_grid.h"

using namespace sysmon::domain;
using namespace sysmon::ui;
using namespace sysmon::tests;

namespace
{

// A main window at 800x600, the smallest size its content must fit, showing
// the Performance tab without opening a window on screen.
class SmallMainWindow : public MainWindow
{
public:
    SmallMainWindow()
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        resize(800, 600);
        show();
        tabWidget()->setCurrentWidget(performanceView());
    }
};

// A view that counts as visible without opening a window on screen.
class ShownPerformanceView : public PerformanceView
{
public:
    ShownPerformanceView()
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        show();
    }
};

} // namespace

TEST(PerformanceView, Construction_SidebarListsFourPagesWithPlaceholdersAndCpuSelected)
{
    PerformanceView view;

    EXPECT_EQ(view.currentPage(), PerformanceView::Page::Cpu);
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n--");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Memory), "Memory\n--");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Disk), "Disk\n--");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Network), "Network\n--");
}

TEST(PerformanceView, UpdateSnapshotWhileVisible_RefreshesCurrentPageAndSidebar)
{
    ShownPerformanceView view;

    const SystemSnapshot snapshot = populatedSnapshot();

    view.updateSnapshot(snapshot);

    ASSERT_EQ(view.cpuPage()->totalChart()->samples().size(), 1u);
    EXPECT_EQ(view.cpuPage()->totalChart()->samples()[0], 42.5f);
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n" + cpuSummary(snapshot.cpu));
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Memory), "Memory\n" + memorySummary(snapshot.memory));
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Disk), "Disk\n" + diskSummary(snapshot.disks));
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Network), "Network\n" + networkSummary(snapshot.networks));
}

TEST(PerformanceView, UpdateSnapshotWhileHidden_RecordsWithoutRefreshing)
{
    PerformanceView view;

    view.updateSnapshot(populatedSnapshot());
    view.updateSnapshot(populatedSnapshot());

    EXPECT_TRUE(view.cpuPage()->totalChart()->samples().empty());
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n--");
    EXPECT_EQ(view.cpuPage()->recordedCount(), 2u);
}

TEST(PerformanceView, ShowAfterHiddenUpdates_ShowsFullHistory)
{
    PerformanceView view;
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.updateSnapshot(populatedSnapshot());
    view.updateSnapshot(populatedSnapshot());

    view.show();

    EXPECT_EQ(view.cpuPage()->totalChart()->samples().size(), 2u);
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n" + cpuSummary(populatedSnapshot().cpu));
}

TEST(PerformanceView, SelectPage_ShowsItsHistoryRecordedWhileNotSelected)
{
    ShownPerformanceView view;
    view.updateSnapshot(populatedSnapshot());
    EXPECT_TRUE(view.memoryPage()->usageChart()->samples().empty());

    view.setCurrentPage(PerformanceView::Page::Memory);

    EXPECT_EQ(view.currentPage(), PerformanceView::Page::Memory);
    ASSERT_EQ(view.memoryPage()->usageChart()->samples().size(), 1u);
    EXPECT_EQ(view.memoryPage()->usageChart()->samples()[0], 50.0f);
}

TEST(PerformanceView, Charts_ScrollGridWithRecordedCount)
{
    ShownPerformanceView view;

    view.updateSnapshot(populatedSnapshot());
    view.updateSnapshot(populatedSnapshot());
    view.updateSnapshot(populatedSnapshot());

    EXPECT_EQ(view.cpuPage()->totalChart()->sampleIndex(), 3u);
}

TEST(CpuPerformancePage, Refresh_OneChartPerCore)
{
    CpuPerformancePage page;
    page.recordSnapshot(populatedSnapshot());

    page.refresh();

    ASSERT_EQ(page.coreChartCount(), 4u);
    ASSERT_EQ(page.coreChart(3)->samples().size(), 1u);
    EXPECT_EQ(page.coreChart(3)->samples()[0], 40.0f);
    EXPECT_FALSE(page.coreChart(0)->isAutoRange());
}

TEST(CpuPerformancePage, CoreCountChanges_RebuildsChartsAndRestartsCoreHistory)
{
    CpuPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    page.refresh();
    auto snapshot = populatedSnapshot();
    snapshot.cpu->coreUsagePercents = {5.0f, 6.0f};

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.coreChartCount(), 2u);
    ASSERT_EQ(page.coreChart(1)->samples().size(), 1u);
    EXPECT_EQ(page.coreChart(1)->samples()[0], 6.0f);
    EXPECT_EQ(page.totalChart()->samples().size(), 2u);
}

TEST(CpuPerformancePage, MissingCpuData_RecordsGapsNotZeros)
{
    CpuPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    auto withoutCores = populatedSnapshot();
    withoutCores.cpu->coreUsagePercents.clear();

    page.recordSnapshot(withoutCores);
    page.recordSnapshot(SystemSnapshot{});
    page.refresh();

    const auto total = page.totalChart()->samples();
    ASSERT_EQ(total.size(), 3u);
    EXPECT_TRUE(total[1].has_value());
    EXPECT_FALSE(total[2].has_value());
    const auto core = page.coreChart(0)->samples();
    ASSERT_EQ(core.size(), 3u);
    EXPECT_FALSE(core[1].has_value());
    EXPECT_FALSE(core[2].has_value());
    EXPECT_EQ(page.summary(), cpuSummary(std::nullopt));
}

TEST(MemoryPerformancePage, Refresh_ChartsUsageAndCommitPercent)
{
    MemoryPerformancePage page;
    page.recordSnapshot(populatedSnapshot());

    page.refresh();

    ASSERT_EQ(page.usageChart()->samples().size(), 1u);
    EXPECT_EQ(page.usageChart()->samples()[0], 50.0f);
    ASSERT_EQ(page.commitChart()->samples().size(), 1u);
    EXPECT_EQ(page.commitChart()->samples()[0], 25.0f);
}

TEST(PerformancePage, PercentCharts_AnnotationsMatchReadoutPrecision)
{
    CpuPerformancePage cpuPage;
    MemoryPerformancePage memoryPage;

    for (const auto* chart : {cpuPage.totalChart(), memoryPage.usageChart(), memoryPage.commitChart()}) {
        EXPECT_EQ(chart->formatValue(7.5f), formatChartPercent(7.5f));
        EXPECT_EQ(chart->formatAnnotationValue(7.5f), formatPercent(7.5f));
    }
}

TEST(DiskPerformancePage, Refresh_LabelledSeriesPerVolumeInNameOrder)
{
    DiskPerformancePage page;
    page.recordSnapshot(populatedSnapshot());

    page.refresh();

    EXPECT_EQ(page.volumeNames(), (std::vector<std::string>{"C:\\", "D:\\"}));
    ASSERT_EQ(page.usageChart()->seriesCount(), 2u);
    EXPECT_EQ(page.usageChart()->seriesLabel(0), "C:\\");
    EXPECT_EQ(page.usageChart()->seriesSamples(0)[0], 75.0f);
    EXPECT_EQ(page.usageChart()->seriesLabel(1), "D:\\");
    EXPECT_EQ(page.usageChart()->seriesSamples(1)[0], 10.0f);
}

TEST(DiskPerformancePage, VolumeDisappears_RecordsGapsThenDropsIt)
{
    DiskPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    auto onlyC = populatedSnapshot();
    onlyC.disks->erase(onlyC.disks->begin()); // Remove D:.

    page.recordSnapshot(onlyC);
    page.refresh();

    ASSERT_EQ(page.usageChart()->seriesCount(), 2u);
    EXPECT_FALSE(page.usageChart()->seriesSamples(1).back().has_value());

    for (std::size_t i = 0; i < PerformancePage::kHistoryCapacity; ++i) {
        page.recordSnapshot(onlyC);
    }
    page.refresh();

    EXPECT_EQ(page.volumeNames(), (std::vector<std::string>{"C:\\"}));
    EXPECT_EQ(page.usageChart()->seriesCount(), 1u);
}

TEST(DiskPerformancePage, VolumeAdded_OtherVolumesKeepTheirColors)
{
    const auto volume = [](const char* name) {
        return DiskSample{.volumeName = name, .totalBytes = 1000, .freeBytes = 500, .usagePercent = 50.0f};
    };
    DiskPerformancePage page;
    SystemSnapshot snapshot;
    snapshot.disks = std::vector<DiskSample>{volume("C:\\"), volume("E:\\")};
    page.recordSnapshot(snapshot);
    page.refresh();
    const QColor colorOfE = page.usageChart()->seriesColor(1);

    snapshot.disks->push_back(volume("D:\\"));
    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.volumeNames(), (std::vector<std::string>{"C:\\", "D:\\", "E:\\"}));
    EXPECT_EQ(page.volumeColorSlots(), (std::vector<std::size_t>{0, 2, 1}));
    EXPECT_EQ(page.usageChart()->seriesColor(2), colorOfE);
}

TEST(NetworkPerformancePage, Refresh_ChartsActiveAdaptersWithReceiveAndSend)
{
    NetworkPerformancePage page;
    page.recordSnapshot(populatedSnapshot());

    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames(), (std::vector<std::string>{"{Ethernet}"}));
    EXPECT_EQ(page.adapterTitle(0), "Ethernet");
    const auto* chart = page.adapterChart(0);
    ASSERT_EQ(chart->seriesCount(), 2u);
    EXPECT_TRUE(chart->isAutoRange());
    EXPECT_EQ(chart->seriesLabel(0), "Receive");
    EXPECT_EQ(chart->seriesSamples(0)[0], 2048.0f);
    EXPECT_EQ(chart->seriesLabel(1), "Send");
    EXPECT_EQ(chart->seriesSamples(1)[0], 1024.0f);
}

TEST(NetworkPerformancePage, AdapterComesUp_ChartShowsHistoryRecordedWhileDown)
{
    NetworkPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    page.refresh();
    auto bothUp = populatedSnapshot();
    (*bothUp.networks)[1].operationalStatus = OperationalStatus::Up;

    page.recordSnapshot(bothUp);
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames(), (std::vector<std::string>{"{Ethernet}", "{Wi-Fi}"}));
    EXPECT_EQ(page.adapterChart(1)->seriesSamples(0).size(), 2u);
}

TEST(NetworkPerformancePage, FirstSampleWithoutRates_KeepsAdapterWithGap)
{
    NetworkPerformancePage page;
    SystemSnapshot snapshot;
    snapshot.networks = std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), std::nullopt, std::nullopt)};

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames().size(), 1u);
    ASSERT_EQ(page.adapterChart(0)->seriesSamples(0).size(), 1u);
    EXPECT_FALSE(page.adapterChart(0)->seriesSamples(0)[0].has_value());
}

TEST(NetworkPerformancePage, NoActiveAdapters_ShowsPlaceholder)
{
    NetworkPerformancePage page;
    auto snapshot = populatedSnapshot();
    (*snapshot.networks)[0].operationalStatus = OperationalStatus::Down;

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_TRUE(page.chartedAdapterNames().empty());
    EXPECT_EQ(page.placeholderText(), "No active network adapters");
}

TEST(NetworkPerformancePage, HiddenVirtualAdaptersUp_ChartsOnlyAdaptersThatCarryTraffic)
{
    NetworkPerformancePage page;
    auto snapshot = populatedSnapshot();
    NetworkSample tunnel = adapter(3, "Mullvad", OperationalStatus::Up, kVirtualAdapter);
    tunnel.ipAddresses = {{.text = "10.64.12.34"}};
    snapshot.networks->push_back(tunnel);
    for (const uint64_t interfaceId : {7, 8}) {
        NetworkSample miniport = withRates(adapter(interfaceId, std::format("Local Area Connection* {}", interfaceId),
                                                   OperationalStatus::Up, kUnboundVirtualAdapter),
                                           0, 0);
        miniport.description = "WAN Miniport (IP)";
        snapshot.networks->push_back(miniport);
    }

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_EQ(page.chartedAdapterNames(), (std::vector<std::string>{"{Ethernet}", "{Mullvad}"}));
}

TEST(NetworkPerformancePage, TwoAdaptersShareAName_SeparateChartsAndHistories)
{
    // Descriptions stand in for names when an alias is empty, and they repeat.
    const NetworkSample first = withRates(adapter(1, "WAN Miniport (IP)"), 100, 10);
    const NetworkSample second = withRates(adapter(2, "WAN Miniport (IP)"), 200, 20);
    SystemSnapshot snapshot;
    snapshot.networks = std::vector<NetworkSample>{first, second};
    NetworkPerformancePage page;

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedInterfaceIds(), (std::vector<uint64_t>{1, 2}));
    const auto firstReceive = page.adapterChart(0)->seriesSamples(0);
    const auto secondReceive = page.adapterChart(1)->seriesSamples(0);
    ASSERT_EQ(firstReceive.size(), 1u);
    ASSERT_EQ(secondReceive.size(), 1u);
    EXPECT_EQ(firstReceive[0], 100.0f);
    EXPECT_EQ(secondReceive[0], 200.0f);
}

TEST(NetworkPerformancePage, IdleAndBusyAdapters_PlotsLineUp)
{
    NetworkPerformancePage page;
    auto snapshot = populatedSnapshot();
    (*snapshot.networks)[0].inBytesPerSec = 0;
    (*snapshot.networks)[0].outBytesPerSec = 0;
    (*snapshot.networks)[1] = withRates(adapter(2, "Wi-Fi"), 12'000'000, 900'000);

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames().size(), 2u);
    EXPECT_DOUBLE_EQ(page.adapterChart(0)->plotArea().left(), page.adapterChart(1)->plotArea().left());
}

TEST(NetworkPerformancePage, AdapterRenamed_KeepsChartAndHistory)
{
    NetworkPerformancePage page;
    auto snapshot = populatedSnapshot();
    page.recordSnapshot(snapshot);
    page.refresh();
    const auto* chart = page.adapterChart(0);
    (*snapshot.networks)[0].adapterName = "{Office LAN}";
    (*snapshot.networks)[0].friendlyName = "Office LAN";

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames(), (std::vector<std::string>{"{Office LAN}"}));
    EXPECT_EQ(page.adapterChart(0), chart);
    EXPECT_EQ(page.adapterTitle(0), "Office LAN");
    EXPECT_EQ(page.adapterChart(0)->seriesSamples(0).size(), 2u);
}

TEST(NetworkPerformancePage, MarkupInAdapterName_TitleShownAsPlainText)
{
    NetworkPerformancePage page;
    SystemSnapshot snapshot;
    snapshot.networks = std::vector<NetworkSample>{adapter(1, "<h1>Ethernet</h1>")};

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames().size(), 1u);
    EXPECT_EQ(page.adapterTitle(0), "<h1>Ethernet</h1>");
    const auto labels = page.findChildren<QLabel*>();
    const auto title = std::ranges::find(labels, QString("<h1>Ethernet</h1>"), &QLabel::text);
    ASSERT_NE(title, labels.end());
    EXPECT_EQ((*title)->textFormat(), Qt::PlainText);
}

TEST(NetworkPerformancePage, ReadFails_KeepsChartsAndRecordsGap)
{
    NetworkPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    page.recordSnapshot(SystemSnapshot{});
    page.refresh();

    ASSERT_EQ(page.chartedAdapterNames().size(), 1u);
    const auto receive = page.adapterChart(0)->seriesSamples(0);
    ASSERT_EQ(receive.size(), 2u);
    EXPECT_FALSE(receive[1].has_value());
    EXPECT_EQ(page.summary(), networkSummary(std::nullopt));
}

TEST(MainWindow, OnSnapshotReady_ForwardsToPerformanceView)
{
    MainWindow mainWindow;

    mainWindow.onSnapshotReady(populatedSnapshot());

    EXPECT_EQ(mainWindow.performanceView()->cpuPage()->recordedCount(), 1u);
}

TEST(MainWindow, DemandingSnapshot_MinimumSizeStaysWithin800x600)
{
    SmallMainWindow mainWindow;
    auto* view = mainWindow.performanceView();

    view->updateSnapshot(demandingSnapshot());
    view->setCurrentPage(PerformanceView::Page::Network);
    QCoreApplication::processEvents();

    ASSERT_EQ(view->cpuPage()->coreChartCount(), 256u);
    ASSERT_EQ(view->networkPage()->chartedAdapterNames().size(), 3u);
    const QSize minimum = mainWindow.minimumSizeHint();
    EXPECT_LE(minimum.width(), 800);
    EXPECT_LE(minimum.height(), 600);
    EXPECT_EQ(mainWindow.size(), QSize(800, 600));
}

TEST(CpuPerformancePage, SmallWindowWith256Cores_CoreGridFitsItsWidth)
{
    SmallMainWindow mainWindow;
    auto* view = mainWindow.performanceView();

    view->updateSnapshot(demandingSnapshot());
    QCoreApplication::processEvents();

    const auto* scrollArea = view->cpuPage()->findChild<QScrollArea*>();
    ASSERT_NE(scrollArea, nullptr);
    ASSERT_NE(scrollArea->widget(), nullptr);
    ASSERT_EQ(view->cpuPage()->coreChartCount(), 256u);
    // The scroll area squeezes a grid that is too wide rather than scrolling it.
    EXPECT_LE(scrollArea->widget()->minimumSizeHint().width(), scrollArea->viewport()->width());
    EXPECT_LE(view->cpuPage()->readouts()->minimumSizeHint().width(), view->cpuPage()->width());
}

TEST(NetworkPerformancePage, SmallWindow_ReadoutsFitWithoutHorizontalScrolling)
{
    SmallMainWindow mainWindow;
    auto* view = mainWindow.performanceView();
    view->setCurrentPage(PerformanceView::Page::Network);

    view->updateSnapshot(demandingSnapshot());
    QCoreApplication::processEvents();

    const auto* scrollArea = view->networkPage()->findChild<QScrollArea*>();
    ASSERT_NE(scrollArea, nullptr);
    ASSERT_EQ(view->networkPage()->chartedAdapterNames().size(), 3u);
    // With horizontal scrolling off, the scroll area squeezes content that is
    // too wide instead of scrolling it, so check the content's minimum width
    // and that no label is narrower than its text.
    EXPECT_EQ(scrollArea->horizontalScrollBar()->maximum(), 0);
    EXPECT_LE(scrollArea->widget()->minimumSizeHint().width(), scrollArea->viewport()->width());
    EXPECT_LE(scrollArea->widget()->width(), scrollArea->viewport()->width());
    for (std::size_t index = 0; index < 3; ++index) {
        const ReadoutGrid* grid = view->networkPage()->adapterReadoutGrid(index);
        EXPECT_LE(grid->mapTo(scrollArea->viewport(), QPoint(grid->width(), 0)).x(), scrollArea->viewport()->width());
        expectLabelsFit(*grid);
    }
}

TEST(CpuPerformancePage, Readouts_PlaceholdersUntilFirstSnapshotThenValues)
{
    CpuPerformancePage page;
    EXPECT_EQ(page.readouts()->readouts(), placeholderReadouts(cpuReadouts(std::nullopt, std::nullopt, std::nullopt)));
    auto snapshot = populatedSnapshot();
    snapshot.activity = SystemActivitySample{.processCount = 367, .threadCount = 8350, .handleCount = 200'157};
    snapshot.uptime = std::chrono::minutes{12};

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_EQ(page.readouts()->readouts(), cpuReadouts(snapshot.cpu, snapshot.activity, snapshot.uptime));
}

TEST(MemoryPerformancePage, Readouts_ShowLatestSample)
{
    MemoryPerformancePage page;
    const SystemSnapshot snapshot = populatedSnapshot();

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_EQ(page.readouts()->readouts(), memoryReadouts(snapshot.memory));
}

TEST(DiskPerformancePage, Readouts_ShowLatestSample)
{
    DiskPerformancePage page;
    const SystemSnapshot snapshot = populatedSnapshot();

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_EQ(page.readouts()->readouts(), diskReadouts(snapshot.disks));
}

TEST(NetworkPerformancePage, Readouts_PerChartedAdapter)
{
    NetworkPerformancePage page;
    const SystemSnapshot snapshot = populatedSnapshot();

    page.recordSnapshot(snapshot);
    page.refresh();

    ASSERT_EQ(page.chartedInterfaceIds(), (std::vector<uint64_t>{1}));
    EXPECT_EQ(page.adapterReadoutGrid(0)->readouts(), adapterReadouts(snapshot.networks->front()));
}

TEST(NetworkPerformancePage, ReadFails_ReadoutsNotAvailable)
{
    NetworkPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    page.recordSnapshot(SystemSnapshot{});
    page.refresh();

    EXPECT_EQ(page.adapterReadoutGrid(0)->readouts(), adapterReadouts(std::nullopt));
}

TEST(PerformanceView, HistoryWindow_DefaultsToOneMinuteAndOffersFiveAndThirty)
{
    PerformanceView view;

    EXPECT_EQ(view.historyWindow(), 60u);
    EXPECT_EQ(PerformanceView::kHistoryWindows, (std::array<std::size_t, 3>{60, 300, 1800}));
    EXPECT_EQ(PerformancePage::kHistoryCapacity, 1800u);
}

TEST(PerformanceView, SetHistoryWindow_ChartsShowThatManyNewestSamples)
{
    ShownPerformanceView view;
    auto snapshot = populatedSnapshot();
    for (int i = 0; i < 400; ++i) {
        snapshot.cpu->totalUsagePercent = static_cast<float>(i % 100);
        view.updateSnapshot(snapshot);
    }
    const auto* chart = view.cpuPage()->totalChart();
    EXPECT_EQ(chart->capacity(), 60u);
    EXPECT_EQ(chart->samples().size(), 60u);

    view.setHistoryWindow(300);

    EXPECT_EQ(chart->capacity(), 300u);
    ASSERT_EQ(chart->samples().size(), 300u);
    EXPECT_EQ(chart->samples().back(), 99.0f); // Sample 399.

    view.setHistoryWindow(1800);

    EXPECT_EQ(chart->capacity(), 1800u);
    EXPECT_EQ(chart->samples().size(), 400u); // All recorded so far.
    EXPECT_EQ(view.cpuPage()->coreChart(0)->capacity(), 1800u);
}

TEST(PerformanceView, HistoryWindow_AutoRangeFollowsVisibleSamplesOnly)
{
    ShownPerformanceView view;
    view.setCurrentPage(PerformanceView::Page::Network);
    auto snapshot = populatedSnapshot();
    (*snapshot.networks)[0].inBytesPerSec = 1'000'000; // An old spike...
    view.updateSnapshot(snapshot);
    (*snapshot.networks)[0].inBytesPerSec = 1000;
    for (int i = 0; i < 100; ++i) {
        view.updateSnapshot(snapshot); // ...followed by 100 quiet samples.
    }
    const auto* chart = view.networkPage()->adapterChart(0);
    EXPECT_LT(chart->effectiveRange().max, 10'000.0f);

    view.setHistoryWindow(300);

    EXPECT_GT(chart->effectiveRange().max, 1'000'000.0f);
}

TEST(PerformanceView, SetSampleInterval_SelectorAndChartsTellTheTimeTheSamplesCover)
{
    ShownPerformanceView view;
    view.updateSnapshot(populatedSnapshot());
    constexpr std::chrono::seconds kInterval{2};

    view.setSampleInterval(kInterval);

    EXPECT_EQ(view.sampleInterval(), kInterval);
    const QComboBox* selector = view.historyWindowSelector();
    ASSERT_EQ(selector->count(), 3);
    EXPECT_EQ(selector->itemText(0), charts::formatHistoryWindow(std::chrono::minutes{2}));
    EXPECT_EQ(selector->itemText(1), charts::formatHistoryWindow(std::chrono::minutes{10}));
    EXPECT_EQ(selector->itemText(2), charts::formatHistoryWindow(std::chrono::hours{1}));
    EXPECT_EQ(view.cpuPage()->totalChart()->sampleInterval(), kInterval);
    EXPECT_EQ(view.cpuPage()->coreChart(0)->sampleInterval(), kInterval);
    // A page that is not shown gives its charts the interval when it is.
    view.setCurrentPage(PerformanceView::Page::Network);
    EXPECT_EQ(view.networkPage()->adapterChart(0)->sampleInterval(), kInterval);
}
