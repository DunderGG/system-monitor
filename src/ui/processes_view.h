#pragma once

#include <QString>
#include <QWidget>

#include "domain/system_snapshot.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTreeView;

namespace sysmon::ui
{

class ProcessModel;
class ProcessSortFilterProxyModel;

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
 * every few seconds. updateSnapshot() applies it to the model whether or not
 * the tab is shown, so the table is current whenever it is; the view only
 * repaints while visible. A snapshot without a list (before the first one, or
 * after a failed read) keeps the rows of the last list read.
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

    /** Returns the summary line above the table, e.g. "Processes: 412". */
    [[nodiscard]] QString summaryText() const;

private:
    void applyTreeMode(bool enabled);

    ProcessModel* m_model{nullptr};
    ProcessSortFilterProxyModel* m_proxyModel{nullptr};
    QTreeView* m_treeView{nullptr};
    QLineEdit* m_filterEdit{nullptr};
    QCheckBox* m_treeModeCheckBox{nullptr};
    QLabel* m_summaryLabel{nullptr};
    bool m_hasProcessList{false};
};

} // namespace sysmon::ui
