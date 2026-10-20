#include "ui/main_window.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QTabWidget>

#include "ui/dashboard_view.h"
#include "ui/network_view.h"
#include "ui/performance_view.h"
#include "ui/processes_view.h"
#include "ui/settings_dialog.h"

namespace sysmon::ui
{

namespace
{

// Wide enough for several readout columns on the Performance pages. The
// window still fits its content at 800x600.
constexpr QSize kDefaultSize{1024, 720};
// The largest share of the screen's available area the window opens at.
constexpr double kMaxScreenShare = 0.9;

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("System Monitor");
    QSize size = kDefaultSize;
    if (const QScreen* current = screen()) {
        size = size.boundedTo(current->availableGeometry().size() * kMaxScreenShare);
    }
    resize(size);

    m_tabWidget = new QTabWidget(this);

    m_dashboardView = new DashboardView(this);
    m_performanceView = new PerformanceView(this);
    m_processesView = new ProcessesView(this);
    m_networkView = new NetworkView(this);

    m_tabWidget->addTab(m_dashboardView, "Dashboard");
    m_tabWidget->addTab(m_performanceView, "Performance");
    m_tabWidget->addTab(m_processesView, "Processes");
    m_tabWidget->addTab(m_networkView, "Network");

    setCentralWidget(m_tabWidget);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_settingsAction = fileMenu->addAction("&Settings...");
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit");
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
}

QTabWidget* MainWindow::tabWidget() const
{
    return m_tabWidget;
}

DashboardView* MainWindow::dashboardView() const
{
    return m_dashboardView;
}

PerformanceView* MainWindow::performanceView() const
{
    return m_performanceView;
}

ProcessesView* MainWindow::processesView() const
{
    return m_processesView;
}

NetworkView* MainWindow::networkView() const
{
    return m_networkView;
}

QAction* MainWindow::settingsAction() const
{
    return m_settingsAction;
}

QAction* MainWindow::exitAction() const
{
    return m_exitAction;
}

void MainWindow::setSettings(const domain::Settings& settings)
{
    m_settings = settings;
    m_performanceView->setSampleInterval(settings.refreshInterval);
    m_networkView->setSampleInterval(settings.refreshInterval);
}

const domain::Settings& MainWindow::settings() const
{
    return m_settings;
}

void MainWindow::openSettingsDialog()
{
    auto* dialog = new SettingsDialog(m_settings, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const domain::Settings edited = dialog->settings();
        if (edited != m_settings) {
            setSettings(edited);
            emit settingsChanged(m_settings);
        }
    });
    dialog->open();
}

void MainWindow::onSnapshotReady(const sysmon::domain::SystemSnapshot& snapshot)
{
    // Every view records every snapshot so its history has no holes when it is
    // shown; each view skips repainting its own widgets while it is hidden.
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
    if (m_performanceView != nullptr) {
        m_performanceView->updateSnapshot(snapshot);
    }
    if (m_processesView != nullptr) {
        m_processesView->updateSnapshot(snapshot);
    }
    if (m_networkView != nullptr) {
        m_networkView->updateSnapshot(snapshot);
    }
}

} // namespace sysmon::ui
