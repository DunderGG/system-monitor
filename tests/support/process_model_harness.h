#pragma once

#include <chrono>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QModelIndex>
#include <QObject>
#include <QString>

#include "ui/process_model.h"

// Checks of a ui::ProcessModel that the process model, proxy and view tests share.

namespace sysmon::tests
{

/** The monotonic time of each refresh, unless a test moves it on. */
inline const auto kNow = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
/** When the highlights of changes made at kNow have ended. */
inline const auto kLater = kNow + ui::ProcessModel::kHighlightDuration;

/**
 * Returns the rows under parent as text, one line per process indented two
 * spaces per level, e.g. "System\n  smss.exe\n". Checks along the way that
 * each row's index and parent agree.
 */
inline QString treeText(const ui::ProcessModel& model, const QModelIndex& parent = {}, int depth = 0)
{
    QString text;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        EXPECT_EQ(index.row(), row);
        EXPECT_EQ(model.parent(index), parent);
        EXPECT_EQ(model.parent(model.index(row, ui::ProcessModel::kColumnCount - 1, parent)), parent);
        EXPECT_EQ(model.rowCount(model.index(row, 1, parent)), 0);
        text += QString(depth * 2, ' ') + index.data().toString() + "\n";
        text += treeText(model, index, depth + 1);
    }
    return text;
}

/** A block of cells named in one dataChanged signal. */
struct CellRange
{
    int firstRow{0};
    int lastRow{0};
    int firstColumn{0};
    int lastColumn{0};

    friend bool operator==(const CellRange&, const CellRange&) = default;
};

/** Records the signals a model emits: inserted and removed rows as (first, last) ranges, and changed cells. */
struct ModelSignalLog
{
    std::vector<std::pair<int, int>> inserted;
    std::vector<std::pair<int, int>> removed;
    std::vector<CellRange> changed;
    int moves = 0;
    int resets = 0;

    explicit ModelSignalLog(ui::ProcessModel& model)
    {
        QObject::connect(
            &model, &ui::ProcessModel::rowsInserted, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { inserted.emplace_back(first, last); });
        QObject::connect(
            &model, &ui::ProcessModel::rowsRemoved, &model,
            [this](const QModelIndex& /*parent*/, int first, int last) { removed.emplace_back(first, last); });
        QObject::connect(&model, &ui::ProcessModel::dataChanged, &model,
                         [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
                             changed.push_back(CellRange{.firstRow = topLeft.row(),
                                                         .lastRow = bottomRight.row(),
                                                         .firstColumn = topLeft.column(),
                                                         .lastColumn = bottomRight.column()});
                         });
        QObject::connect(&model, &ui::ProcessModel::rowsMoved, &model, [this]() { ++moves; });
        QObject::connect(&model, &ui::ProcessModel::modelReset, &model, [this]() { ++resets; });
    }
};

/** Returns the text the model shows for the first row in column. */
inline QString firstRowText(const ui::ProcessModel& model, ui::ProcessModel::Column column, int role = Qt::DisplayRole)
{
    return model.index(0, static_cast<int>(column)).data(role).toString();
}

/** Returns the names in the model's top-level rows, top to bottom. */
inline std::vector<QString> rowNames(const ui::ProcessModel& model)
{
    std::vector<QString> names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.push_back(model.index(row, static_cast<int>(ui::ProcessModel::Column::Name)).data().toString());
    }
    return names;
}

} // namespace sysmon::tests
