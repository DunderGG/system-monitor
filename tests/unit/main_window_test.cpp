#include <chrono>
#include <memory>
#include <thread>

#include <QCoreApplication>
#include <QTabWidget>
#include <gtest/gtest.h>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "monitoring/synthetic_cpu_collector.h"
#include "monitoring/synthetic_memory_collector.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"

using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::monitoring::SyntheticCpuCollector;
using sysmon::monitoring::SyntheticMemoryCollector;
using sysmon::ui::DashboardView;
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

TEST(DashboardView, UpdateSnapshot_UpdatesCpuAndMemoryLabels)
{
    DashboardView dashboard;

    EXPECT_TRUE(dashboard.cpuText().contains("--"));
    EXPECT_TRUE(dashboard.memoryText().contains("--"));

    SystemSnapshot snapshot;
    snapshot.cpu.totalUsagePercent = 42.5f;
    snapshot.cpu.coreCount = 8;
    snapshot.memory.totalBytes = 34'359'738'368ULL;     // 32 GiB
    snapshot.memory.availableBytes = 17'179'869'184ULL; // 16 GiB
    snapshot.memory.usagePercent = 50.0f;

    dashboard.updateSnapshot(snapshot);

    EXPECT_TRUE(dashboard.cpuText().contains("42.5%"));
    EXPECT_TRUE(dashboard.cpuText().contains("8 cores"));
    EXPECT_TRUE(dashboard.memoryText().contains("50.0%"));
    EXPECT_TRUE(dashboard.memoryText().contains("32.0 GiB"));
}

TEST(MainWindow, SnapshotReadyViaQueuedConnection_UpdatesDashboardOnUiThread)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{20});
    scheduler.setCpuCollector(std::make_unique<SyntheticCpuCollector>(4, 55.0f));
    scheduler.setMemoryCollector(std::make_unique<SyntheticMemoryCollector>());

    MainWindow mainWindow;

    QObject::connect(
        &scheduler, &SamplingScheduler::snapshotReady,
        &mainWindow, &MainWindow::onSnapshotReady,
        Qt::QueuedConnection);

    scheduler.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{1000};
    while (mainWindow.dashboardView()->cpuText().contains("--") && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    EXPECT_FALSE(mainWindow.dashboardView()->cpuText().contains("--"));
    EXPECT_TRUE(mainWindow.dashboardView()->cpuText().contains("4 cores"));
    EXPECT_FALSE(mainWindow.dashboardView()->memoryText().contains("--"));
}

