#include "ui/readout_grid.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include <QFont>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QMargins>
#include <QPalette>
#include <QRect>
#include <QSize>

namespace sysmon::ui
{

namespace
{

constexpr int kValuePointSize = 11;
constexpr int kColumnSpacing = 24;
constexpr int kNameValueSpacing = 2;
constexpr int kRowSpacing = 14;

} // namespace

/**
 * Lays out ReadoutGrid's labels as cells of a name above its value, filled
 * row by row into as many columns as the width allows (at most maxColumns).
 * Items are added in pairs: the name label, then its value label.
 *
 * Spare width is shared so the columns come out equally wide where their
 * content allows, as a QGridLayout with equal column stretch would. Each
 * cell's width is the widest it has been since the cells were added (see
 * ReadoutGrid), so a value that shrinks does not add a column back.
 */
class ReadoutLayout : public QLayout
{
public:
    ReadoutLayout(int maxColumns, QWidget* parent) : QLayout(parent), m_maxColumns(std::max(1, maxColumns)) {}

    // m_items owns the layout items; the labels they manage belong to the grid.
    ~ReadoutLayout() override = default;

    void addItem(QLayoutItem* item) override
    {
        m_items.emplace_back(item);
        invalidate();
    }

    [[nodiscard]] QLayoutItem* itemAt(int index) const override
    {
        return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].get() : nullptr;
    }

    QLayoutItem* takeAt(int index) override
    {
        if (index < 0 || index >= count()) {
            return nullptr;
        }
        const auto position = m_items.begin() + index;
        QLayoutItem* item = position->release();
        m_items.erase(position);
        // Removing one label leaves the cells misaligned; only removing all of them is expected.
        m_widestSeen.clear();
        invalidate();
        return item;
    }

    [[nodiscard]] int count() const override
    {
        return static_cast<int>(m_items.size());
    }

    /** Removes every item without deleting the labels, and forgets the widths they reached. */
    void clear()
    {
        m_items.clear();
        m_widestSeen.clear();
        invalidate();
    }

    [[nodiscard]] Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int columns = fullColumns();
        return withMargins({totalWidth(columnNeeds(columns)), heightFor(columns)});
    }

    // The minimum height is the height at the full column count; a narrower
    // grid asks for the rows it needs through heightForWidth().
    [[nodiscard]] QSize minimumSize() const override
    {
        return withMargins({totalWidth(columnNeeds(1)), heightFor(fullColumns())});
    }

    [[nodiscard]] bool hasHeightForWidth() const override
    {
        return true;
    }

    [[nodiscard]] int heightForWidth(int width) const override
    {
        const QMargins margins = contentsMargins();
        return heightFor(columnsFor(width - margins.left() - margins.right())) + verticalMargins();
    }

    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        m_widestSeen.resize(static_cast<std::size_t>(cellCount()), 0);
        for (int cell = 0; cell < cellCount(); ++cell) {
            m_widestSeen[static_cast<std::size_t>(cell)] = cellWidth(cell);
        }

        const QRect area = contentsRect();
        m_columns = columnsFor(area.width());
        const std::vector<int> widths = columnWidths(m_columns, area.width());
        int top = area.top();
        for (int rowStart = 0; rowStart < cellCount(); rowStart += m_columns) {
            int left = area.left();
            int rowHeight = 0;
            for (int column = 0; column < m_columns && rowStart + column < cellCount(); ++column) {
                const int cell = rowStart + column;
                const int width = widths[static_cast<std::size_t>(column)];
                const int nameHeight = name(cell)->sizeHint().height();
                const int valueHeight = value(cell)->sizeHint().height();
                name(cell)->setGeometry(QRect(left, top, width, nameHeight));
                value(cell)->setGeometry(QRect(left, top + nameHeight + kNameValueSpacing, width, valueHeight));
                rowHeight = std::max(rowHeight, cellHeight(cell));
                left += width + kColumnSpacing;
            }
            top += rowHeight + kRowSpacing;
        }
    }

    /** The number of columns used by the last setGeometry(). */
    [[nodiscard]] int columns() const
    {
        return m_columns;
    }

private:
    [[nodiscard]] int cellCount() const
    {
        return count() / 2;
    }

    [[nodiscard]] int fullColumns() const
    {
        return std::max(1, std::min(m_maxColumns, cellCount()));
    }

    [[nodiscard]] QLayoutItem* name(int cell) const
    {
        return m_items[static_cast<std::size_t>(cell) * 2].get();
    }

    [[nodiscard]] QLayoutItem* value(int cell) const
    {
        return m_items[static_cast<std::size_t>(cell) * 2 + 1].get();
    }

    [[nodiscard]] int cellWidth(int cell) const
    {
        const int current = std::max(name(cell)->sizeHint().width(), value(cell)->sizeHint().width());
        const auto index = static_cast<std::size_t>(cell);
        return index < m_widestSeen.size() ? std::max(current, m_widestSeen[index]) : current;
    }

    [[nodiscard]] int cellHeight(int cell) const
    {
        return name(cell)->sizeHint().height() + kNameValueSpacing + value(cell)->sizeHint().height();
    }

    // The width each column needs for its widest cell.
    [[nodiscard]] std::vector<int> columnNeeds(int columns) const
    {
        std::vector<int> needs(static_cast<std::size_t>(columns), 0);
        for (int cell = 0; cell < cellCount(); ++cell) {
            int& need = needs[static_cast<std::size_t>(cell % columns)];
            need = std::max(need, cellWidth(cell));
        }
        return needs;
    }

    [[nodiscard]] static int totalWidth(const std::vector<int>& columnWidths)
    {
        const int spacing = static_cast<int>(columnWidths.size() - 1) * kColumnSpacing;
        return std::accumulate(columnWidths.begin(), columnWidths.end(), 0) + spacing;
    }

    [[nodiscard]] int columnsFor(int width) const
    {
        for (int columns = std::min(m_maxColumns, cellCount()); columns > 1; --columns) {
            if (totalWidth(columnNeeds(columns)) <= width) {
                return columns;
            }
        }
        return 1;
    }

    // Shares the spare width equally: columns wider than an equal share keep
    // their width, and the rest split what is left.
    [[nodiscard]] std::vector<int> columnWidths(int columns, int width) const
    {
        std::vector<int> widths = columnNeeds(columns);
        std::vector<bool> keepsNeed(widths.size(), false);
        int remaining = width - (columns - 1) * kColumnSpacing;
        int sharing = columns;
        for (bool changed = true; changed && sharing > 0;) {
            changed = false;
            const int share = remaining / sharing;
            for (std::size_t column = 0; column < widths.size(); ++column) {
                if (!keepsNeed[column] && widths[column] > share) {
                    keepsNeed[column] = true;
                    remaining -= widths[column];
                    --sharing;
                    changed = true;
                }
            }
        }
        if (sharing > 0) {
            for (std::size_t column = 0; column < widths.size(); ++column) {
                if (!keepsNeed[column]) {
                    widths[column] = remaining / sharing;
                }
            }
        }
        return widths;
    }

    [[nodiscard]] int heightFor(int columns) const
    {
        int height = 0;
        for (int rowStart = 0; rowStart < cellCount(); rowStart += columns) {
            int rowHeight = 0;
            for (int cell = rowStart; cell < std::min(rowStart + columns, cellCount()); ++cell) {
                rowHeight = std::max(rowHeight, cellHeight(cell));
            }
            height += (rowStart > 0 ? kRowSpacing : 0) + rowHeight;
        }
        return height;
    }

    [[nodiscard]] QSize withMargins(QSize size) const
    {
        const QMargins margins = contentsMargins();
        return size.grownBy(margins);
    }

    [[nodiscard]] int verticalMargins() const
    {
        const QMargins margins = contentsMargins();
        return margins.top() + margins.bottom();
    }

    std::vector<std::unique_ptr<QLayoutItem>> m_items;
    std::vector<int> m_widestSeen;
    int m_maxColumns{1};
    int m_columns{1};
};

ReadoutGrid::ReadoutGrid(int maxColumns, QWidget* parent) : QWidget(parent)
{
    m_layout = new ReadoutLayout(maxColumns, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
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

int ReadoutGrid::columnCount() const
{
    return m_layout->columns();
}

void ReadoutGrid::rebuild(const std::vector<Readout>& readouts)
{
    m_layout->clear();
    for (auto* label : m_nameLabels) {
        label->hide();
        label->deleteLater();
    }
    for (auto* label : m_valueLabels) {
        label->hide();
        label->deleteLater();
    }

    std::vector<QLabel*> nameLabels;
    std::vector<QLabel*> valueLabels;
    for (const auto& readout : readouts) {
        auto* name = new QLabel(readout.name, this);
        name->setForegroundRole(QPalette::PlaceholderText);

        auto* value = new QLabel(readout.value, this);
        QFont valueFont = value->font();
        valueFont.setPointSize(kValuePointSize);
        value->setFont(valueFont);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        m_layout->addWidget(name);
        m_layout->addWidget(value);
        nameLabels.push_back(name);
        valueLabels.push_back(value);
    }

    m_nameLabels = std::move(nameLabels);
    m_valueLabels = std::move(valueLabels);
}

} // namespace sysmon::ui
