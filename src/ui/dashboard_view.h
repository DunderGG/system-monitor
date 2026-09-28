#pragma once

#include <cstddef>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/ring_buffer.h"
#include "domain/system_snapshot.h"

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
 */
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    /** Samples of history per sparkline: one minute at the default 1 Hz sampling rate. */
    static constexpr std::size_t kHistoryCapacity = 60;

    explicit DashboardView(QWidget *parent = nullptr);
    ~DashboardView() override = default;

    /** Updates all cards from a newly received system snapshot. */
    void updateSnapshot(const domain::SystemSnapshot &snapshot);

    [[nodiscard]] ResourceCard *cpuCard() const;
    [[nodiscard]] ResourceCard *memoryCard() const;
    [[nodiscard]] ResourceCard *diskCard() const;
    [[nodiscard]] ResourceCard *networkCard() const;
    [[nodiscard]] ResourceCard *uptimeCard() const;

    [[nodiscard]] charts::SparklineWidget *cpuSparkline() const;
    [[nodiscard]] charts::SparklineWidget *memorySparkline() const;
    [[nodiscard]] charts::SparklineWidget *diskSparkline() const;
    [[nodiscard]] charts::SparklineWidget *networkSparkline() const;

    /** Returns the overall status line text, e.g. "System status: Healthy". */
    [[nodiscard]] QString overallStatusText() const;

private:
    void updateCpuCard(const domain::SystemSnapshot &snapshot);
    void updateMemoryCard(const domain::SystemSnapshot &snapshot);
    void updateDiskCard(const domain::SystemSnapshot &snapshot);
    void updateNetworkCard(const domain::SystemSnapshot &snapshot);
    void updateUptimeCard(const domain::SystemSnapshot &snapshot);

    QLabel *m_overallStatusLabel{nullptr};
    ResourceCard *m_cpuCard{nullptr};
    ResourceCard *m_memoryCard{nullptr};
    ResourceCard *m_diskCard{nullptr};
    ResourceCard *m_networkCard{nullptr};
    ResourceCard *m_uptimeCard{nullptr};

    charts::SparklineWidget *m_cpuSparkline{nullptr};
    charts::SparklineWidget *m_memorySparkline{nullptr};
    charts::SparklineWidget *m_diskSparkline{nullptr};
    charts::SparklineWidget *m_networkSparkline{nullptr};

    domain::RingBuffer<std::optional<float>> m_cpuHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_memoryHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_diskHistory{kHistoryCapacity};
    domain::RingBuffer<std::optional<float>> m_networkHistory{kHistoryCapacity};
};

} // namespace sysmon::ui
