#include "ui/charts/sparkline_widget.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <utility>

#include <QColor>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

namespace sysmon::ui::charts
{

namespace
{

// Headroom above the largest sample so the line does not touch the top edge.
constexpr float kAutoRangeHeadroom = 1.1f;
constexpr int kFillTopAlpha = 90;
constexpr qreal kLineWidth = 1.5;

// Grid: vertical lines every capacity / kVerticalGridDivisions samples, fixed
// horizontal lines at quarters of the range. Hairlines in the text colour at
// low opacity so the grid stays recessive on light and dark backgrounds.
constexpr std::size_t kVerticalGridDivisions = 10;
constexpr int kHorizontalGridDivisions = 4;
constexpr qreal kGridAlpha = 0.15;

// Axis labels and min/max text: the text colour, dimmed so the current values stand out.
constexpr qreal kMutedTextAlpha = 0.6;

// Min/max markers: a filled dot with a ring in the background colour so it
// stays distinct where it sits on the line.
constexpr qreal kMarkerRadius = 4.0;
constexpr qreal kMarkerRingWidth = 2.0;

// Space between a label and the plot, and between legend entries.
constexpr qreal kLabelGap = 4.0;
constexpr qreal kLegendEntryGap = 12.0;
constexpr qreal kLegendKeyWidth = 10.0;

// Categorical series colours, light and dark steps of the same hues. The order
// was checked for adjacent-pair separation under colour-vision deficiency on
// both surfaces. Red, amber, and green are left out: they are the health
// status colours (see resource_card.cpp).
constexpr std::array<QRgb, 5> kLightSeriesColors{0xff2a78d6, 0xffeb6834, 0xff1baf7a, 0xff4a3aa7, 0xffe87ba4};
constexpr std::array<QRgb, 5> kDarkSeriesColors{0xff3987e5, 0xffd95926, 0xff199e70, 0xff9085e9, 0xffd55181};
constexpr QRgb kOverflowSeriesColor = 0xff898781;

const QString kNotAvailable = "N/A";

YRange rangeAbove(float largest)
{
    return YRange{.min = 0.0f, .max = largest > 0.0f ? largest * kAutoRangeHeadroom : 1.0f};
}

float largestSample(std::span<const std::optional<float>> samples)
{
    float largest = 0.0f;
    for (const auto& sample : samples) {
        if (sample) {
            largest = std::max(largest, *sample);
        }
    }
    return largest;
}

std::span<const std::optional<float>> visibleSamples(std::span<const std::optional<float>> samples,
                                                     std::size_t capacity)
{
    return samples.size() > capacity ? samples.last(capacity) : samples;
}

// X position of a horizontal slot; with a single slot, the right edge.
double slotX(std::size_t slot, std::size_t capacity, const QRectF& area)
{
    if (capacity < 2) {
        return area.right();
    }
    return area.left() + static_cast<double>(slot) * area.width() / static_cast<double>(capacity - 1);
}

// Y position of a value, clamped to range.
double valueY(float value, const QRectF& area, YRange range)
{
    const double clamped =
        std::clamp(static_cast<double>(value), static_cast<double>(range.min), static_cast<double>(range.max));
    return area.bottom() - (clamped - range.min) / static_cast<double>(range.max - range.min) * area.height();
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

// Whole numbers without decimals; one decimal below 10 so small auto ranges stay readable.
QString defaultFormat(float value)
{
    const bool showDecimal = std::abs(value) < 10.0f && value != std::trunc(value);
    return QString::number(static_cast<double>(value), 'f', showDecimal ? 1 : 0);
}

} // namespace

YRange autoRange(std::span<const std::optional<float>> samples)
{
    return rangeAbove(largestSample(samples));
}

std::vector<QPolygonF> sparklineSegments(std::span<const std::optional<float>> samples, std::size_t capacity,
                                         const QRectF& area, YRange range)
{
    assert(capacity > 0 && "sparkline capacity must be greater than zero");
    assert(range.min < range.max && "sparkline range must not be empty");

    std::vector<QPolygonF> segments;
    if (samples.empty() || area.isEmpty()) {
        return segments;
    }

    // Only the newest `capacity` samples fit on the chart.
    const auto visible = visibleSamples(samples, capacity);
    const std::size_t firstSlot = capacity - visible.size();

    QPolygonF current;
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const auto& sample = visible[i];
        if (!sample) {
            if (!current.isEmpty()) {
                segments.push_back(std::move(current));
                current = QPolygonF{};
            }
            continue;
        }
        current.append(QPointF{slotX(firstSlot + i, capacity, area), valueY(*sample, area, range)});
    }
    if (!current.isEmpty()) {
        segments.push_back(std::move(current));
    }
    return segments;
}

std::vector<double> verticalGridLines(std::size_t capacity, std::uint64_t newestSampleIndex, const QRectF& area)
{
    std::vector<double> lines;
    if (capacity < 2 || area.isEmpty()) {
        return lines;
    }

    const std::size_t spacing = std::max<std::size_t>(1, capacity / kVerticalGridDivisions);
    // Slot 0 holds running index newestSampleIndex - (capacity - 1). Take that
    // modulo spacing without underflowing while the history is still short.
    const std::uint64_t spacing64 = spacing;
    const auto phase =
        static_cast<std::size_t>((newestSampleIndex % spacing64 + spacing64 - (capacity - 1) % spacing64) % spacing64);
    for (std::size_t slot = (spacing - phase) % spacing; slot < capacity; slot += spacing) {
        lines.push_back(slotX(slot, capacity, area));
    }
    return lines;
}

SeriesSummary summarizeSeries(std::span<const std::optional<float>> samples, std::size_t capacity)
{
    assert(capacity > 0 && "sparkline capacity must be greater than zero");

    SeriesSummary summary;
    const auto visible = visibleSamples(samples, capacity);
    if (visible.empty()) {
        return summary;
    }

    summary.current = visible.back();
    for (std::size_t i = 0; i < visible.size(); ++i) {
        if (!visible[i]) {
            continue;
        }
        const float value = *visible[i];
        if (!summary.minimum || value <= summary.minimum->value) {
            summary.minimum = SampleExtremum{.index = i, .value = value};
        }
        if (!summary.maximum || value >= summary.maximum->value) {
            summary.maximum = SampleExtremum{.index = i, .value = value};
        }
    }
    return summary;
}

QString formatHistoryWindow(std::chrono::seconds window)
{
    const auto seconds = window.count();
    if (seconds > 0 && seconds % 3600 == 0) {
        return QString("%1 h").arg(seconds / 3600);
    }
    if (seconds > 0 && seconds % 60 == 0) {
        return QString("%1 min").arg(seconds / 60);
    }
    return QString("%1 s").arg(seconds);
}

QColor defaultSeriesColor(std::size_t index, bool darkSurface)
{
    const auto& colors = darkSurface ? kDarkSeriesColors : kLightSeriesColors;
    return QColor(index < colors.size() ? colors[index] : kOverflowSeriesColor);
}

SparklineWidget::SparklineWidget(QWidget* parent) : QWidget(parent), m_series(1), m_formatter(defaultFormat)
{
    m_series.front().filled = true;
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SparklineWidget::setSamples(std::span<const std::optional<float>> samples)
{
    setSeriesSamples(0, samples);
}

std::span<const std::optional<float>> SparklineWidget::samples() const
{
    return seriesSamples(0);
}

void SparklineWidget::setSeriesCount(std::size_t count)
{
    assert(count > 0 && "sparkline must have at least one series");
    m_series.resize(count);
    update();
}

std::size_t SparklineWidget::seriesCount() const
{
    return m_series.size();
}

void SparklineWidget::setSeriesSamples(std::size_t series, std::span<const std::optional<float>> samples)
{
    assert(series < m_series.size() && "sparkline series index out of range");
    m_series[series].samples.assign(samples.begin(), samples.end());
    update();
}

std::span<const std::optional<float>> SparklineWidget::seriesSamples(std::size_t series) const
{
    assert(series < m_series.size() && "sparkline series index out of range");
    return m_series[series].samples;
}

void SparklineWidget::setSeriesColor(std::size_t series, const QColor& color)
{
    assert(series < m_series.size() && "sparkline series index out of range");
    m_series[series].color = color;
    update();
}

QColor SparklineWidget::seriesColor(std::size_t series) const
{
    assert(series < m_series.size() && "sparkline series index out of range");
    const auto& color = m_series[series].color;
    return color ? *color : defaultSeriesColor(series, hasDarkSurface());
}

void SparklineWidget::setSeriesLabel(std::size_t series, const QString& label)
{
    assert(series < m_series.size() && "sparkline series index out of range");
    m_series[series].label = label;
    update();
}

QString SparklineWidget::seriesLabel(std::size_t series) const
{
    assert(series < m_series.size() && "sparkline series index out of range");
    return m_series[series].label;
}

void SparklineWidget::setSeriesFilled(std::size_t series, bool filled)
{
    assert(series < m_series.size() && "sparkline series index out of range");
    m_series[series].filled = filled;
    update();
}

bool SparklineWidget::isSeriesFilled(std::size_t series) const
{
    assert(series < m_series.size() && "sparkline series index out of range");
    return m_series[series].filled;
}

void SparklineWidget::setCapacity(std::size_t capacity)
{
    assert(capacity > 0 && "sparkline capacity must be greater than zero");
    m_capacity = capacity;
    update();
}

std::size_t SparklineWidget::capacity() const
{
    return m_capacity;
}

void SparklineWidget::setSampleIndex(std::uint64_t index)
{
    m_sampleIndex = index;
    update();
}

std::uint64_t SparklineWidget::sampleIndex() const
{
    return m_sampleIndex;
}

void SparklineWidget::setSampleInterval(std::chrono::milliseconds interval)
{
    assert(interval.count() > 0 && "sparkline sample interval must be positive");
    m_sampleInterval = interval;
    update();
}

std::chrono::milliseconds SparklineWidget::sampleInterval() const
{
    return m_sampleInterval;
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

bool SparklineWidget::isAutoRange() const
{
    return !m_fixedRange.has_value();
}

YRange SparklineWidget::effectiveRange() const
{
    if (m_fixedRange) {
        return *m_fixedRange;
    }
    float largest = 0.0f;
    for (const auto& series : m_series) {
        largest = std::max(largest, largestSample(series.samples));
    }
    return rangeAbove(largest);
}

void SparklineWidget::setValueFormatter(ValueFormatter formatter)
{
    assert(formatter && "sparkline value formatter must not be empty");
    m_formatter = std::move(formatter);
    update();
}

QString SparklineWidget::formatValue(float value) const
{
    return m_formatter(value);
}

void SparklineWidget::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

bool SparklineWidget::isGridVisible() const
{
    return m_gridVisible;
}

void SparklineWidget::setAxisLabelsVisible(bool visible)
{
    m_axisLabelsVisible = visible;
    update();
}

bool SparklineWidget::areAxisLabelsVisible() const
{
    return m_axisLabelsVisible;
}

void SparklineWidget::setAnnotationsVisible(bool visible)
{
    m_annotationsVisible = visible;
    update();
}

bool SparklineWidget::areAnnotationsVisible() const
{
    return m_annotationsVisible;
}

QRectF SparklineWidget::plotArea() const
{
    QRectF area = QRectF(rect());
    const QFontMetricsF metrics(font());
    const qreal textBand = metrics.height() + kLabelGap;
    if (m_annotationsVisible) {
        area.setTop(area.top() + textBand);
    }
    if (m_axisLabelsVisible) {
        const YRange range = effectiveRange();
        const qreal labelWidth = std::max(metrics.horizontalAdvance(formatValue(range.max)),
                                          metrics.horizontalAdvance(formatValue(range.min)));
        area.setLeft(area.left() + labelWidth + kLabelGap);
        area.setBottom(area.bottom() - textBand);
    }
    // Inset so the line, and the min/max markers when shown, are not clipped at the edges.
    const qreal inset = m_annotationsVisible ? kMarkerRadius + kMarkerRingWidth : kLineWidth;
    return area.adjusted(inset, inset, -inset, -inset);
}

QSize SparklineWidget::sizeHint() const
{
    return {160, 36};
}

QSize SparklineWidget::minimumSizeHint() const
{
    return {40, 16};
}

bool SparklineWidget::hasDarkSurface() const
{
    return palette().color(backgroundRole()).lightnessF() < 0.5;
}

void SparklineWidget::paintEvent(QPaintEvent* /*event*/)
{
    const QRectF area = plotArea();
    const YRange range = effectiveRange();

    QPainter painter(this);
    if (m_gridVisible) {
        paintGrid(painter, area);
    }

    painter.setRenderHint(QPainter::Antialiasing);
    // Paint in reverse so the primary series ends up on top.
    for (std::size_t index = m_series.size(); index-- > 0;) {
        paintSeries(painter, m_series[index], seriesColor(index), area, range);
    }

    if (m_axisLabelsVisible) {
        paintAxisLabels(painter, area, range);
    }
    if (m_annotationsVisible) {
        paintAnnotations(painter, area, range);
    }
}

void SparklineWidget::paintGrid(QPainter& painter, const QRectF& area) const
{
    if (area.isEmpty()) {
        return;
    }

    // Hairlines without antialiasing stay crisp on whole pixels.
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(withAlpha(palette().color(foregroundRole()), kGridAlpha), 1));

    for (int division = 1; division < kHorizontalGridDivisions; ++division) {
        const qreal y = area.top() + area.height() * division / kHorizontalGridDivisions;
        painter.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
    }
    for (const double x : verticalGridLines(m_capacity, m_sampleIndex, area)) {
        painter.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
    }
    painter.drawRect(area);
    painter.restore();
}

void SparklineWidget::paintSeries(QPainter& painter, const Series& series, const QColor& color, const QRectF& area,
                                  YRange range) const
{
    const auto segments = sparklineSegments(series.samples, m_capacity, area, range);
    if (segments.empty()) {
        return;
    }

    QLinearGradient gradient(area.topLeft(), area.bottomLeft());
    QColor fillTop = color;
    fillTop.setAlpha(kFillTopAlpha);
    QColor fillBottom = color;
    fillBottom.setAlpha(0);
    gradient.setColorAt(0.0, fillTop);
    gradient.setColorAt(1.0, fillBottom);

    for (const auto& segment : segments) {
        if (segment.size() < 2) {
            // An isolated sample between gaps: draw it as a dot.
            painter.setPen(QPen(color, kLineWidth * 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawPoint(segment.front());
            continue;
        }

        if (series.filled) {
            QPainterPath fill;
            fill.moveTo(segment.front().x(), area.bottom());
            for (const auto& point : segment) {
                fill.lineTo(point);
            }
            fill.lineTo(segment.back().x(), area.bottom());
            fill.closeSubpath();
            painter.fillPath(fill, gradient);
        }

        QPainterPath line;
        line.addPolygon(segment);
        painter.setPen(QPen(color, kLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line);
    }
}

void SparklineWidget::paintAxisLabels(QPainter& painter, const QRectF& area, YRange range) const
{
    const QFontMetricsF metrics(font());
    const qreal lineHeight = metrics.height();
    painter.setPen(withAlpha(palette().color(foregroundRole()), kMutedTextAlpha));

    // Y axis: range max level with the top of the plot, min level with the bottom.
    const qreal gutterWidth = std::max(0.0, area.left() - kLabelGap);
    painter.drawText(QRectF{0.0, area.top(), gutterWidth, lineHeight}, Qt::AlignRight | Qt::AlignTop,
                     formatValue(range.max));
    painter.drawText(QRectF{0.0, area.bottom() - lineHeight, gutterWidth, lineHeight}, Qt::AlignRight | Qt::AlignBottom,
                     formatValue(range.min));

    // X axis: the time the chart covers on the left, the newest sample on the right.
    const auto window = std::chrono::duration_cast<std::chrono::seconds>(
        m_sampleInterval * static_cast<std::chrono::milliseconds::rep>(m_capacity));
    const QRectF xLabels{area.left(), height() - lineHeight, area.width(), lineHeight};
    painter.drawText(xLabels, Qt::AlignLeft | Qt::AlignBottom, formatHistoryWindow(window));
    painter.drawText(xLabels, Qt::AlignRight | Qt::AlignBottom, "now");
}

void SparklineWidget::paintAnnotations(QPainter& painter, const QRectF& area, YRange range) const
{
    const QFontMetricsF metrics(font());
    const QRectF band{area.left(), 0.0, area.width(), metrics.height()};
    const auto primary = summarizeSeries(m_series.front().samples, m_capacity);

    // Right: min and max of the primary series.
    qreal extremesWidth = 0.0;
    if (primary.minimum && primary.maximum) {
        const QString extremes =
            QString("Min %1  Max %2").arg(formatValue(primary.minimum->value), formatValue(primary.maximum->value));
        extremesWidth = metrics.horizontalAdvance(extremes) + kLegendEntryGap;
        painter.setPen(withAlpha(palette().color(foregroundRole()), kMutedTextAlpha));
        painter.drawText(band, Qt::AlignRight | Qt::AlignVCenter, extremes);
    }

    // Left: current values, clipped so they never overlap the extremes.
    painter.save();
    painter.setClipRect(band.adjusted(0.0, 0.0, -extremesWidth, 0.0));
    const QColor ink = palette().color(foregroundRole());
    const auto currentText = [this](const std::optional<float>& value) {
        return value ? formatValue(*value) : kNotAvailable;
    };
    const bool hasLegend = m_series.size() > 1 &&
                           std::ranges::any_of(m_series, [](const Series& series) { return !series.label.isEmpty(); });
    if (!hasLegend) {
        painter.setPen(ink);
        painter.drawText(band, Qt::AlignLeft | Qt::AlignVCenter, currentText(primary.current));
    } else {
        // A short line key in the series colour carries identity; the text stays in the text colour.
        qreal x = band.left();
        for (std::size_t index = 0; index < m_series.size(); ++index) {
            const auto& series = m_series[index];
            if (series.label.isEmpty()) {
                continue;
            }
            painter.setPen(QPen(seriesColor(index), 2 * kLineWidth, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF{x, band.center().y()}, QPointF{x + kLegendKeyWidth, band.center().y()});
            x += kLegendKeyWidth + kLabelGap;

            const QString text =
                QString("%1 %2").arg(series.label, currentText(summarizeSeries(series.samples, m_capacity).current));
            painter.setPen(ink);
            painter.drawText(QRectF{x, band.top(), band.right() - x, band.height()}, Qt::AlignLeft | Qt::AlignVCenter,
                             text);
            x += metrics.horizontalAdvance(text) + kLegendEntryGap;
        }
    }
    painter.restore();

    // Mark the min and max on the primary series' line.
    if (area.isEmpty()) {
        return;
    }
    const std::size_t firstSlot = m_capacity - visibleSamples(m_series.front().samples, m_capacity).size();
    painter.setPen(QPen(palette().color(backgroundRole()), kMarkerRingWidth));
    painter.setBrush(seriesColor(0));
    for (const auto& extremum : {primary.minimum, primary.maximum}) {
        if (!extremum) {
            continue;
        }
        const QPointF point{slotX(firstSlot + extremum->index, m_capacity, area), valueY(extremum->value, area, range)};
        painter.drawEllipse(point, kMarkerRadius, kMarkerRadius);
    }
}

} // namespace sysmon::ui::charts
