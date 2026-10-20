#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QFrame>
#include <QString>

#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "domain/settings.h"
#include "ui/adapter_throughput_history.h"

class QLabel;

namespace sysmon::ui
{

class ReadoutGrid;
class TonedLabel;

namespace charts
{
class SparklineWidget;
}

/**
 * Network tab card for one network adapter: its display name (the alias)
 * with its operational status, the device description below it when that
 * differs from the name, readouts for the connected network, type, link
 * speed, addresses, and DNS servers (see adapterDetailReadouts()), and a chart
 * of its receive and send throughput on an auto-scaled axis in the display units.
 *
 * While the adapter is not Up, the chart is collapsed to a line saying that
 * it is charted while the adapter is Up. The view keeps recording the
 * adapter's history, so the chart is complete when it comes back.
 *
 * Not thread-safe; use only on the UI thread.
 */
class AdapterCard : public QFrame
{
    Q_OBJECT

public:
    /** Creates a card whose chart shows the newest chartCapacity samples. */
    explicit AdapterCard(std::size_t chartCapacity, QWidget* parent = nullptr);
    ~AdapterCard() override = default;

    /** Shows the adapter's latest sample, and the connected networks it belongs to (std::nullopt: unknown). */
    void setAdapter(const domain::NetworkSample& adapter,
                    const std::optional<std::vector<domain::NetworkProfile>>& networkProfiles);

    /** Shows the status and readouts as "N/A" because the last read failed. Keeps the name and description. */
    void setUnavailable();

    /**
     * Charts history, the adapter's throughput, or nothing for nullptr.
     * sampleIndex is the running index of the newest sample, which scrolls the grid.
     */
    void setThroughput(const AdapterThroughputHistory::Adapter* history, std::uint64_t sampleIndex);

    /** Sets the time between the chart's samples, from which its X-axis label tells the time they cover. */
    void setSampleInterval(std::chrono::milliseconds interval);

    /** Sets the units of the chart's throughput: binary bytes per second until set. */
    void setDisplayUnits(domain::DisplayUnits units);

    [[nodiscard]] QString title() const;
    /** The device description shown below the name; empty when it is hidden. */
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] ReadoutGrid* readoutGrid() const;
    [[nodiscard]] charts::SparklineWidget* chart() const;
    [[nodiscard]] TonedLabel* statusLabel() const;
    [[nodiscard]] TonedLabel* descriptionLabel() const;

private:
    void setStatus(const QString& text, bool isUp);

    QLabel* m_titleLabel{nullptr};
    TonedLabel* m_statusLabel{nullptr};
    TonedLabel* m_descriptionLabel{nullptr};
    ReadoutGrid* m_readoutGrid{nullptr};
    charts::SparklineWidget* m_chart{nullptr};
    TonedLabel* m_chartPlaceholder{nullptr};
};

} // namespace sysmon::ui
