#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <QColor>
#include <QPolygonF>
#include <QRectF>
#include <QWidget>

namespace sysmon::ui::charts
{

/** Vertical value range mapped to the chart's height. Invariant: min < max. */
struct YRange
{
    float min{0.0f};
    float max{100.0f};
};

/**
 * Returns a Y range from 0 to just above the largest sample, for data with no
 * natural upper bound (e.g. network throughput). Falls back to {0, 1} when
 * there are no samples or all are zero, so the range is never empty.
 */
[[nodiscard]] YRange autoRange(std::span<const std::optional<float>> samples);

/**
 * Maps samples (oldest first) to polyline segments within area. The chart has
 * capacity horizontal slots and the newest sample sits at the right edge, so a
 * partially filled history grows in from the right. A std::nullopt sample ends
 * the current segment, leaving a visible gap instead of a fake zero. Values
 * outside range are clamped. Pure function for unit testing.
 */
[[nodiscard]] std::vector<QPolygonF> sparklineSegments(std::span<const std::optional<float>> samples,
                                                       std::size_t capacity,
                                                       const QRectF &area,
                                                       YRange range);

/**
 * Minimal single-series sparkline: a polyline with a gradient fill beneath it.
 * The caller owns the history (e.g. a domain::RingBuffer) and passes a view of
 * it with setSamples() on each update; the widget keeps a copy for painting.
 *
 * Phase 3 extends this widget with a grid, multiple series, and annotations.
 * Not thread-safe; use only on the UI thread.
 */
class SparklineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SparklineWidget(QWidget *parent = nullptr);
    ~SparklineWidget() override = default;

    /** Replaces the displayed samples (oldest first) and repaints. */
    void setSamples(std::span<const std::optional<float>> samples);

    /** Sets the number of horizontal slots, normally the history capacity. Must be > 0. */
    void setCapacity(std::size_t capacity);

    /** Uses a fixed Y range, e.g. {0, 100} for percentages. */
    void setFixedRange(YRange range);

    /** Scales the Y range to the current samples (see autoRange). */
    void setAutoRange();

    /** Sets the line colour; the fill uses the same colour fading to transparent. */
    void setLineColor(const QColor &color);

    [[nodiscard]] std::span<const std::optional<float>> samples() const;
    [[nodiscard]] std::size_t capacity() const;
    [[nodiscard]] bool isAutoRange() const;

    /** Returns the Y range used for painting: the fixed range, or autoRange of the samples. */
    [[nodiscard]] YRange effectiveRange() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<std::optional<float>> m_samples;
    std::size_t m_capacity{60};
    std::optional<YRange> m_fixedRange{YRange{}};
    // Fixed neutral blue rather than the system accent colour, which may be red and
    // would read as the "Critical" status colour.
    QColor m_lineColor{0x3c, 0x8f, 0xdc};
};

} // namespace sysmon::ui::charts
