#pragma once

#include <cstddef>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/ring_buffer.h"
#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "ui/sample_history.h"

class QGridLayout;
class QLabel;

namespace sysmon::ui
{

class ResourceCard;

namespace charts
{
class SparklineWidget;
}

/**
 * Dashboard tab: an overall system status line and resource cards for CPU,
 * memory, disk, network, and uptime. Each card shows the current value, a
 * detail line, and a health status label; missing data is shown as "N/A".
 * CPU, memory, disk, and network cards also show a mini sparkline of the last
 * kHistoryCapacity samples, kept in RingBuffers owned by this view (missing
 * samples appear as gaps).
 *
 * The settings choose which cards are shown (setPanels(), DC-125). The cards
 * shown fill the grid in that order from its first cell, and a card that is
 * hidden keeps recording, so its sparkline is whole when it is shown again.
 */
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    /** Samples of history per sparkline: one minute (kOneMinuteOfSamples). */
    static constexpr std::size_t kHistoryCapacity = kOneMinuteOfSamples;

    explicit DashboardView(QWidget* parent = nullptr);
    ~DashboardView() override = default;

    /** Updates all cards from a newly received system snapshot. */
    void updateSnapshot(const domain::SystemSnapshot& snapshot);

    /**
     * Shows the cards that panels turns on and hides the others. When every
     * card is off, a line in their place says where to turn them on. Every
     * card is shown until set.
     */
    void setPanels(const domain::DashboardPanels& panels);
    [[nodiscard]] const domain::DashboardPanels& panels() const;

    [[nodiscard]] ResourceCard* cpuCard() const;
    [[nodiscard]] ResourceCard* memoryCard() const;
    [[nodiscard]] ResourceCard* diskCard() const;
    [[nodiscard]] ResourceCard* networkCard() const;
    [[nodiscard]] ResourceCard* uptimeCard() const;

    [[nodiscard]] charts::SparklineWidget* cpuSparkline() const;
    [[nodiscard]] charts::SparklineWidget* memorySparkline() const;
    [[nodiscard]] charts::SparklineWidget* diskSparkline() const;
    [[nodiscard]] charts::SparklineWidget* networkSparkline() const;

    /** Returns the overall status line text, e.g. "System status: Healthy". */
    [[nodiscard]] QString overallStatusText() const;

    /** The line shown in place of the cards while every card is off. */
    [[nodiscard]] QLabel* noCardsLabel() const;

private:
    void updateCpuCard(const domain::SystemSnapshot& snapshot);
    void updateMemoryCard(const domain::SystemSnapshot& snapshot);
    void updateDiskCard(const domain::SystemSnapshot& snapshot);
    void updateNetworkCard(const domain::SystemSnapshot& snapshot);
    void updateUptimeCard(const domain::SystemSnapshot& snapshot);

    QLabel* m_overallStatusLabel{nullptr};
    QGridLayout* m_cardGrid{nullptr};
    QLabel* m_noCardsLabel{nullptr};
    domain::DashboardPanels m_panels{};
    ResourceCard* m_cpuCard{nullptr};
    ResourceCard* m_memoryCard{nullptr};
    ResourceCard* m_diskCard{nullptr};
    ResourceCard* m_networkCard{nullptr};
    ResourceCard* m_uptimeCard{nullptr};

    charts::SparklineWidget* m_cpuSparkline{nullptr};
    charts::SparklineWidget* m_memorySparkline{nullptr};
    charts::SparklineWidget* m_diskSparkline{nullptr};
    charts::SparklineWidget* m_networkSparkline{nullptr};

    domain::RingBuffer<std::optional<float>> m_cpuHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_memoryHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_diskHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_networkHistory{kHistoryCapacity};
};

} // namespace sysmon::ui
