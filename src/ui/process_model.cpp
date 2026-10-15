#include "ui/process_model.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <utility>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QList>
#include <QPalette>
#include <QString>

#include "ui/dashboard_formatting.h"
#include "ui/process_formatting.h"
#include "ui/process_icons.h"

namespace sysmon::ui
{

namespace
{

using Column = ProcessModel::Column;

// Highlights for new and exited processes. Translucent, so they tint the row
// colours of light and dark palettes alike and keep the text readable.
constexpr QColor kStartedColor{46, 160, 67, 90};
constexpr QColor kExitedColor{218, 54, 51, 90};

// Returns the first and last columns whose values differ between two readings
// of one process, or std::nullopt when none do. Raw values are compared, not
// display text: a change too small to show can still move the row in a sorted
// view. Name and PID never change for a process.
std::optional<std::pair<int, int>> changedColumns(const domain::ProcessInfo& before, const domain::ProcessInfo& after)
{
    std::optional<std::pair<int, int>> columns;
    const auto mark = [&columns](Column column) {
        const int index = static_cast<int>(column);
        columns = columns ? std::pair{columns->first, index} : std::pair{index, index};
    };
    // In column order, so each mark extends the range to the right. The
    // details are read once per process, so they change only if a reader
    // ever fills them in later; the name column shows them.
    if (before.detailsStatus != after.detailsStatus || before.imagePath != after.imagePath ||
        before.commandLine != after.commandLine) {
        mark(Column::Name);
    }
    if (before.cpuUsagePercent != after.cpuUsagePercent) {
        mark(Column::Cpu);
    }
    // The Memory cell shows the private working set, and its tooltip the other two.
    if (before.privateWorkingSetBytes != after.privateWorkingSetBytes ||
        before.workingSetBytes != after.workingSetBytes || before.privateBytes != after.privateBytes) {
        mark(Column::Memory);
    }
    if (before.ioReadBytesPerSec != after.ioReadBytesPerSec || before.ioWriteBytesPerSec != after.ioWriteBytesPerSec) {
        mark(Column::Io);
    }
    if (before.threadCount != after.threadCount) {
        mark(Column::Threads);
    }
    if (before.handleCount != after.handleCount) {
        mark(Column::Handles);
    }
    return columns;
}

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
            return formatBytes(process.privateWorkingSetBytes);
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
            return QStringLiteral("Image name of the process. An icon before the name means its path and command "
                                  "line could not be read; point at the name to see why.");
        case Column::Pid:
            return QStringLiteral("Process ID");
        case Column::Cpu:
            return QStringLiteral("Share of all logical processors the process used since the previous refresh");
        case Column::Memory:
            return QStringLiteral("Private working set: the physical memory the process uses on its own, as Task "
                                  "Manager's Memory column shows it. Point at a value to see the working set, which "
                                  "includes shared memory, and the commit size.");
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

ProcessModel::ProcessModel(QObject* parent) : QAbstractItemModel(parent)
{
    for (const auto status : {domain::ProcessDetailsStatus::NotQueried, domain::ProcessDetailsStatus::Available,
                              domain::ProcessDetailsStatus::AccessDenied, domain::ProcessDetailsStatus::Unavailable}) {
        m_detailsIcons[static_cast<std::size_t>(status)] = detailsStatusIcon(status);
    }
}

void ProcessModel::updateProcesses(const std::vector<domain::ProcessInfo>& processes,
                                   std::chrono::steady_clock::time_point now)
{
    std::unordered_set<domain::ProcessIdentity, domain::ProcessIdentityHash> listed;
    listed.reserve(processes.size());
    for (const auto& process : processes) {
        listed.insert(process.identity());
    }

    // A process missing from the list has exited. Its row stays, marked,
    // until its highlight ends. One that is listed again is running after all.
    for (const auto& [identity, node] : m_nodes) {
        const bool isListed = listed.contains(identity);
        if (!isListed && !node->exitedAt) {
            node->exitedAt = now;
            node->startedAt.reset();
            signalHighlightChanged(node.get());
        } else if (isListed && node->exitedAt) {
            node->exitedAt.reset();
            signalHighlightChanged(node.get());
        }
    }
    updateTime(now);

    std::vector<Node*> added;
    ChangedBlocks changed;
    for (const auto& process : processes) {
        const auto identity = process.identity();
        if (const auto it = m_nodes.find(identity); it != m_nodes.end()) {
            Node& node = *it->second;
            if (const auto columns = changedColumns(node.process, process)) {
                const auto [block, isFirst] =
                    changed.try_emplace(node.parent, ChangedBlock{node.row, node.row, columns->first, columns->second});
                if (!isFirst) {
                    block->second.firstRow = std::min(block->second.firstRow, node.row);
                    block->second.lastRow = std::max(block->second.lastRow, node.row);
                    block->second.firstColumn = std::min(block->second.firstColumn, columns->first);
                    block->second.lastColumn = std::max(block->second.lastColumn, columns->second);
                }
            }
            node.process = process;
        } else if (listed.erase(identity) > 0) {
            // Erasing the identity keeps a duplicate entry in the list from adding a second row.
            auto node = std::make_unique<Node>(Node{.process = process, .listedAt = now});
            if (m_hasList) {
                node->startedAt = now;
            }
            added.push_back(node.get());
            m_nodesByPid.emplace(process.pid, node.get());
            m_nodes.emplace(identity, std::move(node));
        }
    }
    signalValuesChanged(changed);
    insertNodes(added);
    m_hasList = true;
}

void ProcessModel::updateTime(std::chrono::steady_clock::time_point now)
{
    std::vector<std::pair<int, Node*>> expired;
    for (const auto& [identity, node] : m_nodes) {
        if (node->exitedAt && now - *node->exitedAt >= kHighlightDuration) {
            int depth = 0;
            for (const Node* ancestor = node->parent; ancestor != nullptr; ancestor = ancestor->parent) {
                ++depth;
            }
            expired.emplace_back(depth, node.get());
        }
    }
    // Deepest first, so each removed node's remaining children outlive it.
    std::ranges::sort(expired, std::greater{}, &std::pair<int, Node*>::first);
    for (const auto& [depth, node] : expired) {
        removeNode(node);
    }

    for (const auto& [identity, node] : m_nodes) {
        if (node->startedAt && now - *node->startedAt >= kHighlightDuration) {
            node->startedAt.reset();
            signalHighlightChanged(node.get());
        }
    }
}

void ProcessModel::setTreeMode(bool enabled)
{
    if (m_treeMode == enabled) {
        return;
    }

    beginResetModel();
    m_treeMode = enabled;
    m_topLevel.clear();
    std::vector<Node*> nodes;
    nodes.reserve(m_nodes.size());
    for (const auto& [identity, node] : m_nodes) {
        node->parent = nullptr;
        node->children.clear();
        nodes.push_back(node.get());
    }
    // Oldest first gives the top level a stable order. findParent() sees the
    // links made so far, which keeps it from making a cycle.
    std::ranges::sort(nodes, [](const Node* left, const Node* right) {
        return std::tie(left->process.creationTime, left->process.pid) <
               std::tie(right->process.creationTime, right->process.pid);
    });
    for (Node* node : nodes) {
        node->parent = findParent(*node);
    }
    for (Node* node : nodes) {
        auto& siblings = siblingsOf(node->parent);
        node->row = static_cast<int>(siblings.size());
        siblings.push_back(node);
    }
    endResetModel();
}

bool ProcessModel::isTreeMode() const
{
    return m_treeMode;
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
    return indexOfNode(it->second.get(), static_cast<int>(column));
}

QModelIndex ProcessModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    const Node* parentNode = nodeAt(parent);
    const auto& siblings = parentNode != nullptr ? parentNode->children : m_topLevel;
    return createIndex(row, column, siblings[static_cast<std::size_t>(row)]);
}

QModelIndex ProcessModel::parent(const QModelIndex& child) const
{
    const Node* node = nodeAt(child);
    return node != nullptr ? indexOfNode(node->parent) : QModelIndex{};
}

int ProcessModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(m_topLevel.size());
    }
    // Only the first column has children, as views expect.
    const Node* node = nodeAt(parent);
    return node != nullptr && parent.column() == 0 ? static_cast<int>(node->children.size()) : 0;
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
            if (column == Column::Memory) {
                return processMemoryToolTip(node->process);
            }
            if (column == Column::Io) {
                return processIoToolTip(node->process);
            }
            if (column == Column::Name) {
                QString toolTip = processDetailsToolTip(node->process);
                if (node->exitedAt) {
                    toolTip += QStringLiteral("<p>This process has exited; its row is removed in a moment.</p>");
                } else if (node->startedAt) {
                    toolTip += QStringLiteral("<p>This process started in the last few seconds.</p>");
                }
                return toolTip;
            }
            return {};
        case Qt::DecorationRole:
            if (column == Column::Name) {
                return m_detailsIcons[static_cast<std::size_t>(node->process.detailsStatus)];
            }
            return {};
        case Qt::BackgroundRole:
            if (node->exitedAt) {
                return QBrush(kExitedColor);
            }
            if (node->startedAt) {
                return QBrush(kStartedColor);
            }
            return {};
        case Qt::FontRole:
            if (node->exitedAt) {
                QFont font;
                font.setItalic(true);
                return font;
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
        case Qt::InitialSortOrderRole:
            // A first click on a measured value lists the largest first.
            if (column == Column::Name || column == Column::Pid) {
                return QVariant::fromValue(Qt::AscendingOrder);
            }
            return QVariant::fromValue(Qt::DescendingOrder);
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

QModelIndex ProcessModel::indexOfNode(const Node* node, int column) const
{
    return node != nullptr ? createIndex(node->row, column, node) : QModelIndex{};
}

std::vector<ProcessModel::Node*>& ProcessModel::siblingsOf(Node* parent)
{
    return parent != nullptr ? parent->children : m_topLevel;
}

ProcessModel::Node* ProcessModel::findParent(const Node& node) const
{
    const domain::ProcessInfo& child = node.process;
    // The Idle process reports PID 0 as its own parent, and so does System.
    if (!m_treeMode || child.parentPid == 0) {
        return nullptr;
    }

    // The parent's PID may have been reused by a process created after the
    // child, which cannot be its parent. A process found missing in a refresh
    // before the child was first listed had exited before the child was
    // created, so the PID was reused by a process that was never listed. One
    // found missing in the refresh that first listed the child may have
    // started it just before exiting.
    Node* parent = nullptr;
    const auto [first, last] = m_nodesByPid.equal_range(child.parentPid);
    for (auto it = first; it != last; ++it) {
        const Node& candidate = *it->second;
        if (candidate.exitedAt && *candidate.exitedAt < node.listedAt) {
            continue;
        }
        const auto creationTime = candidate.process.creationTime;
        if (creationTime <= child.creationTime && (parent == nullptr || creationTime > parent->process.creationTime)) {
            parent = it->second;
        }
    }

    // Processes created in the same clock tick can name each other, or a
    // process itself; either link would make a cycle.
    for (const Node* ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent) {
        if (ancestor == &node) {
            return nullptr;
        }
    }
    return parent;
}

void ProcessModel::removeNode(Node* node)
{
    // A process's children keep running after it exits; they move to the top level.
    if (!node->children.empty()) {
        const int count = static_cast<int>(node->children.size());
        [[maybe_unused]] const bool canMove =
            beginMoveRows(indexOfNode(node), 0, count - 1, QModelIndex{}, static_cast<int>(m_topLevel.size()));
        assert(canMove && "moving children to the top level is always valid");
        for (Node* child : node->children) {
            child->parent = nullptr;
            child->row = static_cast<int>(m_topLevel.size());
            m_topLevel.push_back(child);
        }
        node->children.clear();
        endMoveRows();
    }

    auto& siblings = siblingsOf(node->parent);
    const int row = node->row;
    beginRemoveRows(indexOfNode(node->parent), row, row);
    siblings.erase(siblings.begin() + row);
    for (auto it = siblings.begin() + row; it != siblings.end(); ++it) {
        --(*it)->row;
    }
    const auto [first, last] = m_nodesByPid.equal_range(node->process.pid);
    for (auto it = first; it != last; ++it) {
        if (it->second == node) {
            m_nodesByPid.erase(it);
            break;
        }
    }
    m_nodes.erase(node->process.identity());
    endRemoveRows();
}

void ProcessModel::insertNodes(const std::vector<Node*>& added)
{
    // Parents are looked up among all nodes, the new ones included, so a
    // process and its parent that appear in the same list are linked.
    for (Node* node : added) {
        node->parent = findParent(*node);
    }

    // Each parent's new children are inserted with one signal. Parents that
    // are already shown go first, so a new parent is shown before its children.
    const std::unordered_set<const Node*> isNew(added.begin(), added.end());
    std::unordered_map<Node*, std::vector<Node*>> newChildren;
    std::vector<Node*> ready;
    for (Node* node : added) {
        auto& group = newChildren[node->parent];
        if (group.empty() && !isNew.contains(node->parent)) {
            ready.push_back(node->parent);
        }
        group.push_back(node);
    }

    while (!ready.empty()) {
        Node* parent = ready.back();
        ready.pop_back();
        const auto& group = newChildren[parent];
        auto& siblings = siblingsOf(parent);
        const int first = static_cast<int>(siblings.size());
        beginInsertRows(indexOfNode(parent), first, first + static_cast<int>(group.size()) - 1);
        for (Node* node : group) {
            node->row = static_cast<int>(siblings.size());
            siblings.push_back(node);
        }
        endInsertRows();
        for (Node* node : group) {
            if (newChildren.contains(node)) {
                ready.push_back(node);
            }
        }
    }
}

void ProcessModel::signalHighlightChanged(const Node* node)
{
    static const QList<int> kHighlightRoles{Qt::BackgroundRole, Qt::FontRole, Qt::ToolTipRole};
    emit dataChanged(indexOfNode(node, 0), indexOfNode(node, kColumnCount - 1), kHighlightRoles);
}

void ProcessModel::signalValuesChanged(const ChangedBlocks& changed)
{
    if (changed.empty()) {
        return;
    }

    int firstColumn = kColumnCount;
    int lastColumn = 0;
    for (const auto& [parent, block] : changed) {
        firstColumn = std::min(firstColumn, block.firstColumn);
        lastColumn = std::max(lastColumn, block.lastColumn);
    }

    static const QList<int> kValueRoles{Qt::DisplayRole, Qt::ToolTipRole, Qt::ForegroundRole, Qt::DecorationRole};
    emit valueChangesStarted(firstColumn, lastColumn, static_cast<int>(changed.size()));
    for (const auto& [parent, block] : changed) {
        const QModelIndex parentIndex = indexOfNode(parent);
        emit dataChanged(index(block.firstRow, block.firstColumn, parentIndex),
                         index(block.lastRow, block.lastColumn, parentIndex), kValueRoles);
    }
    emit valueChangesEnded();
}

} // namespace sysmon::ui
