#pragma once

#include <QAbstractItemModel>
#include <QMetaObject>
#include <QModelIndex>
#include <QSortFilterProxyModel>

namespace verdandi::ui
{

/**
 * Sorts and filters a ProcessModel for display.
 *
 * Columns sort by the processes' values, not their display text, so "9.0%"
 * sorts below "10.0%". A missing value ("N/A") sorts below every value, so it
 * comes last when sorting largest first. Rows that compare equal fall back to
 * the name, ignoring ASCII case, then the PID and creation time, so rows with
 * equal values keep a stable order as the values change.
 *
 * setFilterFixedString() keeps the rows whose name or PID contains the text,
 * ignoring case. In tree mode a row is also kept when one of its descendants
 * matches, so a match is shown in its place in the tree; siblings sort among
 * themselves. The source model must be a ProcessModel; rows of any other
 * model are kept and compare equal.
 */
class ProcessSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ProcessSortFilterProxyModel(QObject* parent = nullptr);
    ~ProcessSortFilterProxyModel() override = default;

    /**
     * Shows sourceModel. With a ProcessModel, a refresh whose value changes
     * touch the sorted column under more than one parent is re-sorted once,
     * after all of them, rather than once per parent.
     */
    void setSourceModel(QAbstractItemModel* sourceModel) override;

protected:
    [[nodiscard]] bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void onValueChangesStarted(int firstColumn, int lastColumn, int signalCount);
    void onValueChangesEnded();

    QMetaObject::Connection m_valueChangesStartedConnection;
    QMetaObject::Connection m_valueChangesEndedConnection;
    bool m_isSortDeferred{false};
};

} // namespace verdandi::ui
