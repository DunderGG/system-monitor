#include "ui/process_model.h"

#include <cstddef>
#include <optional>
#include <unordered_set>

#include <QGuiApplication>
#include <QPalette>
#include <QString>

#include "ui/dashboard_formatting.h"
#include "ui/process_formatting.h"

namespace sysmon::ui
{

namespace
{

using Column = ProcessModel::Column;

// The first column whose values can change while a process runs.
constexpr int kFirstValueColumn = static_cast<int>(Column::Cpu);

QString displayText(const domain::ProcessInfo& process, Column column)
{
    switch (column) {
        case Column::Name:
            return processDisplayName(process);
        case Column::Pid:
            return QString::number(process.pid);
        case Column::Cpu:
            return process.cpuUsagePercent ? formatPercent(*process.cpuUsagePercent) : kNotAvailable;
        case Column::Memory:
            return formatBytes(process.workingSetBytes);
        case Column::Io: {
            const auto bytesPerSec = processIoBytesPerSec(process);
            return bytesPerSec ? formatByteRate(*bytesPerSec) : kNotAvailable;
        }
        case Column::Network:
            return kNotAvailable;
        case Column::Threads:
            return formatCount(process.threadCount);
        case Column::Handles:
            return formatCount(process.handleCount);
    }
    return {};
}

// True when displayText() shows "N/A" for column.
bool isNotAvailable(const domain::ProcessInfo& process, Column column)
{
    switch (column) {
        case Column::Name:
            return process.imageName.empty() && process.pid != 0;
        case Column::Cpu:
            return !process.cpuUsagePercent.has_value();
        case Column::Io:
            return !processIoBytesPerSec(process).has_value();
        case Column::Network:
            return true;
        case Column::Pid:
        case Column::Memory:
        case Column::Threads:
        case Column::Handles:
            return false;
    }
    return false;
}

QString headerText(Column column)
{
    switch (column) {
        case Column::Name:
            return QStringLiteral("Name");
        case Column::Pid:
            return QStringLiteral("PID");
        case Column::Cpu:
            return QStringLiteral("CPU");
        case Column::Memory:
            return QStringLiteral("Memory");
        case Column::Io:
            return QStringLiteral("I/O");
        case Column::Network:
            return QStringLiteral("Network");
        case Column::Threads:
            return QStringLiteral("Threads");
        case Column::Handles:
            return QStringLiteral("Handles");
    }
    return {};
}

QString headerToolTip(Column column)
{
    switch (column) {
        case Column::Name:
            return QStringLiteral("Image name of the process");
        case Column::Pid:
            return QStringLiteral("Process ID");
        case Column::Cpu:
            return QStringLiteral("Share of all logical processors the process used since the previous refresh");
        case Column::Memory:
            return QStringLiteral("Working set: the physical memory the process is using");
        case Column::Io:
            return QStringLiteral("Bytes read and written per second, by all of the process's I/O: files, pipes, "
                                  "and devices, not only disk");
        case Column::Network:
            return QStringLiteral("Per-process network usage is not collected yet; it needs ETW");
        case Column::Threads:
            return QStringLiteral("Threads in the process");
        case Column::Handles:
            return QStringLiteral("Handles the process has open");
    }
    return {};
}

} // namespace

ProcessModel::ProcessModel(QObject* parent) : QAbstractItemModel(parent) {}

void ProcessModel::updateProcesses(const std::vector<domain::ProcessInfo>& processes)
{
    std::unordered_set<domain::ProcessIdentity, domain::ProcessIdentityHash> listed;
    listed.reserve(processes.size());
    for (const auto& process : processes) {
        listed.insert(process.identity());
    }

    // From the last row back, so the rows still to be checked keep their numbers.
    for (int row = static_cast<int>(m_rows.size()) - 1; row >= 0; --row) {
        if (!listed.contains(m_rows[static_cast<std::size_t>(row)]->process.identity())) {
            removeNodeAt(row);
        }
    }

    std::vector<std::unique_ptr<Node>> added;
    for (const auto& process : processes) {
        const auto identity = process.identity();
        if (const auto it = m_nodes.find(identity); it != m_nodes.end()) {
            it->second->process = process;
        } else if (listed.erase(identity) > 0) {
            // Erasing the identity keeps a duplicate entry in the list from adding a second row.
            added.push_back(std::make_unique<Node>(Node{.process = process}));
        }
    }
    if (!m_rows.empty()) {
        emit dataChanged(index(0, kFirstValueColumn), index(static_cast<int>(m_rows.size()) - 1, kColumnCount - 1));
    }
    if (added.empty()) {
        return;
    }

    const int first = static_cast<int>(m_rows.size());
    beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
    for (auto& node : added) {
        const auto identity = node->process.identity();
        node->row = static_cast<int>(m_rows.size());
        m_rows.push_back(node.get());
        m_nodes.insert_or_assign(identity, std::move(node));
    }
    endInsertRows();
}

const domain::ProcessInfo* ProcessModel::process(const QModelIndex& index) const
{
    const Node* node = nodeAt(index);
    return node != nullptr ? &node->process : nullptr;
}

QModelIndex ProcessModel::indexOf(const domain::ProcessIdentity& identity, Column column) const
{
    const auto it = m_nodes.find(identity);
    if (it == m_nodes.end()) {
        return {};
    }
    return createIndex(it->second->row, static_cast<int>(column), it->second.get());
}

QModelIndex ProcessModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    return createIndex(row, column, m_rows[static_cast<std::size_t>(row)]);
}

QModelIndex ProcessModel::parent(const QModelIndex& /*child*/) const
{
    return {};
}

int ProcessModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int ProcessModel::columnCount(const QModelIndex& /*parent*/) const
{
    return kColumnCount;
}

QVariant ProcessModel::data(const QModelIndex& index, int role) const
{
    const Node* node = nodeAt(index);
    if (node == nullptr) {
        return {};
    }

    const auto column = static_cast<Column>(index.column());
    switch (role) {
        case Qt::DisplayRole:
            return displayText(node->process, column);
        case Qt::TextAlignmentRole:
            if (column == Column::Name) {
                return {};
            }
            return QVariant::fromValue(Qt::Alignment{Qt::AlignRight | Qt::AlignVCenter});
        case Qt::ForegroundRole:
            if (isNotAvailable(node->process, column)) {
                return QGuiApplication::palette().color(QPalette::PlaceholderText);
            }
            return {};
        case Qt::ToolTipRole:
            if (column == Column::Io) {
                return processIoToolTip(node->process);
            }
            return {};
        default:
            return {};
    }
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section < 0 || section >= kColumnCount) {
        return {};
    }
    const auto column = static_cast<Column>(section);
    switch (role) {
        case Qt::DisplayRole:
            return headerText(column);
        case Qt::ToolTipRole:
            return headerToolTip(column);
        case Qt::TextAlignmentRole:
            // Numeric headers line up with their right-aligned values.
            if (column == Column::Name) {
                return QVariant::fromValue(Qt::Alignment{Qt::AlignLeft | Qt::AlignVCenter});
            }
            return QVariant::fromValue(Qt::Alignment{Qt::AlignRight | Qt::AlignVCenter});
        default:
            return {};
    }
}

const ProcessModel::Node* ProcessModel::nodeAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this) {
        return nullptr;
    }
    return static_cast<const Node*>(index.constInternalPointer());
}

void ProcessModel::removeNodeAt(int row)
{
    const auto position = m_rows.begin() + row;
    const auto identity = (*position)->process.identity();

    beginRemoveRows({}, row, row);
    m_rows.erase(position);
    for (auto it = m_rows.begin() + row; it != m_rows.end(); ++it) {
        --(*it)->row;
    }
    m_nodes.erase(identity);
    endRemoveRows();
}

} // namespace sysmon::ui
