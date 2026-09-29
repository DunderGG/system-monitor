#pragma once

#include <vector>

#include <QString>
#include <QWidget>

#include "ui/performance_readouts.h"

class QLabel;

namespace sysmon::ui
{

class ReadoutLayout;

/**
 * A grid of numeric readouts in the Task Manager style: each cell shows a
 * dimmed name above its value. Values are selectable so they can be copied
 * (e.g. IP addresses).
 *
 * The grid uses as many columns as fit its width, up to the maximum given to
 * the constructor, and grows taller as it narrows (it has a height for width).
 * Its minimum width is that of its widest readout, so it never forces a page
 * wider than its window. A column count, once given up because a value grew,
 * is not taken back when the value shrinks again; this keeps changing values
 * from making the grid re-flow on every update.
 *
 * setReadouts() updates the value labels in place while the names stay the
 * same, and rebuilds the cells only when the names change (e.g. a volume is
 * added).
 */
class ReadoutGrid : public QWidget
{
    Q_OBJECT

public:
    explicit ReadoutGrid(int maxColumns, QWidget* parent = nullptr);
    ~ReadoutGrid() override = default;

    void setReadouts(const std::vector<Readout>& readouts);
    [[nodiscard]] std::vector<Readout> readouts() const;

    /** Returns the value shown for name, or an empty string if there is no such readout. */
    [[nodiscard]] QString value(const QString& name) const;

    /** The number of columns in the current layout. */
    [[nodiscard]] int columnCount() const;

private:
    void rebuild(const std::vector<Readout>& readouts);

    ReadoutLayout* m_layout{nullptr};
    std::vector<QLabel*> m_nameLabels;
    std::vector<QLabel*> m_valueLabels;
};

} // namespace sysmon::ui
