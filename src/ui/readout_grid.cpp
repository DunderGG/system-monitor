#include "ui/readout_grid.h"

#include <algorithm>
#include <utility>

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

namespace sysmon::ui
{

namespace
{

constexpr int kValuePointSize = 11;
constexpr int kColumnSpacing = 24;
constexpr int kRowSpacing = 2;
constexpr int kCellSpacing = 10;

} // namespace

ReadoutGrid::ReadoutGrid(int columns, QWidget* parent) : QWidget(parent), m_columns(std::max(1, columns))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_cells = new QWidget(this);
    m_layout->addWidget(m_cells);
}

void ReadoutGrid::setReadouts(const std::vector<Readout>& readouts)
{
    const bool sameNames = readouts.size() == m_nameLabels.size() &&
                           std::ranges::equal(readouts, m_nameLabels, [](const Readout& readout, const QLabel* label) {
                               return readout.name == label->text();
                           });
    if (!sameNames) {
        rebuild(readouts);
        return;
    }
    for (std::size_t index = 0; index < readouts.size(); ++index) {
        if (m_valueLabels[index]->text() != readouts[index].value) {
            m_valueLabels[index]->setText(readouts[index].value);
        }
    }
}

std::vector<Readout> ReadoutGrid::readouts() const
{
    std::vector<Readout> result;
    result.reserve(m_nameLabels.size());
    for (std::size_t index = 0; index < m_nameLabels.size(); ++index) {
        result.push_back({m_nameLabels[index]->text(), m_valueLabels[index]->text()});
    }
    return result;
}

QString ReadoutGrid::value(const QString& name) const
{
    for (std::size_t index = 0; index < m_nameLabels.size(); ++index) {
        if (m_nameLabels[index]->text() == name) {
            return m_valueLabels[index]->text();
        }
    }
    return {};
}

void ReadoutGrid::rebuild(const std::vector<Readout>& readouts)
{
    // A fresh container replaces the old one, so no stale rows or column
    // stretches are left behind in the grid layout.
    m_layout->removeWidget(m_cells);
    m_cells->hide();
    m_cells->deleteLater();
    m_cells = new QWidget(this);
    m_layout->addWidget(m_cells);

    auto* grid = new QGridLayout(m_cells);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(kColumnSpacing);
    grid->setVerticalSpacing(kRowSpacing);

    std::vector<QLabel*> nameLabels;
    std::vector<QLabel*> valueLabels;
    for (std::size_t index = 0; index < readouts.size(); ++index) {
        const int cellRow = static_cast<int>(index) / m_columns;
        const int column = static_cast<int>(index) % m_columns;

        auto* name = new QLabel(readouts[index].name, m_cells);
        name->setForegroundRole(QPalette::PlaceholderText);

        auto* value = new QLabel(readouts[index].value, m_cells);
        QFont valueFont = value->font();
        valueFont.setPointSize(kValuePointSize);
        value->setFont(valueFont);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        // Each readout takes two grid rows (name, value); a spacer row keeps cells apart.
        grid->addWidget(name, cellRow * 3, column, Qt::AlignLeft | Qt::AlignBottom);
        grid->addWidget(value, cellRow * 3 + 1, column, Qt::AlignLeft | Qt::AlignTop);
        grid->setRowMinimumHeight(cellRow * 3 + 2, kCellSpacing);

        nameLabels.push_back(name);
        valueLabels.push_back(value);
    }
    for (int column = 0; column < m_columns; ++column) {
        grid->setColumnStretch(column, 1);
    }

    m_nameLabels = std::move(nameLabels);
    m_valueLabels = std::move(valueLabels);
}

} // namespace sysmon::ui
