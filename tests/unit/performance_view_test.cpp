#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QLabel>
#include <QPoint>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QTabWidget>

#include "domain/system_snapshot.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/cpu_performance_page.h"
#include "ui/disk_performance_page.h"
#include "ui/main_window.h"
#include "ui/memory_performance_page.h"
#include "ui/network_performance_page.h"
#include "ui/performance_page.h"
#include "ui/performance_view.h"
#include "ui/readout_grid.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

NetworkSample adapter(std::string name, OperationalStatus status, std::optional<uint64_t> inRate = 2048,
                      std::optional<uint64_t> outRate = 1024)
{
    return NetworkSample{
        .adapterName = "{" + name + "}",
        .friendlyName = std::move(name),
        .inBytesPerSec = inRate,
        .outBytesPerSec = outRate,
        .operationalStatus = status,
        .isHardwareInterface = true,
    };
}

SystemSnapshot populatedSnapshot()
{
    SystemSnapshot snapshot;
    snapshot.cpu =
        CpuSample{.totalUsagePercent = 42.5f, .coreUsagePercents = {10.0f, 20.0f, 30.0f, 40.0f}, .coreCount = 4};
    snapshot.memory = MemorySample{
        .totalBytes = 16 * kGiB,
        .availableBytes = 8 * kGiB,
        .usagePercent = 50.0f,
        .commitLimit = 32 * kGiB,
        .commitCurrent = 8 * kGiB,
    };
    snapshot.disks = std::vector<DiskSample>{
        DiskSample{.volumeName = "D:\\", .totalBytes = 1000, .freeBytes = 900, .usagePercent = 10.0f},
        DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .freeBytes = 250, .usagePercent = 75.0f},
    };
    snapshot.networks = std::vector<NetworkSample>{
        adapter("Ethernet", OperationalStatus::Up),
        adapter("Wi-Fi", OperationalStatus::Down),
    };
    return snapshot;
}

// What the fullest real pages hold: 256 logical processors, and adapters with
// long descriptions, IPv6 addresses, and DNS server lists.
SystemSnapshot demandingSnapshot()
{
    auto snapshot = populatedSnapshot();
    snapshot.cpu->coreUsagePercents.assign(256, 50.0f);
    snapshot.cpu->coreCount = 256;

    NetworkSample wired = adapter("Ethernet", OperationalStatus::Up);
    wired.description = "Realtek Gaming 2.5GbE Family Controller #2";
    wired.linkSpeedBps = 2'500'000'000;
    wired.ipAddresses = {"192.168.1.23", "fd44:555c:2315:4176:c6d0:77e0:5d11:3727", "fe80::4a1c:9d2e:77b0:1f3a%12"};
    wired.dnsServers = {"fd44:555c:2315:4176:c6d0:77e0:5d11:1", "2001:4860:4860::8888", "192.168.1.1"};
    wired.inBytesTotal = 987'654'321'098;
    wired.outBytesTotal = 123'456'789'012;
    NetworkSample tunnel = adapter("Mullvad", OperationalStatus::Up);
    tunnel.description = "Mullvad Tunnel (WireGuard) Virtual Network Adapter";
    tunnel.isHardwareInterface = false;
    tunnel.ipAddresses = {"10.64.12.34", "fc00:bbbb:bbbb:bb01:d:0:1c:2f45"};
    tunnel.dnsServers = {"10.64.0.1"};
    snapshot.networks = std::vector<NetworkSample>{wired, tunnel};
    return snapshot;
}

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

    view.updateSnapshot(populatedSnapshot());

    ASSERT_EQ(view.cpuPage()->totalChart()->samples().size(), 1u);
    EXPECT_EQ(view.cpuPage()->totalChart()->samples()[0], 42.5f);
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n42.5%");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Memory), "Memory\n8.0/16.0 GiB (50.0%)");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Disk), "Disk\nC:\\ 75.0% used");
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Network), "Network\nReceive 2.0 KiB/s\nSend 1.0 KiB/s");
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
    EXPECT_EQ(view.sidebarText(PerformanceView::Page::Cpu), "CPU\n42.5%");
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
    EXPECT_EQ(page.summary(), "N/A");
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
    snapshot.networks =
        std::vector<NetworkSample>{adapter("Ethernet", OperationalStatus::Up, std::nullopt, std::nullopt)};

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
    EXPECT_EQ(page.summary(), "N/A");
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
    ASSERT_EQ(view->networkPage()->chartedAdapterNames().size(), 2u);
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
    ASSERT_EQ(view->networkPage()->chartedAdapterNames().size(), 2u);
    // With horizontal scrolling off, the scroll area squeezes content that is
    // too wide instead of scrolling it, so check the content's minimum width
    // and that no label is narrower than its text.
    EXPECT_EQ(scrollArea->horizontalScrollBar()->maximum(), 0);
    EXPECT_LE(scrollArea->widget()->minimumSizeHint().width(), scrollArea->viewport()->width());
    EXPECT_LE(scrollArea->widget()->width(), scrollArea->viewport()->width());
    for (std::size_t index = 0; index < 2; ++index) {
        const ReadoutGrid* grid = view->networkPage()->adapterReadoutGrid(index);
        EXPECT_LE(grid->mapTo(scrollArea->viewport(), QPoint(grid->width(), 0)).x(), scrollArea->viewport()->width());
        for (const auto* label : grid->findChildren<QLabel*>()) {
            EXPECT_GE(label->width(), label->sizeHint().width()) << label->text().toStdString();
            EXPECT_LE(label->x() + label->width(), grid->width()) << label->text().toStdString();
        }
    }
}

TEST(CpuPerformancePage, Readouts_PlaceholdersUntilFirstSnapshotThenValues)
{
    CpuPerformancePage page;
    EXPECT_EQ(page.readouts()->value("Handles"), "--");
    auto snapshot = populatedSnapshot();
    snapshot.activity = SystemActivitySample{.processCount = 367, .threadCount = 8350, .handleCount = 200'157};
    snapshot.uptime = std::chrono::minutes{12};

    page.recordSnapshot(snapshot);
    page.refresh();

    EXPECT_EQ(page.readouts()->value("Utilization"), "42.5%");
    EXPECT_EQ(page.readouts()->value("Handles"), "200,157");
    EXPECT_EQ(page.readouts()->value("Up time"), "12m 0s");
}

TEST(MemoryPerformancePage, Readouts_ShowLatestSample)
{
    MemoryPerformancePage page;

    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    EXPECT_EQ(page.readouts()->value("Committed"), "8.0/32.0 GiB");
    EXPECT_EQ(page.readouts()->value("Cached"), "N/A");
}

TEST(DiskPerformancePage, Readouts_OnePerVolume)
{
    DiskPerformancePage page;

    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    EXPECT_EQ(page.readouts()->readouts().size(), 2u);
    EXPECT_EQ(page.readouts()->value("C:\\"), "750/1000 B used (75.0%)\n250 B free");
}

TEST(NetworkPerformancePage, Readouts_PerChartedAdapter)
{
    NetworkPerformancePage page;

    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    EXPECT_EQ(page.adapterReadoutGrid(0)->value("Receive"), "2.0 KiB/s");
    EXPECT_EQ(page.adapterReadoutGrid(0)->value("Type"), "Hardware");
}

TEST(NetworkPerformancePage, ReadFails_ReadoutsNotAvailable)
{
    NetworkPerformancePage page;
    page.recordSnapshot(populatedSnapshot());
    page.refresh();

    page.recordSnapshot(SystemSnapshot{});
    page.refresh();

    EXPECT_EQ(page.adapterReadoutGrid(0)->value("Receive"), "N/A");
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
