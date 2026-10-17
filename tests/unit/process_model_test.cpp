#include <chrono>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "support/process_model_harness.h"
#include "support/process_samples.h"
#include "ui/process_model.h"

// ProcessModel: rows added, kept and removed as process lists arrive, and the
// signals that report each change.

using namespace sysmon::domain;
using namespace sysmon::tests;
using namespace std::chrono_literals;
using sysmon::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

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
    model.updateProcesses({process(4, "System"), process(900, "notepad.exe", 100s)}, kNow);
    const QPersistentModelIndex oldNotepad = model.index(1, 0);

    model.updateProcesses({process(4, "System"), process(900, "calc.exe", 200s)}, kNow);
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
