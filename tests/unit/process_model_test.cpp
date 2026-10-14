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
