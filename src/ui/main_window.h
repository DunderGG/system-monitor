#pragma once

#include <QMainWindow>

#include "domain/system_snapshot.h"

class QTabWidget;

namespace sysmon::ui
{

class DashboardView;
class PerformanceView;
class ProcessesView;
class NetworkView;

/**
 * Main application window providing a tabbed layout across system monitor views.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    [[nodiscard]] QTabWidget *tabWidget() const;
    [[nodiscard]] DashboardView *dashboardView() const;
    [[nodiscard]] PerformanceView *performanceView() const;
    [[nodiscard]] ProcessesView *processesView() const;
    [[nodiscard]] NetworkView *networkView() const;

public slots:
    /** Slot receiving immutable system snapshots emitted from the sampling scheduler. */
    void onSnapshotReady(const sysmon::domain::SystemSnapshot &snapshot);

private:
    QTabWidget *m_tabWidget{nullptr};
    DashboardView *m_dashboardView{nullptr};
    PerformanceView *m_performanceView{nullptr};
    ProcessesView *m_processesView{nullptr};
    NetworkView *m_networkView{nullptr};
};

} // namespace sysmon::ui
