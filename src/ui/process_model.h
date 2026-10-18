#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <QAbstractItemModel>
#include <QIcon>
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
 * updateProcesses() changes the rows in place: it updates the values of
 * listed processes, inserts rows for new ones, and marks processes that are
 * no longer listed, so views keep their scroll position, selection, and
 * expanded state. The model is never reset on a refresh; only setTreeMode()
 * resets it. Value changes are signalled only for the cells whose values
 * changed, as one dataChanged() block per parent naming the display, tooltip,
 * and foreground roles, so a list that repeats the previous one signals
 * nothing.
 *
 * Changes are highlighted for kHighlightDuration of the monotonic time passed
 * to updateProcesses() and updateTime(). A new process has a green
 * background, except in the first list. A process that exited keeps its row
 * and last values with a red background and italic text until the highlight
 * ends, and the row is then removed. Both have a tooltip on the name, so the
 * state is not told by colour alone.
 *
 * In tree mode a process's parent is the latest listed process with its
 * parent PID that was created no later than itself. Windows records only the
 * parent's PID, which may have been reused by a younger process since. An
 * exited process's row can be a parent only of a process first listed no
 * later than the refresh that marked it exited; a process listed after that
 * was created after it exited. The Idle process (PID 0) is no process's
 * parent. A process whose parent has exited, or was never listed, is at the
 * top level, and when a parent exits its children move there.
 *
 * Values that are missing (a rate not measured yet, or per-process network
 * usage, which is not collected) show "N/A" in the placeholder text colour,
 * never zero. Column headers have tooltips that say what a value counts.
 *
 * The name column shows an access indicator, detailsStatusIcon(), for a
 * process whose path and command line could not be read, and its tooltip
 * gives them or says why they are missing (processDetailsToolTip()).
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

    /** How long a new or exited process is highlighted; an exited process's row is then removed. */
    static constexpr std::chrono::seconds kHighlightDuration{3};

    explicit ProcessModel(QObject* parent = nullptr);
    ~ProcessModel() override = default;

    using QObject::parent;

    /**
     * Makes the model show processes, the latest process list, at monotonic
     * time now (the snapshot's timestamp), and ends the highlights that have
     * lasted kHighlightDuration by then. A process keeps its row while it is
     * listed, and for kHighlightDuration after it is not.
     */
    void updateProcesses(const std::vector<domain::ProcessInfo>& processes, std::chrono::steady_clock::time_point now);

    /**
     * Ends the highlights that have lasted kHighlightDuration by now, and
     * removes the rows of processes that exited that long ago, without a new
     * list (for a snapshot that has none).
     */
    void updateTime(std::chrono::steady_clock::time_point now);

    /**
     * Shows each process under its parent (true) or every process at the top
     * level (false). Changing the mode resets the model.
     */
    void setTreeMode(bool enabled);
    [[nodiscard]] bool isTreeMode() const;

    /** Returns the process shown at index, or nullptr for an invalid index. */
    [[nodiscard]] const domain::ProcessInfo* process(const QModelIndex& index) const;

    /** Returns true if the process shown at index has exited and its row is kept only until its highlight ends. */
    [[nodiscard]] bool hasExited(const QModelIndex& index) const;

    /** Returns the index of the process with identity in column, or an invalid index if no row shows it. */
    [[nodiscard]] QModelIndex indexOf(const domain::ProcessIdentity& identity, Column column = Column::Name) const;

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;

signals:
    /**
     * Emitted by updateProcesses() before the dataChanged() signals for one
     * refresh's value changes: signalCount signals (one per parent with
     * changes) covering firstColumn to lastColumn. valueChangesEnded() follows
     * them. A sorting proxy can sort once afterwards rather than once per
     * signal.
     */
    void valueChangesStarted(int firstColumn, int lastColumn, int signalCount);
    void valueChangesEnded();

private:
    struct Node
    {
        domain::ProcessInfo process{};
        // The parent's node, or nullptr at the top level.
        Node* parent{nullptr};
        std::vector<Node*> children{};
        // Position among its siblings, kept current so indexOf() needs no search.
        int row{0};
        // When the process was first listed, kept for the life of the row.
        std::chrono::steady_clock::time_point listedAt{};
        // When the process first appeared, while it is highlighted as new.
        std::optional<std::chrono::steady_clock::time_point> startedAt{};
        // When the process was first missing from the list; it has exited.
        std::optional<std::chrono::steady_clock::time_point> exitedAt{};
    };

    // The smallest block of cells that covers the changed values among one
    // parent's children.
    struct ChangedBlock
    {
        int firstRow{0};
        int lastRow{0};
        int firstColumn{0};
        int lastColumn{0};
    };

    // Changed blocks keyed by the parent node, nullptr for the top level.
    using ChangedBlocks = std::unordered_map<Node*, ChangedBlock>;

    [[nodiscard]] const Node* nodeAt(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexOfNode(const Node* node, int column = 0) const;
    [[nodiscard]] std::vector<Node*>& siblingsOf(Node* parent);
    [[nodiscard]] Node* findParent(const Node& node) const;
    void removeNode(Node* node);
    void insertNodes(const std::vector<Node*>& added);
    void signalValuesChanged(const ChangedBlocks& changed);
    void signalHighlightChanged(const Node* node);

    // Owns every node. m_topLevel and each node's children hold them in row order.
    std::unordered_map<domain::ProcessIdentity, std::unique_ptr<Node>, domain::ProcessIdentityHash> m_nodes;
    std::unordered_multimap<uint32_t, Node*> m_nodesByPid;
    std::vector<Node*> m_topLevel;
    bool m_treeMode{false};
    // False until the first list, whose processes are not highlighted as new.
    bool m_hasList{false};
    // The name column's icon for each ProcessDetailsStatus, indexed by its value.
    std::array<QIcon, domain::kProcessDetailsStatusCount> m_detailsIcons;
};

} // namespace sysmon::ui
