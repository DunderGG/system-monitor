#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/sample_history.h"

class QVBoxLayout;

namespace verdandi::ui
{

/**
 * One detail page of the Performance tab (CPU, memory, disk, or network): a
 * heading above full-size history charts.
 *
 * Recording is separate from display, so a page keeps a complete history
 * while it is hidden. recordSnapshot() is called for every snapshot and only
 * appends to the page's own history. refresh() copies that history and the
 * latest values into the page's widgets, and is called only while the page
 * is shown.
 *
 * Not thread-safe; use only on the UI thread.
 */
class PerformancePage : public QWidget
{
    Q_OBJECT

public:
    /** Samples of history kept per series: 30 minutes at the default refresh interval of 1 s (DC-61). */
    static constexpr std::size_t kHistoryCapacity = 1800;

    /** Samples the charts show until setVisibleWindow() is called: one minute at the default interval. */
    static constexpr std::size_t kDefaultVisibleWindow = 60;

    explicit PerformancePage(const QString& title, QWidget* parent = nullptr);
    ~PerformancePage() override = default;

    /** Appends the snapshot's samples to this page's history. Does not touch the widgets. */
    void recordSnapshot(const domain::SystemSnapshot& snapshot);

    /** Copies the recorded history and latest values into the page's widgets. */
    virtual void refresh() = 0;

    /** Short summary of the latest values for the sidebar, e.g. "12.5%"; "--" before the first snapshot. */
    [[nodiscard]] virtual QString summary() const = 0;

    [[nodiscard]] QString title() const;

    /** Number of snapshots recorded so far, the running index that scrolls the chart grids. */
    [[nodiscard]] std::uint64_t recordedCount() const;

    /**
     * Sets how many of the newest samples the charts show, from 1 to
     * kHistoryCapacity. The full history is kept either way. Takes effect on
     * the next refresh().
     */
    void setVisibleWindow(std::size_t samples);
    [[nodiscard]] std::size_t visibleWindow() const;

    /**
     * Sets the time between samples, the refresh interval, from which the
     * charts' X-axis labels tell the time their samples cover. Must be > 0.
     * Takes effect on the next refresh().
     */
    void setSampleInterval(std::chrono::milliseconds interval);
    [[nodiscard]] std::chrono::milliseconds sampleInterval() const;

    /**
     * Sets the units of the page's sizes and rates: its readouts, summary and
     * charts. Binary bytes and bytes per second until set. Takes effect on
     * the next refresh() and summary().
     */
    void setDisplayUnits(domain::DisplayUnits units);
    [[nodiscard]] domain::DisplayUnits displayUnits() const;

protected:
    /** Appends the snapshot's samples to the subclass's history. */
    virtual void recordSamples(const domain::SystemSnapshot& snapshot) = 0;

    /** Layout below the heading that subclasses add their widgets to. */
    [[nodiscard]] QVBoxLayout* contentLayout() const;

    /**
     * Creates a full-size history chart with a scrolling grid, axis labels, and
     * annotations. A fixedRange of std::nullopt scales the Y axis to the data.
     * axisFormatter formats the axis labels, and annotationFormatter the
     * current, min, and max values, which should match the page's readouts.
     */
    [[nodiscard]] static charts::SparklineWidget*
    createHistoryChart(std::optional<charts::YRange> fixedRange, charts::SparklineWidget::ValueFormatter axisFormatter,
                       charts::SparklineWidget::ValueFormatter annotationFormatter, QWidget* parent);

    /**
     * Shows the visible window of history as the given series of chart, sizes
     * the chart to the window, gives it the sample interval, and scrolls its
     * grid to the newest sample. Only
     * the window is passed to the chart, so an auto-scaled Y axis follows the
     * samples on screen.
     */
    void showHistory(charts::SparklineWidget* chart, std::size_t series, const SampleHistory& history) const;

private:
    QString m_title;
    QVBoxLayout* m_contentLayout{nullptr};
    std::uint64_t m_recordedCount{0};
    std::size_t m_visibleWindow{kDefaultVisibleWindow};
    std::chrono::milliseconds m_sampleInterval{domain::kDefaultRefreshInterval};
    domain::DisplayUnits m_displayUnits{};
};

} // namespace verdandi::ui
