#include <algorithm>
#include <vector>

#include <gtest/gtest.h>
#include <QLabel>
#include <QLayout>
#include <QString>

#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

using namespace sysmon::ui;

namespace
{

constexpr int kMaxColumns = 5;

std::vector<Readout> readouts(const QString& namePrefix = "Name")
{
    std::vector<Readout> result;
    for (int index = 0; index < 10; ++index) {
        result.push_back({QString("%1 %2").arg(namePrefix).arg(index), "1"});
    }
    return result;
}

// Lays the grid out at width, and at the height it asks for at that width.
void layOut(ReadoutGrid& grid, int width)
{
    grid.resize(width, grid.heightForWidth(width));
    grid.layout()->invalidate();
    grid.layout()->activate();
}

} // namespace

TEST(ReadoutGrid, WideEnough_UsesMaximumColumns)
{
    ReadoutGrid grid(kMaxColumns);
    grid.setReadouts(readouts());

    layOut(grid, grid.sizeHint().width());

    EXPECT_EQ(grid.columnCount(), kMaxColumns);
}

TEST(ReadoutGrid, Narrower_ReflowsIntoFewerColumnsWithoutClipping)
{
    ReadoutGrid grid(kMaxColumns);
    grid.setReadouts(readouts());
    const int fullWidth = grid.sizeHint().width();

    layOut(grid, fullWidth / 2);

    EXPECT_LT(grid.columnCount(), kMaxColumns);
    EXPECT_GT(grid.heightForWidth(fullWidth / 2), grid.heightForWidth(fullWidth));
    for (const auto* label : grid.findChildren<QLabel*>()) {
        EXPECT_GE(label->width(), label->sizeHint().width()) << label->text().toStdString();
        EXPECT_LE(label->x() + label->width(), grid.width()) << label->text().toStdString();
        EXPECT_LE(label->y() + label->height(), grid.height()) << label->text().toStdString();
    }
}

TEST(ReadoutGrid, MinimumWidth_IsWidestReadoutInOneColumn)
{
    ReadoutGrid grid(kMaxColumns);
    auto withLongValue = readouts();
    withLongValue[3].value = "fd44:555c:2315:4176:c6d0:77e0:5d11:3727";
    grid.setReadouts(withLongValue);
    int widestLabel = 0;
    for (const auto* label : grid.findChildren<QLabel*>()) {
        widestLabel = std::max(widestLabel, label->sizeHint().width());
    }

    layOut(grid, grid.minimumSizeHint().width());

    EXPECT_EQ(grid.minimumSizeHint().width(), widestLabel);
    EXPECT_LT(grid.minimumSizeHint().width(), grid.sizeHint().width());
    EXPECT_EQ(grid.columnCount(), 1);
}

TEST(ReadoutGrid, ValueShrinksAgain_KeepsColumnCount)
{
    ReadoutGrid grid(kMaxColumns);
    auto withLongValue = readouts();
    withLongValue[4].value = QString(40, 'W');
    grid.setReadouts(withLongValue);
    const int width = grid.sizeHint().width() - 1;
    layOut(grid, width);
    const int columns = grid.columnCount();
    ASSERT_LT(columns, kMaxColumns);

    grid.setReadouts(readouts());
    layOut(grid, width);

    EXPECT_EQ(grid.columnCount(), columns);
}

TEST(ReadoutGrid, NamesChange_ForgetsWidestValues)
{
    ReadoutGrid grid(kMaxColumns);
    auto withLongValue = readouts();
    withLongValue[4].value = QString(40, 'W');
    grid.setReadouts(withLongValue);
    const int width = grid.sizeHint().width() - 1;
    layOut(grid, width);
    ASSERT_LT(grid.columnCount(), kMaxColumns);

    grid.setReadouts(readouts("Other"));
    layOut(grid, width);

    EXPECT_EQ(grid.columnCount(), kMaxColumns);
}

TEST(ReadoutGrid, SetReadouts_ShowsNamesAndValues)
{
    ReadoutGrid grid(3);

    grid.setReadouts({{"Processes", "367"}, {"Threads", "8,350"}});

    EXPECT_EQ(grid.value("Processes"), "367");
    EXPECT_EQ(grid.value("Threads"), "8,350");
    EXPECT_TRUE(grid.value("Handles").isEmpty());
}

TEST(ReadoutGrid, SetReadoutsWithSameNames_UpdatesValuesInPlace)
{
    ReadoutGrid grid(3);
    grid.setReadouts({{"Processes", "367"}});
    const QLabel* valueLabel = nullptr;
    for (const auto* label : grid.findChildren<QLabel*>()) {
        if (label->text() == "367") {
            valueLabel = label;
        }
    }
    ASSERT_NE(valueLabel, nullptr);

    grid.setReadouts({{"Processes", "368"}});

    EXPECT_EQ(grid.value("Processes"), "368");
    EXPECT_EQ(valueLabel->text(), "368"); // The same label, not a rebuilt one.
}

TEST(ReadoutGrid, SetReadoutsWithNewNames_Rebuilds)
{
    ReadoutGrid grid(2);
    grid.setReadouts({{"C:\\", "75%"}});

    grid.setReadouts({{"C:\\", "75%"}, {"D:\\", "10%"}});

    EXPECT_EQ(grid.readouts(), (std::vector<Readout>{{"C:\\", "75%"}, {"D:\\", "10%"}}));
}
