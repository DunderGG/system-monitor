#pragma once

#include <QString>
#include <QWidget>

#include "domain/system_snapshot.h"

class QLabel;
class QLineEdit;
class QTreeView;

namespace sysmon::ui
{

class ProcessModel;
class ProcessSortFilterProxyModel;

/**
 * Processes tab: a filter box and a summary line above a table of the running
 * processes. The table is a QTreeView showing a ProcessModel through a
 * ProcessSortFilterProxyModel; clicking a column header sorts by it, and the
 * filter box keeps the processes whose name or PID contains its text. The
 * table starts sorted by name.
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

    [[nodiscard]] ProcessModel* model() const;
    [[nodiscard]] ProcessSortFilterProxyModel* proxyModel() const;
    [[nodiscard]] QTreeView* treeView() const;
    [[nodiscard]] QLineEdit* filterEdit() const;

    /** Returns the summary line above the table, e.g. "Processes: 412". */
    [[nodiscard]] QString summaryText() const;

private:
    ProcessModel* m_model{nullptr};
    ProcessSortFilterProxyModel* m_proxyModel{nullptr};
    QTreeView* m_treeView{nullptr};
    QLineEdit* m_filterEdit{nullptr};
    QLabel* m_summaryLabel{nullptr};
    bool m_hasProcessList{false};
};

} // namespace sysmon::ui
