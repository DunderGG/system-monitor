#include "ui/process_model.h"

#include <cstddef>
#include <unordered_set>

#include <QString>

#include "ui/process_formatting.h"

namespace sysmon::ui
{

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
            switch (column) {
                case Column::Name:
                    return processDisplayName(node->process);
                case Column::Pid:
                    return QString::number(node->process.pid);
            }
            return {};
        case Qt::TextAlignmentRole:
            if (column == Column::Name) {
                return {};
            }
            return QVariant::fromValue(Qt::Alignment{Qt::AlignRight | Qt::AlignVCenter});
        default:
            return {};
    }
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (static_cast<Column>(section)) {
        case Column::Name:
            return QStringLiteral("Name");
        case Column::Pid:
            return QStringLiteral("PID");
    }
    return {};
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
