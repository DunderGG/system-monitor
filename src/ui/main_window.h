#pragma once

#include <QMainWindow>
#include <QString>

#include "domain/settings.h"
#include "domain/system_snapshot.h"

class QAction;
class QTabWidget;

namespace verdandi::ui
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
 * accepted with settings that differ from settings(), the window applies them
 * as setSettings() does and emits settingsChanged(), whose receiver saves
 * them and applies the rest: the window applies only what the views show.
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

    /**
     * Sets the settings that the Settings dialog opens with, and applies what
     * the views show of them: the dashboard cards shown, the units of sizes
     * and rates, and the refresh interval, from which the charts tell the time
     * they cover.
     */
    void setSettings(const domain::Settings& settings);
    [[nodiscard]] const domain::Settings& settings() const;

    /**
     * Says in a window-modal message box that the settings could not be saved
     * to the file at path, and why (reason), and that they apply until the
     * monitor closes. Both are shown as plain text.
     */
    void showSettingsNotSaved(const QString& path, const QString& reason);

    /**
     * Says in a window-modal message box that the log cannot be written to
     * the file at path, and why (reason), and that the monitor runs without
     * it until it starts again. Both are shown as plain text.
     */
    void showLogNotWritten(const QString& path, const QString& reason);

signals:
    /** Emitted when the Settings dialog is accepted with settings that differ from the previous settings(). */
    void settingsChanged(const verdandi::domain::Settings& settings);

public slots:
    /** Slot receiving immutable system snapshots emitted from the sampling scheduler. */
    void onSnapshotReady(const verdandi::domain::SystemSnapshot& snapshot);

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

} // namespace verdandi::ui
