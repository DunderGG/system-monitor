#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/ring_buffer.h"
#include "domain/system_snapshot.h"
#include "ui/charts/sparkline_widget.h"

class QVBoxLayout;

namespace sysmon::ui
{

/** History of one chart series, oldest first. A missing sample is std::nullopt and shows as a gap. */
using SampleHistory = domain::RingBuffer<std::optional<float>>;

/** Returns true if history holds at least one sample that is not a gap. */
[[nodiscard]] bool hasAnyValue(const SampleHistory& history);

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
    /** Samples of history kept per series: 30 minutes at the default 1 Hz sampling rate. */
    static constexpr std::size_t kHistoryCapacity = 1800;

    /** Samples the charts show until setVisibleWindow() is called: one minute at 1 Hz. */
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

protected:
    /** Appends the snapshot's samples to the subclass's history. */
    virtual void recordSamples(const domain::SystemSnapshot& snapshot) = 0;

    /** Layout below the heading that subclasses add their widgets to. */
    [[nodiscard]] QVBoxLayout* contentLayout() const;

    /**
     * Creates a full-size history chart with a scrolling grid, axis labels, and
     * annotations. A fixedRange of std::nullopt scales the Y axis to the data.
     */
    [[nodiscard]] static charts::SparklineWidget* createHistoryChart(std::optional<charts::YRange> fixedRange,
                                                                     charts::SparklineWidget::ValueFormatter formatter,
                                                                     QWidget* parent);

    /**
     * Shows the visible window of history as the given series of chart, sizes
     * the chart to the window, and scrolls its grid to the newest sample. Only
     * the window is passed to the chart, so an auto-scaled Y axis follows the
     * samples on screen.
     */
    void showHistory(charts::SparklineWidget* chart, std::size_t series, const SampleHistory& history) const;

private:
    QString m_title;
    QVBoxLayout* m_contentLayout{nullptr};
    std::uint64_t m_recordedCount{0};
    std::size_t m_visibleWindow{kDefaultVisibleWindow};
};

} // namespace sysmon::ui
