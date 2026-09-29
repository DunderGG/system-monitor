#pragma once

#include <vector>

#include <QString>
#include <QWidget>

#include "ui/performance_readouts.h"

class QLabel;
class QVBoxLayout;

namespace sysmon::ui
{

/**
 * A grid of numeric readouts in the Task Manager style: each cell shows a
 * dimmed name above its value, filling a fixed number of columns per row.
 * Values are selectable so they can be copied (e.g. IP addresses).
 *
 * setReadouts() updates the value labels in place while the names stay the
 * same, and rebuilds the cells only when the names change (e.g. a volume is
 * added).
 */
class ReadoutGrid : public QWidget
{
    Q_OBJECT

public:
    explicit ReadoutGrid(int columns, QWidget* parent = nullptr);
    ~ReadoutGrid() override = default;

    void setReadouts(const std::vector<Readout>& readouts);
    [[nodiscard]] std::vector<Readout> readouts() const;

    /** Returns the value shown for name, or an empty string if there is no such readout. */
    [[nodiscard]] QString value(const QString& name) const;

private:
    void rebuild(const std::vector<Readout>& readouts);

    int m_columns{1};
    QVBoxLayout* m_layout{nullptr};
    QWidget* m_cells{nullptr};
    std::vector<QLabel*> m_nameLabels;
    std::vector<QLabel*> m_valueLabels;
};

} // namespace sysmon::ui
