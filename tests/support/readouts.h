#pragma once

#include <ostream>
#include <vector>

#include <gtest/gtest.h>
#include <QLabel>
#include <QString>

#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

namespace verdandi::ui
{

/** Prints a readout in a failed expectation as {name, value, toolTip}. GoogleTest finds it by argument lookup. */
inline void PrintTo(const Readout& readout, std::ostream* stream)
{
    *stream << "{\"" << readout.name.toStdString() << "\", \"" << readout.value.toStdString() << "\"";
    if (!readout.toolTip.isEmpty()) {
        *stream << ", \"" << readout.toolTip.toStdString() << "\"";
    }
    *stream << "}";
}

} // namespace verdandi::ui

namespace verdandi::tests
{

/** Returns the value of the readout called name, failing the test if there is none. */
[[nodiscard]] inline QString valueOf(const std::vector<ui::Readout>& readouts, const QString& name)
{
    for (const auto& readout : readouts) {
        if (readout.name == name) {
            return readout.value;
        }
    }
    ADD_FAILURE() << "no readout named " << name.toStdString();
    return {};
}

/** Expects every label in grid to be at least as wide as its text and inside the grid. */
inline void expectLabelsFit(const ui::ReadoutGrid& grid)
{
    for (const auto* label : grid.findChildren<QLabel*>()) {
        EXPECT_GE(label->width(), label->sizeHint().width()) << label->text().toStdString();
        EXPECT_LE(label->x() + label->width(), grid.width()) << label->text().toStdString();
    }
}

} // namespace verdandi::tests
