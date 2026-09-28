#pragma once

#include <QString>
#include <QWidget>

#include "domain/system_snapshot.h"

class QLabel;

namespace sysmon::ui
{

class ResourceCard;

/**
 * Dashboard tab: an overall system status line and resource cards for CPU,
 * memory, disk, network, and uptime. Each card shows the current value, a
 * detail line, and a health status label; missing data is shown as "N/A".
 */
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardView(QWidget *parent = nullptr);
    ~DashboardView() override = default;

    /** Updates all cards from a newly received system snapshot. */
    void updateSnapshot(const domain::SystemSnapshot &snapshot);

    [[nodiscard]] ResourceCard *cpuCard() const;
    [[nodiscard]] ResourceCard *memoryCard() const;
    [[nodiscard]] ResourceCard *diskCard() const;
    [[nodiscard]] ResourceCard *networkCard() const;
    [[nodiscard]] ResourceCard *uptimeCard() const;

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
};

} // namespace sysmon::ui
