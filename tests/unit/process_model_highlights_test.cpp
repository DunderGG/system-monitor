#include <chrono>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QFont>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>
#include <QVariant>

#include "domain/process_info.h"
#include "support/process_model_harness.h"
#include "support/process_samples.h"
#include "ui/process_model.h"

// ProcessModel: started and exited processes highlighted for a time, and
// exited rows removed when their highlight ends.

using namespace sysmon::domain;
using namespace sysmon::tests;
using namespace std::chrono_literals;
using sysmon::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

} // namespace

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
