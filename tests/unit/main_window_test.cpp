#include <chrono>
#include <memory>
#include <QCoreApplication>
#include <QTabWidget>
#include <thread>

#include <gtest/gtest.h>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "monitoring/synthetic_cpu_collector.h"
#include "monitoring/synthetic_memory_collector.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/resource_card.h"

using sysmon::monitoring::SamplingScheduler;
using sysmon::monitoring::SyntheticCpuCollector;
using sysmon::monitoring::SyntheticMemoryCollector;
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

TEST(MainWindow, SnapshotReadyViaQueuedConnection_UpdatesDashboardOnUiThread)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{20});
    scheduler.setCpuCollector(std::make_unique<SyntheticCpuCollector>(4, 55.0f));
    scheduler.setMemoryCollector(std::make_unique<SyntheticMemoryCollector>());

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

    const auto *dashboard = mainWindow.dashboardView();
    EXPECT_NE(dashboard->cpuCard()->valueText(), "--");
    EXPECT_EQ(dashboard->cpuCard()->detailText(), "4 cores");
    EXPECT_NE(dashboard->memoryCard()->valueText(), "--");
}
