#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>
#include <QVariant>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "ui/process_formatting.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"

using namespace sysmon::domain;
using sysmon::ui::processMemoryToolTip;
using sysmon::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

// The monotonic time of each refresh, unless a test moves it on.
const auto kNow = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
// When the highlights of changes made at kNow have ended.
const auto kLater = kNow + ProcessModel::kHighlightDuration;

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

    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}, kNow);

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "explorer.exe", "smss.exe"}));
    EXPECT_EQ(model.index(1, static_cast<int>(Column::Pid)).data().toString(), "800");
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 2}}));
    EXPECT_EQ(log.resets, 0);
}

TEST(ProcessModel, UpdateProcesses_ExitedProcess_KeptUntilHighlightEndsThenOnlyItsRowRemoved)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}, kNow);
    const QPersistentModelIndex explorer = model.index(1, 0);
    const QPersistentModelIndex smss = model.index(2, 0);
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kNow);
    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "explorer.exe", "smss.exe"}));
    EXPECT_TRUE(log.removed.empty());

    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kLater);

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
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kNow);
    const QPersistentModelIndex system = model.index(0, 0);
    ModelSignalLog log(model);

    model.updateProcesses({process(4, "System"), process(900, "notepad.exe"), process(12, "smss.exe")}, kNow);

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "smss.exe", "notepad.exe"}));
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{2, 2}}));
    EXPECT_TRUE(log.removed.empty());
    EXPECT_EQ(log.resets, 0);
    EXPECT_EQ(system.row(), 0);
}

TEST(ProcessModel, UpdateProcesses_ReusedPid_NewProcessGetsItsOwnRow)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe", 100)}, kNow);
    const QPersistentModelIndex oldNotepad = model.index(1, 0);

    model.updateProcesses({process(4, "System"), process(900, "calc.exe", 200)}, kNow);
    // The exited process is still shown, beside the one that reused its PID.
    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "notepad.exe", "calc.exe"}));
    model.updateTime(kLater);

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
    model.updateProcesses({process(4, "System")}, kNow);
    const QPersistentModelIndex system = model.index(0, 0);

    ProcessInfo updated = process(4, "System");
    updated.threadCount = 250;
    model.updateProcesses({updated}, kNow);

    ASSERT_TRUE(system.isValid());
    const ProcessInfo* shown = model.process(system);
    ASSERT_NE(shown, nullptr);
    EXPECT_EQ(shown->threadCount, 250u);
}

TEST(ProcessModel, UpdateProcesses_DuplicateIdentityInList_AddsOneRow)
{
    ProcessModel model;

    model.updateProcesses({process(4, "System"), process(4, "System")}, kNow);

    EXPECT_EQ(model.rowCount(), 1);
}

TEST(ProcessModel, UpdateProcesses_EmptyList_RemovesEveryRowWhenHighlightsEnd)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kNow);

    model.updateProcesses({}, kNow);
    EXPECT_EQ(model.rowCount(), 2);
    model.updateProcesses({}, kLater);

    EXPECT_EQ(model.rowCount(), 0);
}

TEST(ProcessModel, IndexOf_ListedIdentity_ReturnsItsRowInColumn)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}, kNow);
    model.updateProcesses({process(800, "explorer.exe"), process(12, "smss.exe")}, kNow);
    model.updateTime(kLater);

    const QModelIndex smss = model.indexOf(process(12, "smss.exe").identity(), Column::Pid);

    EXPECT_EQ(smss.row(), 1);
    EXPECT_EQ(smss.column(), static_cast<int>(Column::Pid));
    EXPECT_EQ(smss.data().toString(), "12");
}

TEST(ProcessModel, IndexOf_UnknownIdentity_ReturnsInvalidIndex)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")}, kNow);

    EXPECT_FALSE(model.indexOf(ProcessIdentity{.pid = 4}).isValid());
    EXPECT_FALSE(model.indexOf(process(5, "other").identity()).isValid());
}

TEST(ProcessModel, Structure_RowsAreTopLevelWithoutChildren)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kNow);

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
    EXPECT_TRUE(toolTip(Column::Memory).startsWith("Private working set"));
    EXPECT_TRUE(toolTip(Column::Memory).contains("Task Manager"));
    EXPECT_TRUE(toolTip(Column::Io).contains("not only disk"));
    EXPECT_TRUE(toolTip(Column::Network).contains("ETW"));
}

TEST(ProcessModel, Data_ValueColumns_ShowLatestValuesFormatted)
{
    ProcessModel model;
    ProcessInfo explorer = process(800, "explorer.exe");
    explorer.cpuUsagePercent = 12.54f;
    explorer.workingSetBytes = 3'145'728;        // 3.0 MiB
    explorer.privateWorkingSetBytes = 1'572'864; // 1.5 MiB
    explorer.privateBytes = 2'097'152;           // 2.0 MiB
    explorer.ioReadBytesPerSec = 1024;
    explorer.ioWriteBytesPerSec = 1024;
    explorer.threadCount = 1234;
    explorer.handleCount = 56'789;

    model.updateProcesses({explorer}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Cpu), "12.5%");
    EXPECT_EQ(firstRowText(model, Column::Memory), "1.5 MiB");
    EXPECT_EQ(firstRowText(model, Column::Io), "2.0 KiB/s");
    EXPECT_EQ(firstRowText(model, Column::Threads), "1,234");
    EXPECT_EQ(firstRowText(model, Column::Handles), "56,789");
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), "Read: 1.0 KiB/s\nWrite: 1.0 KiB/s");
    EXPECT_EQ(firstRowText(model, Column::Memory, Qt::ToolTipRole), processMemoryToolTip(explorer));
}

TEST(ProcessModel, Data_Memory_ShowsPrivateWorkingSetNotSharedPages)
{
    ProcessModel model;
    ProcessInfo browser = process(800, "browser.exe");
    browser.workingSetBytes = 397'082'214;        // 378.7 MiB, of which 171.7 MiB is shared
    browser.privateWorkingSetBytes = 217'055'232; // 207.0 MiB

    model.updateProcesses({browser}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Memory), "207.0 MiB");
    EXPECT_TRUE(firstRowText(model, Column::Memory, Qt::ToolTipRole).contains("Working set: 378.7 MiB"));
}

TEST(ProcessModel, Data_ZeroRates_ShowZeroNotMissing)
{
    ProcessModel model;
    ProcessInfo idle = process(800, "idle.exe");
    idle.cpuUsagePercent = 0.0f;
    idle.ioReadBytesPerSec = 0;
    idle.ioWriteBytesPerSec = 0;

    model.updateProcesses({idle}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Cpu), "0.0%");
    EXPECT_EQ(firstRowText(model, Column::Io), "0 B/s");
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Cpu)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, Data_MissingValues_ShowNotAvailableInPlaceholderColour)
{
    ProcessModel model;
    ProcessInfo firstSeen = process(800, "explorer.exe");
    firstSeen.ioReadBytesPerSec = 4096;

    model.updateProcesses({firstSeen}, kNow);

    for (const Column column : {Column::Cpu, Column::Io, Column::Network}) {
        EXPECT_EQ(firstRowText(model, column), "N/A") << static_cast<int>(column);
        EXPECT_TRUE(model.index(0, static_cast<int>(column)).data(Qt::ForegroundRole).isValid())
            << static_cast<int>(column);
    }
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), "Read: 4.0 KiB/s\nWrite: N/A");
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Name)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, UpdateProcesses_SameValues_SignalsNothing)
{
    ProcessModel model;
    ProcessInfo system = process(4, "System");
    system.cpuUsagePercent = 1.5f;
    system.ioReadBytesPerSec = 100;
    model.updateProcesses({system, process(12, "smss.exe")}, kNow);
    ModelSignalLog log(model);

    model.updateProcesses({system, process(12, "smss.exe")}, kNow);

    EXPECT_TRUE(log.changed.empty());
    EXPECT_TRUE(log.inserted.empty());
    EXPECT_TRUE(log.removed.empty());
}

TEST(ProcessModel, UpdateProcesses_OneValueChanged_SignalsOnlyThatCellWithValueRoles)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe"), process(20, "csrss.exe")}, kNow);
    ModelSignalLog log(model);
    QList<int> roles;
    QObject::connect(
        &model, &ProcessModel::dataChanged, &model,
        [&roles](const QModelIndex&, const QModelIndex&, const QList<int>& changedRoles) { roles = changedRoles; });

    ProcessInfo smss = process(12, "smss.exe");
    smss.cpuUsagePercent = 0.5f;
    model.updateProcesses({process(4, "System"), smss, process(20, "csrss.exe")}, kNow);

    ASSERT_EQ(log.changed.size(), 1u);
    EXPECT_EQ(log.changed[0], (CellRange{.firstRow = 1,
                                         .lastRow = 1,
                                         .firstColumn = static_cast<int>(Column::Cpu),
                                         .lastColumn = static_cast<int>(Column::Cpu)}));
    EXPECT_TRUE(roles.contains(Qt::DisplayRole));
    EXPECT_TRUE(roles.contains(Qt::ToolTipRole));
    EXPECT_TRUE(roles.contains(Qt::ForegroundRole));
    EXPECT_TRUE(roles.contains(Qt::DecorationRole));
}

TEST(ProcessModel, UpdateProcesses_ChangesInSeveralRows_OneBlockCoveringThem)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(12, "smss.exe"), process(20, "csrss.exe")}, kNow);
    ModelSignalLog log(model);

    ProcessInfo system = process(4, "System");
    system.workingSetBytes = 8192;
    ProcessInfo csrss = process(20, "csrss.exe");
    csrss.handleCount = 900;
    model.updateProcesses({system, process(12, "smss.exe"), csrss}, kNow);

    ASSERT_EQ(log.changed.size(), 1u);
    EXPECT_EQ(log.changed[0], (CellRange{.firstRow = 0,
                                         .lastRow = 2,
                                         .firstColumn = static_cast<int>(Column::Memory),
                                         .lastColumn = static_cast<int>(Column::Handles)}));
}

TEST(ProcessModel, UpdateProcesses_EachValueColumn_DetectedOnItsOwn)
{
    const auto changedColumnAfter = [](auto change) {
        ProcessModel model;
        model.updateProcesses({process(4, "System")}, kNow);
        ModelSignalLog log(model);
        ProcessInfo system = process(4, "System");
        change(system);
        model.updateProcesses({system}, kNow);
        EXPECT_EQ(log.changed.size(), 1u);
        EXPECT_TRUE(log.changed.empty() || log.changed[0].firstColumn == log.changed[0].lastColumn);
        return log.changed.empty() ? -1 : log.changed[0].firstColumn;
    };

    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.detailsStatus = ProcessDetailsStatus::AccessDenied; }),
              static_cast<int>(Column::Name));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.imagePath = "C:/app.exe"; }), static_cast<int>(Column::Name));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.commandLine = "app"; }), static_cast<int>(Column::Name));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.cpuUsagePercent = 0.0f; }), static_cast<int>(Column::Cpu));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.privateWorkingSetBytes = 1; }),
              static_cast<int>(Column::Memory));
    // The Memory cell's tooltip shows these two.
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.workingSetBytes = 1; }), static_cast<int>(Column::Memory));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.privateBytes = 1; }), static_cast<int>(Column::Memory));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.ioReadBytesPerSec = 0; }), static_cast<int>(Column::Io));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.ioWriteBytesPerSec = 0; }), static_cast<int>(Column::Io));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.threadCount = 1; }), static_cast<int>(Column::Threads));
    EXPECT_EQ(changedColumnAfter([](ProcessInfo& p) { p.handleCount = 1; }), static_cast<int>(Column::Handles));
}

TEST(ProcessModel, Data_NumbersAlignRightAndNamesDoNot)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")}, kNow);

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

    model.updateProcesses(
        {
            child(0, "", 0),
            child(4, "System", 0),
            child(100, "smss.exe", 4),
            child(200, "csrss.exe", 150),
            child(300, "wininit.exe", 150),
            child(400, "services.exe", 300),
            child(500, "svchost.exe", 400),
            child(600, "svchost.exe", 400),
        },
        kNow);

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

    model.updateProcesses({child(500, "svchost.exe", 400), child(400, "services.exe", 300)}, kNow);

    EXPECT_EQ(treeText(model), "services.exe\n  svchost.exe\n");
}

TEST(ProcessModel, TreeMode_ParentPidReusedByYoungerProcess_NotAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    // The orphan's parent exited and PID 400 now belongs to a later process.
    model.updateProcesses({child(500, "orphan.exe", 400, 500), child(400, "newcomer.exe", 0, 900)}, kNow);

    EXPECT_EQ(treeText(model), "orphan.exe\nnewcomer.exe\n");
}

TEST(ProcessModel, TreeMode_ParentCreatedInSameTick_IsAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(400, "launcher.exe", 0, 500), child(500, "app.exe", 400, 500)}, kNow);

    EXPECT_EQ(treeText(model), "launcher.exe\n  app.exe\n");
}

TEST(ProcessModel, TreeMode_ProcessesNamingEachOther_NoCycle)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(10, "a.exe", 20, 5), child(20, "b.exe", 10, 5), child(30, "self.exe", 30)}, kNow);

    EXPECT_EQ(treeText(model), "b.exe\n  a.exe\nself.exe\n");
}

TEST(ProcessModel, TreeMode_NewChildOfShownParent_InsertedUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(400, "services.exe", 0)}, kNow);
    const QPersistentModelIndex services = model.index(0, 0);
    ModelSignalLog log(model);

    model.updateProcesses({child(400, "services.exe", 0), child(500, "svchost.exe", 400)}, kNow);

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
                           child(600, "svchost.exe", 400), child(700, "spoolsv.exe", 400)},
                          kNow);

    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 0}, {0, 2}}));
}

TEST(ProcessModel, TreeMode_ParentExits_ChildrenMoveToTopLevel)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4), child(200, "user.exe", 100)}, kNow);
    const QPersistentModelIndex user = model.indexOf(child(200, "user.exe", 100).identity());
    ModelSignalLog log(model);

    model.updateProcesses({child(4, "System", 0), child(200, "user.exe", 100)}, kNow);
    // The exited parent stays, with its child under it, until its highlight ends.
    EXPECT_EQ(treeText(model), "System\n  smss.exe\n    user.exe\n");
    model.updateTime(kLater);

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
        {child(10, "a.exe", 0), child(20, "b.exe", 10), child(30, "c.exe", 20), child(40, "d.exe", 10)}, kNow);

    model.updateProcesses({child(30, "c.exe", 20), child(40, "d.exe", 10)}, kNow);
    model.updateTime(kLater);

    EXPECT_EQ(treeText(model), "c.exe\nd.exe\n");
}

TEST(ProcessModel, TreeMode_ValuesChange_OneSignalPerParentWithChanges)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(
        {child(4, "System", 0), child(100, "smss.exe", 4), child(200, "other.exe", 0), child(300, "quiet.exe", 200)},
        kNow);
    ModelSignalLog log(model);

    ProcessInfo system = child(4, "System", 0);
    system.threadCount = 200;
    ProcessInfo smss = child(100, "smss.exe", 4);
    smss.workingSetBytes = 4096;
    ProcessInfo other = child(200, "other.exe", 0);
    other.cpuUsagePercent = 1.0f;
    model.updateProcesses({system, smss, other, child(300, "quiet.exe", 200)}, kNow);

    // The top level's two rows, and System's one child. Nothing changed under other.exe.
    ASSERT_EQ(log.changed.size(), 2u);
    std::ranges::sort(log.changed, {}, &CellRange::lastRow);
    EXPECT_EQ(log.changed[0], (CellRange{.firstRow = 0,
                                         .lastRow = 0,
                                         .firstColumn = static_cast<int>(Column::Memory),
                                         .lastColumn = static_cast<int>(Column::Memory)}));
    EXPECT_EQ(log.changed[1], (CellRange{.firstRow = 0,
                                         .lastRow = 1,
                                         .firstColumn = static_cast<int>(Column::Cpu),
                                         .lastColumn = static_cast<int>(Column::Threads)}));
}

TEST(ProcessModel, SetTreeMode_Toggle_ResetsOnceAndRebuilds)
{
    ProcessModel model;
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)}, kNow);
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

    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)}, kNow);

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

TEST(ProcessModel, TreeMode_RandomChurn_StructureHighlightsAndPersistentIndexesStayConsistent)
{
    using std::chrono::seconds;
    using std::chrono::steady_clock;

    std::mt19937 random(20261014);
    ProcessModel model;
    model.setTreeMode(true);
    // A sorted proxy on top, as in the view, checks that the signals are consistent.
    sysmon::ui::ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);

    std::vector<ProcessInfo> live;
    // When each process left the list or joined it, as the model was told.
    std::unordered_map<ProcessIdentity, steady_clock::time_point, ProcessIdentityHash> exitedAt;
    std::unordered_map<ProcessIdentity, steady_clock::time_point, ProcessIdentityHash> startedAt;
    std::vector<std::pair<QPersistentModelIndex, ProcessIdentity>> tracked;

    const auto isLive = [&live](const ProcessIdentity& identity) {
        return std::ranges::any_of(live, [&identity](const ProcessInfo& info) { return info.identity() == identity; });
    };
    const auto isRecent = [](const auto& times, const ProcessIdentity& identity, steady_clock::time_point now) {
        const auto it = times.find(identity);
        return it != times.end() && now - it->second < ProcessModel::kHighlightDuration;
    };
    const auto check = [&](steady_clock::time_point now, int step) {
        for (const auto& [index, identity] : tracked) {
            const bool isShown = isLive(identity) || isRecent(exitedAt, identity, now);
            ASSERT_EQ(index.isValid(), isShown) << "step " << step;
            if (isShown) {
                ASSERT_EQ(model.process(index)->identity(), identity) << "step " << step;
            }
        }

        std::size_t rows = 0;
        forEachRow(model, [&](const QModelIndex& index) {
            ++rows;
            const ProcessInfo* childInfo = model.process(index);
            ASSERT_EQ(model.indexOf(childInfo->identity()), index);
            const bool isHighlighted =
                isRecent(exitedAt, childInfo->identity(), now) ||
                (isLive(childInfo->identity()) && isRecent(startedAt, childInfo->identity(), now));
            EXPECT_EQ(index.data(Qt::BackgroundRole).isValid(), isHighlighted) << "step " << step;
            const QModelIndex parent = model.parent(index);
            if (parent.isValid()) {
                const ProcessInfo* parentInfo = model.process(parent);
                EXPECT_EQ(parentInfo->pid, childInfo->parentPid);
                EXPECT_LE(parentInfo->creationTime, childInfo->creationTime);
                // A process that had exited before the child was first listed
                // cannot have started it. A child without a start time was in
                // the first list, before anything exited.
                const auto parentExited = exitedAt.find(parentInfo->identity());
                const auto childStarted = startedAt.find(childInfo->identity());
                if (parentExited != exitedAt.end() && childStarted != startedAt.end()) {
                    EXPECT_GE(parentExited->second, childStarted->second) << "step " << step;
                }
            }
        });
        const auto recentlyExited = std::ranges::count_if(
            exitedAt, [now](const auto& entry) { return now - entry.second < ProcessModel::kHighlightDuration; });
        ASSERT_EQ(rows, live.size() + static_cast<std::size_t>(recentlyExited)) << "step " << step;

        std::size_t proxyRows = 0;
        forEachRow(proxy, [&proxyRows](const QModelIndex&) { ++proxyRows; });
        ASSERT_EQ(proxyRows, rows) << "step " << step;
    };

    int64_t clock = 1;
    for (int step = 0; step < 300; ++step) {
        // A refresh every 2 seconds and a tick without a list between them, as the scheduler sends them.
        const auto now = kNow + seconds{2 * step};
        tracked.clear();
        forEachRow(model, [&](const QModelIndex& index) {
            tracked.emplace_back(QPersistentModelIndex(index), model.process(index)->identity());
        });

        std::erase_if(live, [&](const ProcessInfo& info) {
            const bool exits = random() % 8 == 0;
            if (exits) {
                exitedAt[info.identity()] = now;
            }
            return exits;
        });
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
            if (step > 0) {
                startedAt[info.identity()] = now;
            }
            live.push_back(info);
        }

        model.updateProcesses(live, now);
        check(now, step);
        model.updateTime(now + seconds{1});
        check(now + seconds{1}, step);
    }
}

namespace
{

bool isHighlighted(const QModelIndex& index)
{
    return index.data(Qt::BackgroundRole).isValid();
}

} // namespace

TEST(ProcessModel, Highlights_FirstList_NothingHighlighted)
{
    ProcessModel model;

    model.updateProcesses({process(4, "System"), process(12, "smss.exe")}, kNow);

    EXPECT_FALSE(isHighlighted(model.index(0, 0)));
    EXPECT_FALSE(isHighlighted(model.index(1, 0)));
}

TEST(ProcessModel, Highlights_NewProcess_TintedUntilHighlightEnds)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")}, kNow);

    model.updateProcesses({process(4, "System"), process(900, "notepad.exe")}, kNow);
    const QModelIndex notepad = model.indexOf(process(900, "notepad.exe").identity());

    EXPECT_TRUE(isHighlighted(notepad));
    EXPECT_TRUE(isHighlighted(model.indexOf(process(900, "notepad.exe").identity(), Column::Handles)));
    EXPECT_FALSE(isHighlighted(model.index(0, 0)));
    EXPECT_FALSE(notepad.data(Qt::FontRole).isValid());
    EXPECT_TRUE(notepad.data(Qt::ToolTipRole).toString().contains("started"));

    model.updateTime(kLater - std::chrono::milliseconds{1});
    EXPECT_TRUE(isHighlighted(notepad));
    model.updateTime(kLater);
    EXPECT_FALSE(isHighlighted(notepad));
    EXPECT_FALSE(notepad.data(Qt::ToolTipRole).toString().contains("started"));
}

TEST(ProcessModel, Highlights_ExitedProcess_TintedItalicWithToolTipThenRemoved)
{
    ProcessModel model;
    ProcessInfo notepad = process(900, "notepad.exe");
    notepad.threadCount = 7;
    model.updateProcesses({process(4, "System"), notepad}, kNow);

    model.updateProcesses({process(4, "System")}, kNow);
    const QPersistentModelIndex row = model.indexOf(notepad.identity());

    ASSERT_TRUE(row.isValid());
    EXPECT_TRUE(isHighlighted(row));
    EXPECT_NE(row.data(Qt::BackgroundRole), model.index(0, 0).data(Qt::BackgroundRole));
    EXPECT_TRUE(row.data(Qt::FontRole).value<QFont>().italic());
    EXPECT_TRUE(row.data(Qt::ToolTipRole).toString().contains("exited"));
    // The last values stay on show.
    EXPECT_EQ(model.process(row)->threadCount, 7u);

    model.updateProcesses({process(4, "System")}, kLater);
    EXPECT_FALSE(row.isValid());
    EXPECT_EQ(model.rowCount(), 1);
}

TEST(ProcessModel, Highlights_StartedAndExitedTintsDiffer)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "old.exe")}, kNow);

    model.updateProcesses({process(4, "System"), process(950, "new.exe")}, kNow);

    const QVariant exited = model.indexOf(process(900, "old.exe").identity()).data(Qt::BackgroundRole);
    const QVariant started = model.indexOf(process(950, "new.exe").identity()).data(Qt::BackgroundRole);
    EXPECT_TRUE(exited.isValid());
    EXPECT_TRUE(started.isValid());
    EXPECT_NE(exited, started);
}

TEST(ProcessModel, Highlights_ExitedProcessListedAgain_NoLongerMarked)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe")}, kNow);
    model.updateProcesses({process(4, "System")}, kNow);

    model.updateProcesses({process(4, "System"), process(900, "notepad.exe")}, kNow + std::chrono::seconds{1});
    model.updateTime(kLater + std::chrono::seconds{10});

    const QModelIndex notepad = model.indexOf(process(900, "notepad.exe").identity());
    ASSERT_TRUE(notepad.isValid());
    EXPECT_FALSE(isHighlighted(notepad));
    EXPECT_FALSE(model.hasExited(notepad));
}

TEST(ProcessModel, HasExited_TrueOnlyForRowsMarkedAsExited)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe")}, kNow);
    EXPECT_FALSE(model.hasExited(model.indexOf(process(900, "notepad.exe").identity())));

    model.updateProcesses({process(4, "System")}, kNow);

    EXPECT_TRUE(model.hasExited(model.indexOf(process(900, "notepad.exe").identity())));
    EXPECT_TRUE(model.hasExited(model.indexOf(process(900, "notepad.exe").identity(), Column::Handles)));
    EXPECT_FALSE(model.hasExited(model.indexOf(process(4, "System").identity())));
    EXPECT_FALSE(model.hasExited(QModelIndex{}));
}

TEST(ProcessModel, Highlights_Changes_SignalledForTheWholeRowWithHighlightRoles)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe")}, kNow);
    std::vector<std::pair<CellRange, QList<int>>> emitted;
    QObject::connect(&model, &ProcessModel::dataChanged, &model,
                     [&emitted](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
                         emitted.emplace_back(CellRange{.firstRow = topLeft.row(),
                                                        .lastRow = bottomRight.row(),
                                                        .firstColumn = topLeft.column(),
                                                        .lastColumn = bottomRight.column()},
                                              roles);
                     });

    model.updateProcesses({process(4, "System")}, kNow);

    ASSERT_EQ(emitted.size(), 1u);
    EXPECT_EQ(emitted[0].first,
              (CellRange{.firstRow = 1, .lastRow = 1, .firstColumn = 0, .lastColumn = ProcessModel::kColumnCount - 1}));
    EXPECT_TRUE(emitted[0].second.contains(Qt::BackgroundRole));
    EXPECT_TRUE(emitted[0].second.contains(Qt::FontRole));
    EXPECT_TRUE(emitted[0].second.contains(Qt::ToolTipRole));
}

TEST(ProcessModel, UpdateTime_WithoutList_EndsHighlightsAndRemovesExitedRows)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), process(900, "old.exe")}, kNow);
    model.updateProcesses({process(4, "System"), process(950, "new.exe")}, kNow);

    model.updateTime(kLater);

    EXPECT_EQ(rowNames(model), (std::vector<QString>{"System", "new.exe"}));
    EXPECT_FALSE(isHighlighted(model.index(1, 0)));
}

TEST(ProcessModel, TreeMode_ChildOfProcessThatJustExited_ShownUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(400, "launcher.exe", 4)}, kNow);

    // The launcher started a child and exited between two refreshes.
    const auto refresh = kNow + std::chrono::seconds{2};
    model.updateProcesses({child(4, "System", 0), child(500, "app.exe", 400)}, refresh);

    EXPECT_EQ(treeText(model), "System\n  launcher.exe\n    app.exe\n");
    model.updateTime(refresh + ProcessModel::kHighlightDuration);
    EXPECT_EQ(treeText(model), "System\napp.exe\n");
}

TEST(ProcessModel, TreeMode_ChildListedAfterExitedRowWasMarked_NotUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "old.exe", 4)}, kNow);
    model.updateProcesses({child(4, "System", 0)}, kNow + std::chrono::seconds{1});

    // A launcher that was never listed reused PID 100, started child.exe and
    // exited, all after old.exe had exited. old.exe's row is still shown.
    model.updateProcesses({child(4, "System", 0), child(500, "child.exe", 100)}, kNow + std::chrono::seconds{2});

    EXPECT_EQ(treeText(model), "System\n  old.exe\nchild.exe\n");
}

TEST(ProcessModel, SetTreeMode_ChildListedAfterExitedRowWasMarked_NotLinkedToIt)
{
    ProcessModel model;
    model.updateProcesses({child(4, "System", 0), child(100, "old.exe", 4)}, kNow);
    model.updateProcesses({child(4, "System", 0)}, kNow + std::chrono::seconds{1});
    model.updateProcesses({child(4, "System", 0), child(500, "child.exe", 100)}, kNow + std::chrono::seconds{2});

    model.setTreeMode(true);

    EXPECT_EQ(treeText(model), "System\n  old.exe\nchild.exe\n");
}

TEST(ProcessModel, TreeMode_PidReusedWhileExitedRowShown_ChildFindsTheNewProcess)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(400, "old.exe", 0, 400)}, kNow);

    model.updateProcesses({child(400, "new.exe", 0, 450), child(500, "app.exe", 400, 460)}, kNow);

    EXPECT_EQ(treeText(model), "old.exe\nnew.exe\n  app.exe\n");
}

namespace
{

// The icon's 16-pixel image, as the view draws it at 100% scale.
QImage iconImage(const QVariant& decoration)
{
    return decoration.value<QIcon>().pixmap(16, 16).toImage();
}

bool hasVisiblePixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                return true;
            }
        }
    }
    return false;
}

ProcessInfo withDetails(uint32_t pid, ProcessDetailsStatus status)
{
    ProcessInfo info = process(pid, "p" + std::to_string(pid));
    info.detailsStatus = status;
    return info;
}

} // namespace

TEST(ProcessModel, Data_NameColumn_AccessIndicatorPerDetailsStatus)
{
    ProcessModel model;
    model.updateProcesses(
        {withDetails(1, ProcessDetailsStatus::Available), withDetails(2, ProcessDetailsStatus::AccessDenied),
         withDetails(3, ProcessDetailsStatus::NotQueried), withDetails(4, ProcessDetailsStatus::Unavailable)},
        kNow);

    std::vector<QImage> images;
    for (int row = 0; row < 4; ++row) {
        const QVariant decoration = model.index(row, static_cast<int>(Column::Name)).data(Qt::DecorationRole);
        ASSERT_TRUE(decoration.canConvert<QIcon>()) << row;
        images.push_back(iconImage(decoration));
        EXPECT_FALSE(model.index(row, static_cast<int>(Column::Pid)).data(Qt::DecorationRole).isValid()) << row;
    }

    // A blank icon keeps the names of processes with details aligned with the others.
    EXPECT_EQ(images[0].size(), images[1].size());
    EXPECT_FALSE(hasVisiblePixel(images[0]));
    for (std::size_t status = 1; status < images.size(); ++status) {
        EXPECT_TRUE(hasVisiblePixel(images[status])) << status;
    }
    EXPECT_NE(images[1], images[2]);
    EXPECT_NE(images[1], images[3]);
    EXPECT_NE(images[2], images[3]);
}

TEST(ProcessModel, Data_NameToolTip_GivesDetailsOrWhyMissing)
{
    ProcessModel model;
    ProcessInfo available = withDetails(1, ProcessDetailsStatus::Available);
    available.imagePath = "C:/Apps/p1.exe";
    available.commandLine = "p1.exe --run";
    model.updateProcesses({available, withDetails(2, ProcessDetailsStatus::AccessDenied)}, kNow);

    EXPECT_EQ(model.index(0, 0).data(Qt::ToolTipRole).toString(),
              "<p><b>Path:</b> C:/Apps/p1.exe</p><p><b>Command line:</b> p1.exe --run</p>");
    EXPECT_TRUE(model.index(1, 0).data(Qt::ToolTipRole).toString().startsWith("<p>Access denied:"));
}

TEST(ProcessModel, Data_NameToolTip_ExitedProcess_AddsExitedAfterDetails)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System"), withDetails(2, ProcessDetailsStatus::AccessDenied)}, kNow);

    model.updateProcesses({process(4, "System")}, kNow);

    const QString toolTip =
        model.indexOf(withDetails(2, ProcessDetailsStatus::AccessDenied).identity()).data(Qt::ToolTipRole).toString();
    EXPECT_TRUE(toolTip.startsWith("<p>Access denied:"));
    EXPECT_TRUE(toolTip.endsWith("<p>This process has exited; its row is removed in a moment.</p>"));
}

TEST(ProcessModel, HeaderData_NameToolTip_ExplainsTheIndicator)
{
    ProcessModel model;

    const QString toolTip =
        model.headerData(static_cast<int>(Column::Name), Qt::Horizontal, Qt::ToolTipRole).toString();

    EXPECT_TRUE(toolTip.contains("icon"));
    EXPECT_TRUE(toolTip.contains("could not be read"));
}
