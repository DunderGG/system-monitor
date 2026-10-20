#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QAction>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>

#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "support/fake_collectors.h"
#include "support/settings_samples.h"
#include "ui/dashboard_formatting.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/network_view.h"
#include "ui/performance_view.h"
#include "ui/process_model.h"
#include "ui/processes_view.h"
#include "ui/resource_card.h"
#include "ui/settings_dialog.h"

using sysmon::domain::Settings;
using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::FakeCpuCollector;
using sysmon::tests::FakeMemoryCollector;
using sysmon::tests::nonDefaultSettings;
using sysmon::tests::populatedSnapshot;
using sysmon::ui::formatPercent;
using sysmon::ui::MainWindow;
using sysmon::ui::SettingsDialog;

namespace
{

// Triggers Settings... and returns the dialog it opened, or nullptr if none
// did. A dialog that has closed stays a hidden child until its deferred
// deletion runs, which these tests never do.
SettingsDialog* openSettingsDialog(MainWindow& mainWindow)
{
    mainWindow.settingsAction()->trigger();
    for (auto* dialog : mainWindow.findChildren<SettingsDialog*>()) {
        if (dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

// Records each settingsChanged() of mainWindow in emitted.
void recordSettingsChanges(MainWindow& mainWindow, std::vector<Settings>& emitted)
{
    QObject::connect(&mainWindow, &MainWindow::settingsChanged,
                     [&emitted](const Settings& settings) { emitted.push_back(settings); });
}

} // namespace

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

TEST(MainWindow, SettingsAction_OpensWindowModalDialogWithTheSettings)
{
    MainWindow mainWindow;
    mainWindow.setSettings(nonDefaultSettings());

    const auto* dialog = openSettingsDialog(mainWindow);

    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->windowModality(), Qt::WindowModal);
    EXPECT_EQ(dialog->settings(), nonDefaultSettings());
}

TEST(MainWindow, SetSettings_ChartsTellTimeAtTheRefreshInterval)
{
    MainWindow mainWindow;

    mainWindow.setSettings(nonDefaultSettings());

    EXPECT_EQ(mainWindow.settings(), nonDefaultSettings());
    EXPECT_EQ(mainWindow.performanceView()->sampleInterval(), nonDefaultSettings().refreshInterval);
    EXPECT_EQ(mainWindow.networkView()->sampleInterval(), nonDefaultSettings().refreshInterval);
}

TEST(MainWindow, SettingsDialogAccepted_ChangedSettings_AppliesAndEmitsThem)
{
    MainWindow mainWindow;
    std::vector<Settings> emitted;
    recordSettingsChanges(mainWindow, emitted);
    auto* dialog = openSettingsDialog(mainWindow);
    ASSERT_NE(dialog, nullptr);

    dialog->setSettings(nonDefaultSettings());
    dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();

    EXPECT_EQ(mainWindow.settings(), nonDefaultSettings());
    EXPECT_EQ(mainWindow.performanceView()->sampleInterval(), nonDefaultSettings().refreshInterval);
    EXPECT_EQ(emitted, std::vector<Settings>{nonDefaultSettings()});
}

TEST(MainWindow, SettingsDialogAccepted_UnchangedSettings_EmitsNothing)
{
    MainWindow mainWindow;
    std::vector<Settings> emitted;
    recordSettingsChanges(mainWindow, emitted);
    auto* dialog = openSettingsDialog(mainWindow);
    ASSERT_NE(dialog, nullptr);

    dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();

    EXPECT_TRUE(emitted.empty());
}

TEST(MainWindow, SettingsDialogCancelled_KeepsTheSettingsAndEmitsNothing)
{
    MainWindow mainWindow;
    std::vector<Settings> emitted;
    recordSettingsChanges(mainWindow, emitted);
    auto* dialog = openSettingsDialog(mainWindow);
    ASSERT_NE(dialog, nullptr);

    dialog->setSettings(nonDefaultSettings());
    dialog->buttonBox()->button(QDialogButtonBox::Cancel)->click();

    EXPECT_EQ(mainWindow.settings(), Settings{});
    EXPECT_TRUE(emitted.empty());
}

TEST(MainWindow, ExitAction_ClosesTheWindow)
{
    MainWindow mainWindow;
    mainWindow.setAttribute(Qt::WA_DontShowOnScreen);
    mainWindow.show();

    mainWindow.exitAction()->trigger();

    EXPECT_FALSE(mainWindow.isVisible());
}
