#pragma once

#include <QString>
#include <QWidget>

#include "domain/system_snapshot.h"

class QLabel;
class QTreeView;

namespace sysmon::ui
{

class ProcessModel;

/**
 * Processes tab: a summary line above a table of the running processes, shown
 * in a QTreeView over a ProcessModel.
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
    [[nodiscard]] QTreeView* treeView() const;

    /** Returns the summary line above the table, e.g. "Processes: 412". */
    [[nodiscard]] QString summaryText() const;

private:
    ProcessModel* m_model{nullptr};
    QTreeView* m_treeView{nullptr};
    QLabel* m_summaryLabel{nullptr};
    bool m_hasProcessList{false};
};

} // namespace sysmon::ui
