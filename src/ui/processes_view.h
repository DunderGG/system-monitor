#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "domain/process_termination.h"
#include "domain/system_snapshot.h"

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeView;

namespace sysmon::ui
{

class ProcessModel;
class ProcessSortFilterProxyModel;

/**
 * Ends the process that a ProcessInfo identifies by (pid, creationTime) and
 * says what happened. main.cpp passes platform::terminateOwnedProcess(),
 * which the UI cannot call itself.
 */
using ProcessTerminator = std::function<domain::ProcessTerminationResult(const domain::ProcessInfo&)>;

/**
 * Processes tab: a filter box, a "Show as tree" check box, and a summary line
 * above a table of the running processes. The table is a QTreeView showing a
 * ProcessModel through a ProcessSortFilterProxyModel; clicking a column header
 * sorts by it, and the filter box keeps the processes whose name or PID
 * contains its text. The table starts as a flat list sorted by name.
 *
 * In tree mode every branch is expanded when the mode is switched on and when
 * the filter changes, and a process is expanded when it gains its first
 * child; a branch the user collapsed otherwise stays collapsed. The selected
 * process stays selected when the mode changes.
 *
 * Every snapshot carries the scheduler's latest process list, which refreshes
 * every few seconds. updateSnapshot() applies it to the model at the
 * snapshot's timestamp whether or not the tab is shown, so the table is
 * current whenever it is; the view only repaints while visible. A snapshot
 * without a list (before the first one, or after a failed read) keeps the
 * rows of the last list read, but still ends highlights that are due.
 *
 * The End process action ends the selected process once the user confirms
 * it in a dialog that names the process and its PID and warns that unsaved
 * data is lost; Cancel is the dialog's default. The toolbar's End process
 * button, the table's context menu, and the Delete key (while the table has
 * focus) all trigger it. It is enabled while a terminator is set and a
 * process is selected whose row is not marked as exited, and it is checked
 * again after every snapshot. The dialog keeps a copy of the process it
 * names, so confirming ends that process, by (PID, creation time), even if
 * the selection has moved or the process has left the list since it opened.
 * Once the dialog has closed, a message says why the process was left
 * running, or that it had already exited. A process that was ended gets no
 * message; its row is marked as exited when the next list no longer has it.
 *
 * For the monitor's own row (setMonitorIdentity()), the dialog offers to
 * close Verdandi instead, and confirming closes the window that holds the
 * view, so the monitor shuts down the same way as when the user closes it.
 */
class ProcessesView : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessesView(QWidget* parent = nullptr);
    ~ProcessesView() override = default;

    /** Applies the snapshot's process list, if it has one, and updates the summary line. */
    void updateSnapshot(const domain::SystemSnapshot& snapshot);

    /** Shows the processes as a tree (true) or a flat list (false), as the check box does. */
    void setTreeMode(bool enabled);
    [[nodiscard]] bool isTreeMode() const;

    [[nodiscard]] ProcessModel* model() const;
    [[nodiscard]] ProcessSortFilterProxyModel* proxyModel() const;
    [[nodiscard]] QTreeView* treeView() const;
    [[nodiscard]] QLineEdit* filterEdit() const;
    [[nodiscard]] QCheckBox* treeModeCheckBox() const;
    [[nodiscard]] QAction* endProcessAction() const;
    [[nodiscard]] QPushButton* endProcessButton() const;

    /** Sets the function that ends a process once the user confirms. Without one, End process is disabled. */
    void setProcessTerminator(ProcessTerminator terminator);

    /**
     * Sets the identity of the monitor's own process, whose row End process
     * offers to close rather than end. Without one, its row is treated like
     * any other.
     */
    void setMonitorIdentity(const domain::ProcessIdentity& identity);

    /** Returns the summary line above the table, e.g. "Processes: 412". */
    [[nodiscard]] QString summaryText() const;

private:
    void applyTreeMode(bool enabled);
    [[nodiscard]] const domain::ProcessInfo* currentProcess() const;
    // The current process, or nullptr if there is none or its row is marked as exited.
    [[nodiscard]] const domain::ProcessInfo* currentRunningProcess() const;
    void updateEndProcessAction();
    void confirmEndProcess();

    ProcessModel* m_model{nullptr};
    ProcessSortFilterProxyModel* m_proxyModel{nullptr};
    QTreeView* m_treeView{nullptr};
    QLineEdit* m_filterEdit{nullptr};
    QCheckBox* m_treeModeCheckBox{nullptr};
    QLabel* m_summaryLabel{nullptr};
    QAction* m_endProcessAction{nullptr};
    QPushButton* m_endProcessButton{nullptr};
    ProcessTerminator m_terminator;
    std::optional<domain::ProcessIdentity> m_monitorIdentity;
    bool m_hasProcessList{false};
};

} // namespace sysmon::ui
