#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <QColor>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace sysmon::ui::charts
{

/** Vertical value range mapped to the chart's height. Invariant: min < max. */
struct YRange
{
    float min{0.0f};
    float max{100.0f};
};

/** A sample value and its index within the visible window (0 = oldest visible sample). */
struct SampleExtremum
{
    std::size_t index{0};
    float value{0.0f};
};

/** Current value and extremes of the visible part of one series. Gaps are ignored. */
struct SeriesSummary
{
    /** The newest sample; std::nullopt when there are no samples or the newest is a gap. */
    std::optional<float> current;
    std::optional<SampleExtremum> minimum;
    std::optional<SampleExtremum> maximum;
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
                                                       std::size_t capacity, const QRectF& area, YRange range);

/**
 * Returns the X positions of the vertical grid lines within area. Lines sit on
 * every (capacity / 10)th sample of the running sample sequence, where
 * newestSampleIndex is the index of the sample at the right edge. Advancing the
 * index by one moves every line one slot to the left, so the grid scrolls with
 * the data. Returns no lines when capacity < 2. Pure function for unit testing.
 */
[[nodiscard]] std::vector<double> verticalGridLines(std::size_t capacity, std::uint64_t newestSampleIndex,
                                                    const QRectF& area);

/**
 * Summarizes the visible part (the newest capacity samples) of a series. On
 * ties, the newest occurrence is reported as the minimum or maximum. Pure
 * function for unit testing.
 */
[[nodiscard]] SeriesSummary summarizeSeries(std::span<const std::optional<float>> samples, std::size_t capacity);

/**
 * Formats the time span a chart covers for its X-axis label in the largest
 * unit that divides it exactly: "90 s", "1 min", "30 min", "2 h".
 */
[[nodiscard]] QString formatHistoryWindow(std::chrono::seconds window);

/**
 * Returns the default colour of series index on a light or dark surface. The
 * first five series take fixed categorical hues (blue, orange, aqua, violet,
 * magenta) in an order checked for colour-vision-deficiency separation. Red,
 * amber, and green are left out because they are the health status colours.
 * Further series share a muted grey rather than generated or repeated hues;
 * give them explicit colours or split them across charts.
 */
[[nodiscard]] QColor defaultSeriesColor(std::size_t index, bool darkSurface);

/**
 * Task Manager-style rolling chart: one or more series drawn as polylines,
 * optionally over a scrolling grid, with optional axis labels and value
 * annotations. The caller owns the history (e.g. a domain::RingBuffer per
 * series) and passes a view of it on each update; the widget keeps a copy for
 * painting. All series share the time axis and Y range, and each has its
 * newest sample at the right edge.
 *
 * Series 0 is the primary series: setSamples() and samples() are shortcuts for
 * it, and the min/max annotations describe it. The grid, axis labels, and
 * annotations are off by default so the widget also works as a compact
 * dashboard sparkline.
 *
 * Not thread-safe; use only on the UI thread.
 */
class SparklineWidget : public QWidget
{
    Q_OBJECT

public:
    /** Formats a value for axis labels and annotations, e.g. "42%" or "1.5 MB/s". */
    using ValueFormatter = std::function<QString(float)>;

    explicit SparklineWidget(QWidget* parent = nullptr);
    ~SparklineWidget() override = default;

    /** Replaces the samples (oldest first) of the primary series and repaints. */
    void setSamples(std::span<const std::optional<float>> samples);
    [[nodiscard]] std::span<const std::optional<float>> samples() const;

    /**
     * Sets the number of series. Must be > 0. Existing series keep their samples
     * and style; new series start empty with the default colour, no label, and no
     * fill.
     */
    void setSeriesCount(std::size_t count);
    [[nodiscard]] std::size_t seriesCount() const;

    /** Replaces the samples (oldest first) of series and repaints. */
    void setSeriesSamples(std::size_t series, std::span<const std::optional<float>> samples);
    [[nodiscard]] std::span<const std::optional<float>> seriesSamples(std::size_t series) const;

    /** Overrides the colour of series; the fill uses the same colour fading to transparent. */
    void setSeriesColor(std::size_t series, const QColor& color);

    /** Returns the colour series is painted with: its override, or its default for the current palette. */
    [[nodiscard]] QColor seriesColor(std::size_t series) const;

    /** Names series in the legend. Unlabelled series are left out of the legend. */
    void setSeriesLabel(std::size_t series, const QString& label);
    [[nodiscard]] QString seriesLabel(std::size_t series) const;

    /** Sets whether series has a gradient fill below its line. Only the primary series is filled by default. */
    void setSeriesFilled(std::size_t series, bool filled);
    [[nodiscard]] bool isSeriesFilled(std::size_t series) const;

    /** Sets the number of horizontal slots, normally the history capacity. Must be > 0. */
    void setCapacity(std::size_t capacity);
    [[nodiscard]] std::size_t capacity() const;

    /**
     * Sets the running index of the newest sample, e.g. the number of samples
     * recorded so far. The grid is offset by it so it scrolls with the data.
     */
    void setSampleIndex(std::uint64_t index);
    [[nodiscard]] std::uint64_t sampleIndex() const;

    /** Sets the time between samples, used for the X-axis label. Must be > 0. Defaults to one second. */
    void setSampleInterval(std::chrono::milliseconds interval);
    [[nodiscard]] std::chrono::milliseconds sampleInterval() const;

    /** Uses a fixed Y range, e.g. {0, 100} for percentages. */
    void setFixedRange(YRange range);

    /** Scales the Y range to the samples of all series (see autoRange). */
    void setAutoRange();
    [[nodiscard]] bool isAutoRange() const;

    /** Returns the Y range used for painting: the fixed range, or the auto range of all series. */
    [[nodiscard]] YRange effectiveRange() const;

    /** Sets how values are formatted in axis labels and annotations. Must not be empty. */
    void setValueFormatter(ValueFormatter formatter);
    [[nodiscard]] QString formatValue(float value) const;

    /** Shows a scrolling grid and a frame behind the series. */
    void setGridVisible(bool visible);
    [[nodiscard]] bool isGridVisible() const;

    /** Shows Y-axis labels (range min and max) on the left and X-axis labels (history window, "now") below. */
    void setAxisLabelsVisible(bool visible);
    [[nodiscard]] bool areAxisLabelsVisible() const;

    /**
     * Shows a line above the chart with the current value (or, for several
     * series, a legend of labelled series with their current values) and the
     * primary series' min and max, which are also marked on its line.
     */
    void setAnnotationsVisible(bool visible);
    [[nodiscard]] bool areAnnotationsVisible() const;

    /** Returns the rectangle the series are drawn in, after room is reserved for labels. */
    [[nodiscard]] QRectF plotArea() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Series
    {
        std::vector<std::optional<float>> samples;
        std::optional<QColor> color;
        QString label;
        bool filled{false};
    };

    [[nodiscard]] bool hasDarkSurface() const;
    void paintGrid(QPainter& painter, const QRectF& area) const;
    void paintSeries(QPainter& painter, const Series& series, const QColor& color, const QRectF& area,
                     YRange range) const;
    void paintAxisLabels(QPainter& painter, const QRectF& area, YRange range) const;
    void paintAnnotations(QPainter& painter, const QRectF& area, YRange range) const;

    std::vector<Series> m_series;
    std::size_t m_capacity{60};
    std::uint64_t m_sampleIndex{0};
    std::chrono::milliseconds m_sampleInterval{std::chrono::seconds{1}};
    std::optional<YRange> m_fixedRange{YRange{}};
    ValueFormatter m_formatter;
    bool m_gridVisible{false};
    bool m_axisLabelsVisible{false};
    bool m_annotationsVisible{false};
};

} // namespace sysmon::ui::charts
