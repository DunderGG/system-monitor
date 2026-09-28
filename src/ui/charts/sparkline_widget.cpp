#include "ui/charts/sparkline_widget.h"

#include <algorithm>
#include <cassert>

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace sysmon::ui::charts
{

namespace
{

// Headroom above the largest sample so the line does not touch the top edge.
constexpr float kAutoRangeHeadroom = 1.1f;
constexpr int kFillTopAlpha = 90;
constexpr qreal kLineWidth = 1.5;

} // namespace

YRange autoRange(std::span<const std::optional<float>> samples)
{
    float largest = 0.0f;
    for (const auto &sample : samples) {
        if (sample) {
            largest = std::max(largest, *sample);
        }
    }
    return YRange{.min = 0.0f, .max = largest > 0.0f ? largest * kAutoRangeHeadroom : 1.0f};
}

std::vector<QPolygonF> sparklineSegments(std::span<const std::optional<float>> samples,
                                         std::size_t capacity,
                                         const QRectF &area,
                                         YRange range)
{
    assert(capacity > 0 && "sparkline capacity must be greater than zero");
    assert(range.min < range.max && "sparkline range must not be empty");

    std::vector<QPolygonF> segments;
    if (samples.empty() || area.isEmpty()) {
        return segments;
    }

    // Only the newest `capacity` samples fit on the chart.
    const auto visible = samples.size() > capacity ? samples.last(capacity) : samples;
    const std::size_t firstSlot = capacity - visible.size();
    const double slotWidth = capacity > 1 ? area.width() / static_cast<double>(capacity - 1) : 0.0;
    const double span = static_cast<double>(range.max - range.min);

    QPolygonF current;
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const auto &sample = visible[i];
        if (!sample) {
            if (!current.isEmpty()) {
                segments.push_back(std::move(current));
                current = QPolygonF{};
            }
            continue;
        }

        const double slot = static_cast<double>(firstSlot + i);
        const double x = capacity > 1 ? area.left() + slot * slotWidth : area.right();
        const double clamped = std::clamp(static_cast<double>(*sample), static_cast<double>(range.min),
                                          static_cast<double>(range.max));
        const double y = area.bottom() - (clamped - range.min) / span * area.height();
        current.append(QPointF{x, y});
    }
    if (!current.isEmpty()) {
        segments.push_back(std::move(current));
    }
    return segments;
}

SparklineWidget::SparklineWidget(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SparklineWidget::setSamples(std::span<const std::optional<float>> samples)
{
    m_samples.assign(samples.begin(), samples.end());
    update();
}

void SparklineWidget::setCapacity(std::size_t capacity)
{
    assert(capacity > 0 && "sparkline capacity must be greater than zero");
    m_capacity = capacity;
    update();
}

void SparklineWidget::setFixedRange(YRange range)
{
    assert(range.min < range.max && "sparkline range must not be empty");
    m_fixedRange = range;
    update();
}

void SparklineWidget::setAutoRange()
{
    m_fixedRange.reset();
    update();
}

std::span<const std::optional<float>> SparklineWidget::samples() const
{
    return m_samples;
}

std::size_t SparklineWidget::capacity() const
{
    return m_capacity;
}

void SparklineWidget::setLineColor(const QColor &color)
{
    m_lineColor = color;
    update();
}

bool SparklineWidget::isAutoRange() const
{
    return !m_fixedRange.has_value();
}

YRange SparklineWidget::effectiveRange() const
{
    return m_fixedRange ? *m_fixedRange : autoRange(m_samples);
}

QSize SparklineWidget::sizeHint() const
{
    return {160, 36};
}

QSize SparklineWidget::minimumSizeHint() const
{
    return {40, 16};
}

void SparklineWidget::paintEvent(QPaintEvent * /*event*/)
{
    // Inset by half the pen width so the line is not clipped at the edges.
    const QRectF area = QRectF(rect()).adjusted(kLineWidth, kLineWidth, -kLineWidth, -kLineWidth);
    const auto segments = sparklineSegments(m_samples, m_capacity, area, effectiveRange());
    if (segments.empty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor lineColor = m_lineColor;
    QColor fillTop = lineColor;
    fillTop.setAlpha(kFillTopAlpha);
    QColor fillBottom = lineColor;
    fillBottom.setAlpha(0);
    QLinearGradient gradient(area.topLeft(), area.bottomLeft());
    gradient.setColorAt(0.0, fillTop);
    gradient.setColorAt(1.0, fillBottom);

    for (const auto &segment : segments) {
        if (segment.size() < 2) {
            // An isolated sample between gaps: draw it as a dot.
            painter.setPen(QPen(lineColor, kLineWidth * 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawPoint(segment.front());
            continue;
        }

        QPainterPath fill;
        fill.moveTo(segment.front().x(), area.bottom());
        for (const auto &point : segment) {
            fill.lineTo(point);
        }
        fill.lineTo(segment.back().x(), area.bottom());
        fill.closeSubpath();
        painter.fillPath(fill, gradient);

        QPainterPath line;
        line.addPolygon(segment);
        painter.setPen(QPen(lineColor, kLineWidth));
        painter.drawPath(line);
    }
}

} // namespace sysmon::ui::charts
