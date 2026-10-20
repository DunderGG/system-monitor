#include "ui/dashboard_view.h"

#include <array>
#include <utility>

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/charts/sparkline_widget.h"
#include "ui/dashboard_formatting.h"
#include "ui/resource_card.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

constexpr int kCardColumns = 3;

QString overallStatusLine(domain::HealthLevel level)
{
    return QString("System status: %1").arg(healthLevelText(level));
}

charts::SparklineWidget* addSparkline(ResourceCard* card, std::optional<charts::YRange> fixedRange)
{
    auto* sparkline = new charts::SparklineWidget(card);
    sparkline->setCapacity(DashboardView::kHistoryCapacity);
    if (fixedRange) {
        sparkline->setFixedRange(*fixedRange);
    } else {
        sparkline->setAutoRange();
    }
    card->setChart(sparkline);
    return sparkline;
}

// Appends one sample (std::nullopt records a gap) and refreshes the chart.
void recordSample(domain::RingBuffer<std::optional<float>>& history, charts::SparklineWidget* sparkline,
                  std::optional<float> sample)
{
    history.push(sample);
    sparkline->setSamples(history.samples());
}

} // namespace

DashboardView::DashboardView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kViewMargin, kViewMargin, kViewMargin, kViewMargin);
    layout->setSpacing(kSectionSpacing);

    auto* titleLabel = new QLabel("Dashboard", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(kTitlePointSize);
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

    m_cpuSparkline = addSparkline(m_cpuCard, charts::kPercentRange);
    m_memorySparkline = addSparkline(m_memoryCard, charts::kPercentRange);
    m_diskSparkline = addSparkline(m_diskCard, charts::kPercentRange);
    m_networkSparkline = addSparkline(m_networkCard, std::nullopt); // Throughput has no natural maximum.

    m_cardGrid = new QGridLayout();
    m_cardGrid->setSpacing(kSectionSpacing);
    for (int column = 0; column < kCardColumns; ++column) {
        m_cardGrid->setColumnStretch(column, 1);
    }
    layout->addLayout(m_cardGrid);

    m_noCardsLabel = new QLabel("Every card is turned off. Choose the cards to show in File > Settings.", this);
    m_noCardsLabel->setWordWrap(true);
    layout->addWidget(m_noCardsLabel);
    layout->addStretch();

    setPanels(m_panels);
}

void DashboardView::setPanels(const domain::DashboardPanels& panels)
{
    m_panels = panels;
    const std::array cards{
        std::pair{m_cpuCard, panels.cpu},       std::pair{m_memoryCard, panels.memory},
        std::pair{m_diskCard, panels.disk},     std::pair{m_networkCard, panels.network},
        std::pair{m_uptimeCard, panels.uptime},
    };

    // The cards shown take the grid's cells in order, so a hidden card leaves no hole.
    int cell = 0;
    for (const auto& [card, isShown] : cards) {
        m_cardGrid->removeWidget(card);
        card->setVisible(isShown);
        if (isShown) {
            m_cardGrid->addWidget(card, cell / kCardColumns, cell % kCardColumns);
            ++cell;
        }
    }
    m_noCardsLabel->setVisible(cell == 0);
}

const domain::DashboardPanels& DashboardView::panels() const
{
    return m_panels;
}

void DashboardView::updateSnapshot(const domain::SystemSnapshot& snapshot)
{
    m_hasSnapshot = true;
    m_latestMemory = snapshot.memory;
    m_latestDisks = snapshot.disks;
    m_latestNetworks = snapshot.networks;
    m_latestConnectivity = snapshot.connectivity;

    m_overallStatusLabel->setText(overallStatusLine(snapshot.health.overall));
    updateCpuCard(snapshot);
    updateMemoryCard(snapshot);
    updateDiskCard(snapshot);
    updateNetworkCard(snapshot);
    updateUptimeCard(snapshot);
}

void DashboardView::setDisplayUnits(domain::DisplayUnits units)
{
    m_displayUnits = units;
    // The latest sizes and rates are shown in the new units at once, not at the next snapshot.
    if (m_hasSnapshot) {
        showMemoryValue();
        showDiskValue();
        showNetworkValue();
    }
}

domain::DisplayUnits DashboardView::displayUnits() const
{
    return m_displayUnits;
}

void DashboardView::updateCpuCard(const domain::SystemSnapshot& snapshot)
{
    m_cpuCard->setStatus(snapshot.health.cpu);
    if (!snapshot.cpu) {
        recordSample(m_cpuHistory, m_cpuSparkline, std::nullopt);
        m_cpuCard->setValue(kNotAvailable);
        return;
    }
    recordSample(m_cpuHistory, m_cpuSparkline, snapshot.cpu->totalUsagePercent);
    m_cpuCard->setValue(formatPercent(snapshot.cpu->totalUsagePercent),
                        QString("%1 cores").arg(snapshot.cpu->coreCount));
}

void DashboardView::updateMemoryCard(const domain::SystemSnapshot& snapshot)
{
    m_memoryCard->setStatus(snapshot.health.memory);
    recordSample(m_memoryHistory, m_memorySparkline,
                 snapshot.memory ? std::optional<float>{snapshot.memory->usagePercent} : std::nullopt);
    showMemoryValue();
}

void DashboardView::showMemoryValue()
{
    if (!m_latestMemory) {
        m_memoryCard->setValue(kNotAvailable);
        return;
    }
    const auto& memory = *m_latestMemory;
    const uint64_t usedBytes = usedMemoryBytes(memory);
    m_memoryCard->setValue(
        formatPercent(memory.usagePercent),
        QString("%1 of %2 used")
            .arg(formatBytes(usedBytes, m_displayUnits.bytes), formatBytes(memory.totalBytes, m_displayUnits.bytes)));
}

void DashboardView::updateDiskCard(const domain::SystemSnapshot& snapshot)
{
    m_diskCard->setStatus(snapshot.health.disk);
    const auto* fullest = snapshot.disks ? fullestVolume(*snapshot.disks) : nullptr;
    recordSample(m_diskHistory, m_diskSparkline,
                 fullest != nullptr ? std::optional<float>{fullest->usagePercent} : std::nullopt);
    showDiskValue();
}

void DashboardView::showDiskValue()
{
    const auto* fullest = m_latestDisks ? fullestVolume(*m_latestDisks) : nullptr;
    if (fullest == nullptr) {
        m_diskCard->setValue(kNotAvailable);
        return;
    }
    QString detail =
        QString("%1 %2 free")
            .arg(QString::fromStdString(fullest->volumeName), formatBytes(fullest->freeBytes, m_displayUnits.bytes));
    if (m_latestDisks->size() > 1) {
        detail += QString(" (fullest of %1 volumes)").arg(m_latestDisks->size());
    }
    m_diskCard->setValue(formatPercent(fullest->usagePercent), detail);
}

void DashboardView::updateNetworkCard(const domain::SystemSnapshot& snapshot)
{
    m_networkCard->setStatus(snapshot.health.network);
    // The sparkline charts combined (in + out) throughput.
    const auto totals = snapshot.networks ? sumActiveThroughput(*snapshot.networks) : std::nullopt;
    recordSample(m_networkHistory, m_networkSparkline,
                 totals ? std::optional<float>{static_cast<float>(totals->inBytesPerSec + totals->outBytesPerSec)}
                        : std::nullopt);
    showNetworkValue();
}

void DashboardView::showNetworkValue()
{
    const QString connectivity = connectivityText(m_latestConnectivity);

    // No adapter data at all (no collector, or the query failed): nothing is known
    // about the adapters, so do not claim that none is active.
    if (!m_latestNetworks) {
        m_networkCard->setValue(kNotAvailable, connectivity);
        return;
    }
    if (!hasActiveAdapter(*m_latestNetworks)) {
        m_networkCard->setValue("No active adapter", connectivity);
        return;
    }
    const auto totals = sumActiveThroughput(*m_latestNetworks);
    if (!totals) {
        m_networkCard->setValue(kNotAvailable, connectivity);
        return;
    }
    m_networkCard->setValue(QString("In %1\nOut %2")
                                .arg(formatNetworkRate(totals->inBytesPerSec, m_displayUnits),
                                     formatNetworkRate(totals->outBytesPerSec, m_displayUnits)),
                            connectivity);
}

void DashboardView::updateUptimeCard(const domain::SystemSnapshot& snapshot)
{
    m_uptimeCard->setValue(snapshot.uptime ? formatUptime(*snapshot.uptime) : kNotAvailable, "Since last boot");
}

ResourceCard* DashboardView::cpuCard() const
{
    return m_cpuCard;
}

ResourceCard* DashboardView::memoryCard() const
{
    return m_memoryCard;
}

ResourceCard* DashboardView::diskCard() const
{
    return m_diskCard;
}

ResourceCard* DashboardView::networkCard() const
{
    return m_networkCard;
}

ResourceCard* DashboardView::uptimeCard() const
{
    return m_uptimeCard;
}

charts::SparklineWidget* DashboardView::cpuSparkline() const
{
    return m_cpuSparkline;
}

charts::SparklineWidget* DashboardView::memorySparkline() const
{
    return m_memorySparkline;
}

charts::SparklineWidget* DashboardView::diskSparkline() const
{
    return m_diskSparkline;
}

charts::SparklineWidget* DashboardView::networkSparkline() const
{
    return m_networkSparkline;
}

QString DashboardView::overallStatusText() const
{
    return m_overallStatusLabel->text();
}

QLabel* DashboardView::noCardsLabel() const
{
    return m_noCardsLabel;
}

} // namespace sysmon::ui
