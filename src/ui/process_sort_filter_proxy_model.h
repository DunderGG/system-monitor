#pragma once

#include <QModelIndex>
#include <QSortFilterProxyModel>

namespace sysmon::ui
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
 * ignoring case. The source model must be a ProcessModel; rows of any other
 * model are kept and compare equal.
 */
class ProcessSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ProcessSortFilterProxyModel(QObject* parent = nullptr);
    ~ProcessSortFilterProxyModel() override = default;

protected:
    [[nodiscard]] bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

} // namespace sysmon::ui
