#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"

using namespace sysmon::domain;
using sysmon::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

// A process created creationSeconds after the epoch; by default its PID, so
// lower PIDs are older.
ProcessInfo process(uint32_t pid, std::string name, int64_t creationSeconds = -1)
{
    return ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{
            creationSeconds >= 0 ? creationSeconds : static_cast<int64_t>(pid)}},
        .imageName = std::move(name),
    };
}

// A process started by parentPid, created creationSeconds after the epoch (by
// default its PID).
ProcessInfo child(uint32_t pid, std::string name, uint32_t parentPid, int64_t creationSeconds = -1)
{
    ProcessInfo info = process(pid, std::move(name), creationSeconds);
    info.parentPid = parentPid;
    return info;
}

// Returns the rows under parent as text, one line per process indented two
// spaces per level, e.g. "System\n  smss.exe\n". Checks along the way that
// each row's index and parent agree.
QString treeText(const ProcessModel& model, const QModelIndex& parent = {}, int depth = 0)
{
    QString text;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        EXPECT_EQ(index.row(), row);
        EXPECT_EQ(model.parent(index), parent);
        EXPECT_EQ(model.parent(model.index(row, ProcessModel::kColumnCount - 1, parent)), parent);
        EXPECT_EQ(model.rowCount(model.index(row, 1, parent)), 0);
        text += QString(depth * 2, ' ') + index.data().toString() + "\n";
        text += treeText(model, index, depth + 1);
    }
    return text;
}

// A block of cells named in one dataChanged signal.
struct CellRange
{
    int firstRow{0};
    int lastRow{0};
    int firstColumn{0};
    int lastColumn{0};

    friend bool operator==(const CellRange&, const CellRange&) = default;
};

// Records the signals a model emits: inserted and removed rows as (first,
// last) ranges, and changed cells.
struct ModelSignalLog
{
    std::vector<std::pair<int, int>> inserted;
    std::vector<std::pair<int, int>> removed;
    std::vector<CellRange> changed;
    int moves = 0;
    int resets = 0;

    explicit ModelSignalLog(ProcessModel& model)
    {
        QObject::connect(
            &model, &ProcessModel::rowsInserted, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { inserted.emplace_back(first, last); });
        QObject::connect(
            &model, &ProcessModel::rowsRemoved, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { removed.emplace_back(first, last); });
        QObject::connect(&model, &ProcessModel::dataChanged, &model,
                         [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
                             changed.push_back(CellRange{.firstRow = topLeft.row(),
                                                         .lastRow = bottomRight.row(),
                                                         .firstColumn = topLeft.column(),
                                                         .lastColumn = bottomRight.column()});
                         });
        QObject::connect(&model, &ProcessModel::rowsMoved, &model, [this]() { ++moves; });
        QObject::connect(&model, &ProcessModel::modelReset, &model, [this]() { ++resets; });
    }
};

// Returns the text the model shows for the first row in column.
QString firstRowText(const ProcessModel& model, Column column, int role = Qt::DisplayRole)
{
    return model.index(0, static_cast<int>(column)).data(role).toString();
}

// Returns the names in the model's rows, top to bottom.
std::vector<QString> rowNames(const ProcessModel& model)
{
    std::vector<QString> names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.push_back(model.index(row, static_cast<int>(Column::Name)).data().toString());
    }
    return names;
}

} // namespace

TEST(ProcessModel, UpdateProcesses_FirstList_AddsOneRowPerProcessInListOrder)
{
    ProcessModel model;
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")});

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "explorer.exe", "smss.exe"}));
    EXPECT_EQ(model.index(1, static_cast<int>(Column::Pid)).data().toString(), "800");
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 2}}));
    EXPECT_EQ(log.resets, 0);
}

TEST(ProcessModel, UpdateProcesses_ExitedProcess_RemovesOnlyItsRow)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")});
    const QPersistentModelIndex explorer = model.index(1, 0);
    const QPersistentModelIndex smss = model.index(2, 0);
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "smss.exe"}));
    EXPECT_EQ(log.removed, (std::vector<std::pair<int, int>>{{1, 1}}));
    EXPECT_TRUE(log.inserted.empty());
    EXPECT_EQ(log.resets, 0);
    EXPECT_FALSE(explorer.isValid());
    EXPECT_EQ(smss.row(), 1);
    EXPECT_EQ(smss.data().toString(), "smss.exe");
}

TEST(ProcessModel, UpdateProcesses_NewProcess_AppendsRowWithoutReset)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});
    const QPersistentModelIndex system = model.index(0, 0);
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(900, "notepad.exe"), process(12, "smss.exe")});

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "smss.exe", "notepad.exe"}));
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{2, 2}}));
    EXPECT_TRUE(log.removed.empty());
    EXPECT_EQ(log.resets, 0);
    EXPECT_EQ(system.row(), 0);
}

TEST(ProcessModel, UpdateProcesses_ReusedPid_ReplacesRowWithNewProcess)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe", 100)});
    const QPersistentModelIndex oldNotepad = model.index(1, 0);

    model.updateProcesses({process(4, "System"), process(900, "calc.exe", 200)});

    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_FALSE(oldNotepad.isValid());
    const ProcessInfo* reused = model.process(model.index(1, 0));
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused->imageName, "calc.exe");
    EXPECT_EQ(reused->creationTime, std::chrono::system_clock::time_point{std::chrono::seconds{200}});
}

TEST(ProcessModel, UpdateProcesses_ListedProcess_KeepsRowAndTakesLatestValues)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")});
    const QPersistentModelIndex system = model.index(0, 0);

    ProcessInfo updated = process(4, "System");
    updated.threadCount = 250;
    model.updateProcesses({updated});

    ASSERT_TRUE(system.isValid());
    const ProcessInfo* shown = model.process(system);
    ASSERT_NE(shown, nullptr);
    EXPECT_EQ(shown->threadCount, 250u);
}

TEST(ProcessModel, UpdateProcesses_DuplicateIdentityInList_AddsOneRow)
{
    ProcessModel model;

    model.updateProcesses({process(4, "System"), process(4, "System")});

    EXPECT_EQ(model.rowCount(), 1);
}

TEST(ProcessModel, UpdateProcesses_EmptyList_RemovesEveryRow)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});

    model.updateProcesses({});

    EXPECT_EQ(model.rowCount(), 0);
}

TEST(ProcessModel, IndexOf_ListedIdentity_ReturnsItsRowInColumn)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")});
    model.updateProcesses({process(800, "explorer.exe"), process(12, "smss.exe")});

    const QModelIndex smss = model.indexOf(process(12, "smss.exe").identity(), Column::Pid);

    EXPECT_EQ(smss.row(), 1);
    EXPECT_EQ(smss.column(), static_cast<int>(Column::Pid));
    EXPECT_EQ(smss.data().toString(), "12");
}

TEST(ProcessModel, IndexOf_UnknownIdentity_ReturnsInvalidIndex)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")});

    EXPECT_FALSE(model.indexOf(ProcessIdentity{.pid = 4}).isValid());
    EXPECT_FALSE(model.indexOf(process(5, "other").identity()).isValid());
}

TEST(ProcessModel, Structure_RowsAreTopLevelWithoutChildren)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});

    const QModelIndex first = model.index(0, 0);

    EXPECT_FALSE(model.parent(first).isValid());
    EXPECT_EQ(model.rowCount(first), 0);
    EXPECT_EQ(model.columnCount(), ProcessModel::kColumnCount);
    EXPECT_FALSE(model.index(2, 0).isValid());
    EXPECT_FALSE(model.index(0, ProcessModel::kColumnCount).isValid());
    EXPECT_EQ(model.process(QModelIndex{}), nullptr);
}

TEST(ProcessModel, HeaderData_NamesColumnsInDisplayOrder)
{
    ProcessModel model;

    std::vector<QString> titles;
    for (int section = 0; section < model.columnCount(); ++section) {
        titles.push_back(model.headerData(section, Qt::Horizontal).toString());
    }

    EXPECT_EQ(titles, (std::vector<QString>{"Name", "PID", "CPU", "Memory", "I/O", "Network", "Threads", "Handles"}));
    EXPECT_FALSE(model.headerData(ProcessModel::kColumnCount, Qt::Horizontal).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(ProcessModel, HeaderData_NumericHeadersAlignRightLikeTheirValues)
{
    ProcessModel model;

    const auto alignment = [&model](int section) {
        return model.headerData(section, Qt::Horizontal, Qt::TextAlignmentRole).value<Qt::Alignment>();
    };

    EXPECT_EQ(alignment(static_cast<int>(Column::Name)), Qt::AlignLeft | Qt::AlignVCenter);
    for (int section = static_cast<int>(Column::Pid); section < ProcessModel::kColumnCount; ++section) {
        EXPECT_EQ(alignment(section), Qt::AlignRight | Qt::AlignVCenter) << section;
    }
}

TEST(ProcessModel, HeaderData_MeasuredValuesSortLargestFirstOnFirstClick)
{
    ProcessModel model;

    const auto initialOrder = [&model](Column column) {
        return model.headerData(static_cast<int>(column), Qt::Horizontal, Qt::InitialSortOrderRole)
            .value<Qt::SortOrder>();
    };

    EXPECT_EQ(initialOrder(Column::Name), Qt::AscendingOrder);
    EXPECT_EQ(initialOrder(Column::Pid), Qt::AscendingOrder);
    for (const Column column :
         {Column::Cpu, Column::Memory, Column::Io, Column::Network, Column::Threads, Column::Handles}) {
        EXPECT_EQ(initialOrder(column), Qt::DescendingOrder) << static_cast<int>(column);
    }
}

TEST(ProcessModel, HeaderData_ToolTipsSayWhatValuesCount)
{
    ProcessModel model;

    const auto toolTip = [&model](Column column) {
        return model.headerData(static_cast<int>(column), Qt::Horizontal, Qt::ToolTipRole).toString();
    };

    EXPECT_TRUE(toolTip(Column::Cpu).contains("all logical processors"));
    EXPECT_TRUE(toolTip(Column::Memory).contains("Working set"));
    EXPECT_TRUE(toolTip(Column::Io).contains("not only disk"));
    EXPECT_TRUE(toolTip(Column::Network).contains("ETW"));
}

TEST(ProcessModel, Data_ValueColumns_ShowLatestValuesFormatted)
{
    ProcessModel model;
    ProcessInfo explorer = process(800, "explorer.exe");
    explorer.cpuUsagePercent = 12.54f;
    explorer.workingSetBytes = 1'572'864; // 1.5 MiB
    explorer.ioReadBytesPerSec = 1024;
    explorer.ioWriteBytesPerSec = 1024;
    explorer.threadCount = 1234;
    explorer.handleCount = 56'789;

    model.updateProcesses({explorer});

    EXPECT_EQ(firstRowText(model, Column::Cpu), "12.5%");
    EXPECT_EQ(firstRowText(model, Column::Memory), "1.5 MiB");
    EXPECT_EQ(firstRowText(model, Column::Io), "2.0 KiB/s");
    EXPECT_EQ(firstRowText(model, Column::Threads), "1,234");
    EXPECT_EQ(firstRowText(model, Column::Handles), "56,789");
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), "Read: 1.0 KiB/s\nWrite: 1.0 KiB/s");
}

TEST(ProcessModel, Data_ZeroRates_ShowZeroNotMissing)
{
    ProcessModel model;
    ProcessInfo idle = process(800, "idle.exe");
    idle.cpuUsagePercent = 0.0f;
    idle.ioReadBytesPerSec = 0;
    idle.ioWriteBytesPerSec = 0;

    model.updateProcesses({idle});

    EXPECT_EQ(firstRowText(model, Column::Cpu), "0.0%");
    EXPECT_EQ(firstRowText(model, Column::Io), "0 B/s");
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Cpu)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, Data_MissingValues_ShowNotAvailableInPlaceholderColour)
{
    ProcessModel model;
    ProcessInfo firstSeen = process(800, "explorer.exe");
    firstSeen.ioReadBytesPerSec = 4096;

    model.updateProcesses({firstSeen});

    for (const Column column : {Column::Cpu, Column::Io, Column::Network}) {
        EXPECT_EQ(firstRowText(model, column), "N/A") << static_cast<int>(column);
        EXPECT_TRUE(model.index(0, static_cast<int>(column)).data(Qt::ForegroundRole).isValid())
            << static_cast<int>(column);
    }
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), "Read: 4.0 KiB/s\nWrite: N/A");
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Name)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, UpdateProcesses_ListedProcesses_SignalValueColumnsChanged)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(12, "smss.exe")});

    ASSERT_EQ(log.changed.size(), 1u);
    EXPECT_EQ(log.changed[0], (CellRange{.firstRow = 0,
                                         .lastRow = 1,
                                         .firstColumn = static_cast<int>(Column::Cpu),
                                         .lastColumn = ProcessModel::kColumnCount - 1}));
}

TEST(ProcessModel, Data_NumbersAlignRightAndNamesDoNot)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")});

    const auto alignment = [&model](int column) { return model.index(0, column).data(Qt::TextAlignmentRole); };

    EXPECT_FALSE(alignment(static_cast<int>(Column::Name)).isValid());
    for (int column = static_cast<int>(Column::Pid); column < ProcessModel::kColumnCount; ++column) {
        EXPECT_EQ(alignment(column).value<Qt::Alignment>(), Qt::AlignRight | Qt::AlignVCenter) << column;
    }
}

TEST(ProcessModel, TreeMode_FirstList_ShowsEachProcessUnderItsParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({
        child(0, "", 0),
        child(4, "System", 0),
        child(100, "smss.exe", 4),
        child(200, "csrss.exe", 150),
        child(300, "wininit.exe", 150),
        child(400, "services.exe", 300),
        child(500, "svchost.exe", 400),
        child(600, "svchost.exe", 400),
    });

    // PID 0 parents nothing, and PID 150 is not listed.
    EXPECT_EQ(treeText(model), "System Idle Process\n"
                               "System\n"
                               "  smss.exe\n"
                               "csrss.exe\n"
                               "wininit.exe\n"
                               "  services.exe\n"
                               "    svchost.exe\n"
                               "    svchost.exe\n");
}

TEST(ProcessModel, TreeMode_ChildListedBeforeItsParent_StillUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(500, "svchost.exe", 400), child(400, "services.exe", 300)});

    EXPECT_EQ(treeText(model), "services.exe\n  svchost.exe\n");
}

TEST(ProcessModel, TreeMode_ParentPidReusedByYoungerProcess_NotAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    // The orphan's parent exited and PID 400 now belongs to a later process.
    model.updateProcesses({child(500, "orphan.exe", 400, 500), child(400, "newcomer.exe", 0, 900)});

    EXPECT_EQ(treeText(model), "orphan.exe\nnewcomer.exe\n");
}

TEST(ProcessModel, TreeMode_ParentCreatedInSameTick_IsAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(400, "launcher.exe", 0, 500), child(500, "app.exe", 400, 500)});

    EXPECT_EQ(treeText(model), "launcher.exe\n  app.exe\n");
}

TEST(ProcessModel, TreeMode_ProcessesNamingEachOther_NoCycle)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(10, "a.exe", 20, 5), child(20, "b.exe", 10, 5), child(30, "self.exe", 30)});

    EXPECT_EQ(treeText(model), "b.exe\n  a.exe\nself.exe\n");
}

TEST(ProcessModel, TreeMode_NewChildOfShownParent_InsertedUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(400, "services.exe", 0)});
    const QPersistentModelIndex services = model.index(0, 0);
    ModelSignalLog log(model);

    model.updateProcesses({child(400, "services.exe", 0), child(500, "svchost.exe", 400)});

    EXPECT_EQ(treeText(model), "services.exe\n  svchost.exe\n");
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 0}}));
    EXPECT_EQ(log.resets, 0);
    EXPECT_TRUE(services.isValid());
}

TEST(ProcessModel, TreeMode_NewParentAndChildren_OneInsertPerParent)
{
    ProcessModel model;
    model.setTreeMode(true);
    ModelSignalLog log(model);

    model.updateProcesses({child(400, "services.exe", 0), child(500, "svchost.exe", 400),
                           child(600, "svchost.exe", 400), child(700, "spoolsv.exe", 400)});

    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 0}, {0, 2}}));
}

TEST(ProcessModel, TreeMode_ParentExits_ChildrenMoveToTopLevel)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4), child(200, "user.exe", 100)});
    const QPersistentModelIndex user = model.indexOf(child(200, "user.exe", 100).identity());
    ModelSignalLog log(model);

    model.updateProcesses({child(4, "System", 0), child(200, "user.exe", 100)});

    EXPECT_EQ(treeText(model), "System\nuser.exe\n");
    EXPECT_EQ(log.moves, 1);
    EXPECT_EQ(log.removed.size(), 1u);
    EXPECT_EQ(log.resets, 0);
    ASSERT_TRUE(user.isValid());
    EXPECT_FALSE(user.parent().isValid());
    EXPECT_EQ(user.data().toString(), "user.exe");
}

TEST(ProcessModel, TreeMode_ParentAndChildExitTogether_SurvivorsMoveToTopLevel)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(
        {child(10, "a.exe", 0), child(20, "b.exe", 10), child(30, "c.exe", 20), child(40, "d.exe", 10)});

    model.updateProcesses({child(30, "c.exe", 20), child(40, "d.exe", 10)});

    EXPECT_EQ(treeText(model), "c.exe\nd.exe\n");
}

TEST(ProcessModel, TreeMode_ValuesChange_OneSignalPerParent)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4), child(200, "other.exe", 0)});
    ModelSignalLog log(model);

    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4), child(200, "other.exe", 0)});

    // The top level's two rows, and System's one child.
    std::vector<int> rowCounts;
    for (const auto& range : log.changed) {
        rowCounts.push_back(range.lastRow - range.firstRow + 1);
    }
    std::ranges::sort(rowCounts);
    EXPECT_EQ(rowCounts, (std::vector<int>{1, 2}));
}

TEST(ProcessModel, SetTreeMode_Toggle_ResetsOnceAndRebuilds)
{
    ProcessModel model;
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)});
    ModelSignalLog log(model);

    model.setTreeMode(true);
    EXPECT_TRUE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\n  smss.exe\n");
    EXPECT_EQ(log.resets, 1);

    model.setTreeMode(true);
    EXPECT_EQ(log.resets, 1);

    model.setTreeMode(false);
    EXPECT_FALSE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\nsmss.exe\n");
    EXPECT_EQ(log.resets, 2);
}

TEST(ProcessModel, FlatMode_ParentPids_Ignored)
{
    ProcessModel model;

    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)});

    EXPECT_FALSE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\nsmss.exe\n");
}

namespace
{

// Visits every row of model depth first, calling visit(index).
template <typename Model, typename Visit>
void forEachRow(const Model& model, Visit visit, const QModelIndex& parent = {})
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        visit(index);
        forEachRow(model, visit, index);
    }
}

} // namespace

TEST(ProcessModel, TreeMode_RandomChurn_StructureAndPersistentIndexesStayConsistent)
{
    std::mt19937 random(20261014);
    ProcessModel model;
    model.setTreeMode(true);
    // A sorted proxy on top, as in the view, checks that the signals are consistent.
    sysmon::ui::ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);

    std::vector<ProcessInfo> live;
    int64_t clock = 1;
    for (int step = 0; step < 300; ++step) {
        std::vector<std::pair<QPersistentModelIndex, ProcessIdentity>> tracked;
        for (const auto& info : live) {
            tracked.emplace_back(model.indexOf(info.identity()), info.identity());
        }

        std::erase_if(live, [&random](const ProcessInfo&) { return random() % 8 == 0; });
        for (auto& info : live) {
            info.cpuUsagePercent = static_cast<float>(random() % 100);
        }
        // A PID is reused only after its process exits, so the new process is
        // created later and has a different identity.
        ++clock;
        const auto starts = random() % 6;
        for (unsigned start = 0; start < starts; ++start) {
            // A small PID space makes reuse common. Two running processes never share a PID.
            const auto pid = static_cast<uint32_t>(4 * (1 + random() % 40));
            if (std::ranges::any_of(live, [pid](const ProcessInfo& info) { return info.pid == pid; })) {
                continue;
            }
            // Processes started in one step share a clock tick about half the time.
            clock += static_cast<int64_t>(random() % 2);
            const uint32_t parentPid = !live.empty() && random() % 4 != 0 ? live[random() % live.size()].pid
                                                                          : static_cast<uint32_t>(4 * (random() % 41));
            ProcessInfo info = child(pid, "p" + std::to_string(pid), parentPid, clock);
            info.cpuUsagePercent = static_cast<float>(random() % 100);
            live.push_back(info);
        }

        model.updateProcesses(live);

        for (const auto& [index, identity] : tracked) {
            const bool isLive =
                std::ranges::any_of(live, [&identity](const ProcessInfo& info) { return info.identity() == identity; });
            ASSERT_EQ(index.isValid(), isLive) << "step " << step;
            if (isLive) {
                ASSERT_EQ(model.process(index)->identity(), identity) << "step " << step;
            }
        }

        std::size_t rows = 0;
        forEachRow(model, [&](const QModelIndex& index) {
            ++rows;
            ASSERT_EQ(model.indexOf(model.process(index)->identity()), index);
            const QModelIndex parent = model.parent(index);
            if (parent.isValid()) {
                const ProcessInfo* childInfo = model.process(index);
                const ProcessInfo* parentInfo = model.process(parent);
                EXPECT_EQ(parentInfo->pid, childInfo->parentPid);
                EXPECT_LE(parentInfo->creationTime, childInfo->creationTime);
            }
        });
        ASSERT_EQ(rows, live.size()) << "step " << step;

        std::size_t proxyRows = 0;
        forEachRow(proxy, [&proxyRows](const QModelIndex&) { ++proxyRows; });
        ASSERT_EQ(proxyRows, live.size()) << "step " << step;
    }
}
