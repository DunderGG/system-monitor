#include <chrono>
#include <cstdint>
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

// Records the row signals a model emits, as (first, last) ranges.
struct ModelSignalLog
{
    std::vector<std::pair<int, int>> inserted;
    std::vector<std::pair<int, int>> removed;
    int resets = 0;

    explicit ModelSignalLog(ProcessModel& model)
    {
        QObject::connect(
            &model, &ProcessModel::rowsInserted, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { inserted.emplace_back(first, last); });
        QObject::connect(
            &model, &ProcessModel::rowsRemoved, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { removed.emplace_back(first, last); });
        QObject::connect(&model, &ProcessModel::modelReset, &model, [this]() { ++resets; });
    }
};

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

TEST(ProcessModel, HeaderData_NamesColumns)
{
    ProcessModel model;

    EXPECT_EQ(model.headerData(static_cast<int>(Column::Name), Qt::Horizontal).toString(), "Name");
    EXPECT_EQ(model.headerData(static_cast<int>(Column::Pid), Qt::Horizontal).toString(), "PID");
}

TEST(ProcessModel, Data_NumbersAlignRightAndNamesDoNot)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")});

    const auto alignment = [&model](Column column) {
        return model.index(0, static_cast<int>(column)).data(Qt::TextAlignmentRole);
    };

    EXPECT_FALSE(alignment(Column::Name).isValid());
    EXPECT_EQ(alignment(Column::Pid).value<Qt::Alignment>(), Qt::AlignRight | Qt::AlignVCenter);
}
