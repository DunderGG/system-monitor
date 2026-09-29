#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QString>

#include "domain/system_snapshot.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/cpu_performance_page.h"
#include "ui/disk_performance_page.h"
#include "ui/main_window.h"
#include "ui/memory_performance_page.h"
#include "ui/network_performance_page.h"
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
