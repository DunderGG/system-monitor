#include "ui/process_sort_filter_proxy_model.h"

#include <algorithm>
#include <compare>
#include <string_view>

#include <QRegularExpression>
#include <QString>

#include "domain/process_info.h"
#include "ui/process_formatting.h"
#include "ui/process_model.h"

namespace sysmon::ui
{

namespace
{

using Column = ProcessModel::Column;

const domain::ProcessInfo* processAt(const QModelIndex& sourceIndex)
{
    const auto* model = qobject_cast<const ProcessModel*>(sourceIndex.model());
    return model != nullptr ? model->process(sourceIndex) : nullptr;
}

std::string_view sortName(const domain::ProcessInfo& process)
{
    if (process.imageName.empty() && process.pid == 0) {
        return kIdleProcessName;
    }
    return process.imageName;
}

unsigned char foldAsciiCase(char character)
{
    const auto byte = static_cast<unsigned char>(character);
    return byte >= 'A' && byte <= 'Z' ? static_cast<unsigned char>(byte - 'A' + 'a') : byte;
}

// Orders UTF-8 names ignoring ASCII case, which covers almost every image
// name. Other bytes compare by value: a consistent order, if not an
// alphabetical one. Unlike QString comparison, it allocates nothing, and
// sorting by any column compares names to break ties.
std::weak_ordering compareNames(std::string_view left, std::string_view right)
{
    return std::lexicographical_compare_three_way(
        left.begin(), left.end(), right.begin(), right.end(),
        [](char a, char b) -> std::weak_ordering { return foldAsciiCase(a) <=> foldAsciiCase(b); });
}

// Orders two processes by one column's value. std::optional orders a missing
// value before every value.
std::partial_ordering compareColumn(const domain::ProcessInfo& left, const domain::ProcessInfo& right, Column column)
{
    switch (column) {
        case Column::Name:
            return compareNames(sortName(left), sortName(right));
        case Column::Pid:
            return left.pid <=> right.pid;
        case Column::Cpu:
            return left.cpuUsagePercent <=> right.cpuUsagePercent;
        case Column::Memory:
            return left.workingSetBytes <=> right.workingSetBytes;
        case Column::Io:
            return processIoBytesPerSec(left) <=> processIoBytesPerSec(right);
        case Column::Network:
            return std::partial_ordering::equivalent;
        case Column::Threads:
            return left.threadCount <=> right.threadCount;
        case Column::Handles:
            return left.handleCount <=> right.handleCount;
    }
    return std::partial_ordering::equivalent;
}

} // namespace

ProcessSortFilterProxyModel::ProcessSortFilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool ProcessSortFilterProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
    const domain::ProcessInfo* left = processAt(sourceLeft);
    const domain::ProcessInfo* right = processAt(sourceRight);
    if (left == nullptr || right == nullptr) {
        return false;
    }

    if (const auto byColumn = compareColumn(*left, *right, static_cast<Column>(sourceLeft.column())); byColumn != 0) {
        return byColumn < 0;
    }
    if (const auto byName = compareNames(sortName(*left), sortName(*right)); byName != 0) {
        return byName < 0;
    }
    if (left->pid != right->pid) {
        return left->pid < right->pid;
    }
    return left->creationTime < right->creationTime;
}

bool ProcessSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QRegularExpression expression = filterRegularExpression();
    if (expression.pattern().isEmpty()) {
        return true;
    }

    const domain::ProcessInfo* process = processAt(sourceModel()->index(sourceRow, 0, sourceParent));
    if (process == nullptr) {
        return true;
    }
    return expression.match(processDisplayName(*process)).hasMatch() ||
           expression.match(QString::number(process->pid)).hasMatch();
}

} // namespace sysmon::ui
