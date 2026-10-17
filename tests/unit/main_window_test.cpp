#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTabWidget>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "support/fake_collectors.h"
#include "ui/dashboard_formatting.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/network_view.h"
#include "ui/process_model.h"
#include "ui/processes_view.h"
#include "ui/resource_card.h"

using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::FakeCpuCollector;
using sysmon::tests::FakeMemoryCollector;
using sysmon::tests::populatedSnapshot;
using sysmon::ui::formatPercent;
using sysmon::ui::MainWindow;

TEST(MainWindow, Construction_InitializesFourTabsInExpectedOrder)
{
    MainWindow mainWindow;

    ASSERT_NE(mainWindow.tabWidget(), nullptr);
    EXPECT_EQ(mainWindow.tabWidget()->count(), 4);

    EXPECT_EQ(mainWindow.tabWidget()->tabText(0), "Dashboard");
    EXPECT_EQ(mainWindow.tabWidget()->tabText(1), "Performance");
    EXPECT_EQ(mainWindow.tabWidget()->tabText(2), "Processes");
    EXPECT_EQ(mainWindow.tabWidget()->tabText(3), "Network");

    EXPECT_NE(mainWindow.dashboardView(), nullptr);
    EXPECT_NE(mainWindow.performanceView(), nullptr);
    EXPECT_NE(mainWindow.processesView(), nullptr);
    EXPECT_NE(mainWindow.networkView(), nullptr);
}

TEST(MainWindow, OnSnapshotReady_ForwardsProcessListToProcessesView)
{
    MainWindow mainWindow;
    sysmon::domain::SystemSnapshot snapshot;
    snapshot.processes = std::vector<sysmon::domain::ProcessInfo>{
        sysmon::domain::ProcessInfo{.pid = 4, .imageName = "System"},
    };

    mainWindow.onSnapshotReady(snapshot);

    EXPECT_EQ(mainWindow.processesView()->model()->rowCount(), 1);
}

TEST(MainWindow, NetworkTabSelected_ListsAdaptersRecordedWhileHidden)
{
    MainWindow mainWindow;
    mainWindow.setAttribute(Qt::WA_DontShowOnScreen);
    mainWindow.show();
    sysmon::domain::SystemSnapshot snapshot;
    snapshot.networks = std::vector<sysmon::domain::NetworkSample>{
        sysmon::domain::NetworkSample{.interfaceId = 7, .friendlyName = "Ethernet", .isHardwareInterface = true},
    };

    mainWindow.onSnapshotReady(snapshot);

    EXPECT_TRUE(mainWindow.networkView()->listedInterfaceIds().empty());

    mainWindow.tabWidget()->setCurrentWidget(mainWindow.networkView());

    EXPECT_EQ(mainWindow.networkView()->listedInterfaceIds(), (std::vector<uint64_t>{7}));
}

TEST(MainWindow, SnapshotReadyViaQueuedConnection_UpdatesDashboardOnUiThread)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{20});
    scheduler.setCpuCollector(std::make_unique<FakeCpuCollector>());
    scheduler.setMemoryCollector(std::make_unique<FakeMemoryCollector>());

    MainWindow mainWindow;

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, &mainWindow, &MainWindow::onSnapshotReady,
                     Qt::QueuedConnection);

    scheduler.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{1000};
    while (mainWindow.dashboardView()->cpuCard()->valueText() == "--" && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    const auto* dashboard = mainWindow.dashboardView();
    EXPECT_EQ(dashboard->cpuCard()->valueText(), formatPercent(populatedSnapshot().cpu->totalUsagePercent));
    EXPECT_EQ(dashboard->memoryCard()->valueText(), formatPercent(populatedSnapshot().memory->usagePercent));
}
