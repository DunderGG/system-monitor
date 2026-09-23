#include "ui/main_window.h"

#include <QTabWidget>

#include "ui/dashboard_view.h"
#include "ui/network_view.h"
#include "ui/performance_view.h"
#include "ui/processes_view.h"

namespace sysmon::ui
{

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("System Monitor");
    resize(800, 600);

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
}

QTabWidget *MainWindow::tabWidget() const
{
    return m_tabWidget;
}

DashboardView *MainWindow::dashboardView() const
{
    return m_dashboardView;
}

PerformanceView *MainWindow::performanceView() const
{
    return m_performanceView;
}

ProcessesView *MainWindow::processesView() const
{
    return m_processesView;
}

NetworkView *MainWindow::networkView() const
{
    return m_networkView;
}

void MainWindow::onSnapshotReady(const sysmon::domain::SystemSnapshot &snapshot)
{
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
}

} // namespace sysmon::ui
