#include "ui/dashboard_view.h"

#include <array>

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/dashboard_formatting.h"
#include "ui/resource_card.h"

namespace sysmon::ui
{

namespace
{

constexpr int kCardColumns = 3;
const QString kNotAvailable = "N/A";

QString overallStatusLine(domain::HealthLevel level)
{
    return QString("System status: %1").arg(healthLevelText(level));
}

} // namespace

DashboardView::DashboardView(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel("Dashboard", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    m_overallStatusLabel = new QLabel(overallStatusLine(domain::HealthLevel::Unknown), this);
    QFont statusFont = m_overallStatusLabel->font();
    statusFont.setPointSize(11);
    m_overallStatusLabel->setFont(statusFont);
    layout->addWidget(m_overallStatusLabel);

    m_cpuCard = new ResourceCard("CPU", this);
    m_memoryCard = new ResourceCard("Memory", this);
    m_diskCard = new ResourceCard("Disk", this);
    m_networkCard = new ResourceCard("Network", this);
    m_uptimeCard = new ResourceCard("Uptime", this);
    m_uptimeCard->setStatus(std::nullopt); // Uptime has no health dimension.

    auto *grid = new QGridLayout();
    grid->setSpacing(12);
    const std::array cards{m_cpuCard, m_memoryCard, m_diskCard, m_networkCard, m_uptimeCard};
    for (int index = 0; index < static_cast<int>(cards.size()); ++index) {
        grid->addWidget(cards[static_cast<std::size_t>(index)], index / kCardColumns, index % kCardColumns);
    }
    for (int column = 0; column < kCardColumns; ++column) {
        grid->setColumnStretch(column, 1);
    }
    layout->addLayout(grid);
    layout->addStretch();
}

void DashboardView::updateSnapshot(const domain::SystemSnapshot &snapshot)
{
    m_overallStatusLabel->setText(overallStatusLine(snapshot.health.overall));
    updateCpuCard(snapshot);
    updateMemoryCard(snapshot);
    updateDiskCard(snapshot);
    updateNetworkCard(snapshot);
    updateUptimeCard(snapshot);
}

void DashboardView::updateCpuCard(const domain::SystemSnapshot &snapshot)
{
    m_cpuCard->setStatus(snapshot.health.cpu);
    if (!snapshot.cpu) {
        m_cpuCard->setValue(kNotAvailable);
        return;
    }
    m_cpuCard->setValue(formatPercent(snapshot.cpu->totalUsagePercent),
                        QString("%1 cores").arg(snapshot.cpu->coreCount));
}

void DashboardView::updateMemoryCard(const domain::SystemSnapshot &snapshot)
{
    m_memoryCard->setStatus(snapshot.health.memory);
    if (!snapshot.memory) {
        m_memoryCard->setValue(kNotAvailable);
        return;
    }
    const auto &memory = *snapshot.memory;
    const uint64_t usedBytes =
        (memory.totalBytes > memory.availableBytes) ? (memory.totalBytes - memory.availableBytes) : 0ULL;
    m_memoryCard->setValue(formatPercent(memory.usagePercent),
                           QString("%1 of %2 used").arg(formatBytes(usedBytes), formatBytes(memory.totalBytes)));
}

void DashboardView::updateDiskCard(const domain::SystemSnapshot &snapshot)
{
    m_diskCard->setStatus(snapshot.health.disk);
    const auto *fullest = fullestVolume(snapshot.disks);
    if (fullest == nullptr) {
        m_diskCard->setValue(kNotAvailable);
        return;
    }
    QString detail = QString("%1 %2 free").arg(QString::fromStdString(fullest->volumeName),
                                               formatBytes(fullest->freeBytes));
    if (snapshot.disks.size() > 1) {
        detail += QString(" (fullest of %1 volumes)").arg(snapshot.disks.size());
    }
    m_diskCard->setValue(formatPercent(fullest->usagePercent), detail);
}

void DashboardView::updateNetworkCard(const domain::SystemSnapshot &snapshot)
{
    m_networkCard->setStatus(snapshot.health.network);
    const QString connectivity = connectivityText(snapshot.connectivity);

    if (!hasActiveAdapter(snapshot.networks)) {
        m_networkCard->setValue("No active adapter", connectivity);
        return;
    }
    const auto totals = sumActiveThroughput(snapshot.networks);
    if (!totals) {
        m_networkCard->setValue(kNotAvailable, connectivity);
        return;
    }
    m_networkCard->setValue(
        QString("In %1\nOut %2").arg(formatByteRate(totals->inBytesPerSec), formatByteRate(totals->outBytesPerSec)),
        connectivity);
}

void DashboardView::updateUptimeCard(const domain::SystemSnapshot &snapshot)
{
    m_uptimeCard->setValue(snapshot.uptime ? formatUptime(*snapshot.uptime) : kNotAvailable, "Since last boot");
}

ResourceCard *DashboardView::cpuCard() const
{
    return m_cpuCard;
}

ResourceCard *DashboardView::memoryCard() const
{
    return m_memoryCard;
}

ResourceCard *DashboardView::diskCard() const
{
    return m_diskCard;
}

ResourceCard *DashboardView::networkCard() const
{
    return m_networkCard;
}

ResourceCard *DashboardView::uptimeCard() const
{
    return m_uptimeCard;
}

QString DashboardView::overallStatusText() const
{
    return m_overallStatusLabel->text();
}

} // namespace sysmon::ui
