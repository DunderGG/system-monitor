#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include "domain/process_identity.h"
#include "domain/process_info.h"

namespace sysmon::ui
{

/**
 * Qt item model of the running processes, one row per process, either as a
 * flat list (the default) or as a tree that shows each process under the
 * process that started it.
 *
 * A row belongs to one ProcessIdentity (PID and creation time), never to a
 * PID alone, so a process that reuses an exited process's PID gets a new row.
 * updateProcesses() changes the rows in place: it removes the rows of
 * processes that are no longer listed, updates the values of the others, and
 * inserts rows for new ones, so views keep their scroll position, selection,
 * and expanded state. The model is never reset on a refresh; only
 * setTreeMode() resets it.
 *
 * In tree mode a process's parent is the latest listed process with its
 * parent PID that was created no later than itself. Windows records only the
 * parent's PID, which may have been reused by a younger process since. The
 * Idle process (PID 0) is no process's parent. A process whose parent has
 * exited, or was never listed, is at the top level, and when a parent exits
 * its children move there.
 *
 * Values that are missing (a rate not measured yet, or per-process network
 * usage, which is not collected) show "N/A" in the placeholder text colour,
 * never zero. Column headers have tooltips that say what a value counts.
 *
 * Not thread-safe; use only on the UI thread.
 */
class ProcessModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /** Columns in display order. */
    enum class Column
    {
        Name,
        Pid,
        // Share of all logical processors used since the previous refresh.
        Cpu,
        // Working set.
        Memory,
        // Read plus write bytes per second, of all I/O, not only disk.
        Io,
        // Placeholder until per-process network usage is collected (with ETW).
        Network,
        Threads,
        Handles,
    };

    static constexpr int kColumnCount = 8;

    explicit ProcessModel(QObject* parent = nullptr);
    ~ProcessModel() override = default;

    using QObject::parent;

    /**
     * Makes the model show processes, the latest process list. A process keeps
     * its row while it is listed.
     */
    void updateProcesses(const std::vector<domain::ProcessInfo>& processes);

    /**
     * Shows each process under its parent (true) or every process at the top
     * level (false). Changing the mode resets the model.
     */
    void setTreeMode(bool enabled);
    [[nodiscard]] bool isTreeMode() const;

    /** Returns the process shown at index, or nullptr for an invalid index. */
    [[nodiscard]] const domain::ProcessInfo* process(const QModelIndex& index) const;

    /** Returns the index of the process with identity in column, or an invalid index if no row shows it. */
    [[nodiscard]] QModelIndex indexOf(const domain::ProcessIdentity& identity, Column column = Column::Name) const;

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;

private:
    struct Node
    {
        domain::ProcessInfo process;
        // The parent's node, or nullptr at the top level.
        Node* parent{nullptr};
        std::vector<Node*> children;
        // Position among its siblings, kept current so indexOf() needs no search.
        int row{0};
    };

    [[nodiscard]] const Node* nodeAt(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexOfNode(const Node* node, int column = 0) const;
    [[nodiscard]] std::vector<Node*>& siblingsOf(Node* parent);
    [[nodiscard]] Node* findParent(const Node& node) const;
    void removeNode(Node* node);
    void insertNodes(const std::vector<Node*>& added);
    void signalValuesChanged();

    // Owns every node. m_topLevel and each node's children hold them in row order.
    std::unordered_map<domain::ProcessIdentity, std::unique_ptr<Node>, domain::ProcessIdentityHash> m_nodes;
    std::unordered_multimap<uint32_t, Node*> m_nodesByPid;
    std::vector<Node*> m_topLevel;
    bool m_treeMode{false};
};

} // namespace sysmon::ui
