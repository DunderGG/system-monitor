#pragma once

#include <QMainWindow>

#include "domain/settings.h"
#include "domain/system_snapshot.h"

class QAction;
class QTabWidget;

namespace sysmon::ui
{

class DashboardView;
class PerformanceView;
class ProcessesView;
class NetworkView;

/**
 * Main application window providing a tabbed layout across system monitor views.
 *
 * Its File menu has Settings..., which opens a window-modal SettingsDialog
 * with settings(), and Exit, which closes the window. When the dialog is
 * accepted with settings that differ from settings(), they become settings()
 * and settingsChanged() is emitted. The window applies none of them itself.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    [[nodiscard]] QTabWidget* tabWidget() const;
    [[nodiscard]] DashboardView* dashboardView() const;
    [[nodiscard]] PerformanceView* performanceView() const;
    [[nodiscard]] ProcessesView* processesView() const;
    [[nodiscard]] NetworkView* networkView() const;
    [[nodiscard]] QAction* settingsAction() const;
    [[nodiscard]] QAction* exitAction() const;

    /** Sets the settings that the Settings dialog opens with. */
    void setSettings(const domain::Settings& settings);
    [[nodiscard]] const domain::Settings& settings() const;

signals:
    /** Emitted when the Settings dialog is accepted with settings that differ from the previous settings(). */
    void settingsChanged(const sysmon::domain::Settings& settings);

public slots:
    /** Slot receiving immutable system snapshots emitted from the sampling scheduler. */
    void onSnapshotReady(const sysmon::domain::SystemSnapshot& snapshot);

private:
    void openSettingsDialog();

    QTabWidget* m_tabWidget{nullptr};
    DashboardView* m_dashboardView{nullptr};
    PerformanceView* m_performanceView{nullptr};
    ProcessesView* m_processesView{nullptr};
    NetworkView* m_networkView{nullptr};
    QAction* m_settingsAction{nullptr};
    QAction* m_exitAction{nullptr};
    domain::Settings m_settings{};
};

} // namespace sysmon::ui
