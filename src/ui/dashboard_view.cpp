#include "ui/dashboard_view.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace sysmon::ui
{

DashboardView::DashboardView(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("Dashboard", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    m_cpuLabel = new QLabel("CPU: --% (-- cores)", this);
    QFont metricFont = m_cpuLabel->font();
    metricFont.setPointSize(11);
    m_cpuLabel->setFont(metricFont);
    layout->addWidget(m_cpuLabel);

    m_memoryLabel = new QLabel("Memory: --% (-- / -- GiB)", this);
    m_memoryLabel->setFont(metricFont);
    layout->addWidget(m_memoryLabel);

    layout->addStretch();
}

void DashboardView::updateSnapshot(const domain::SystemSnapshot& snapshot)
{
    m_cpuLabel->setText(
        QString("CPU: %1% (%2 cores)")
            .arg(static_cast<double>(snapshot.cpu.totalUsagePercent), 0, 'f', 1)
            .arg(snapshot.cpu.coreCount));

    constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;
    const double totalGiB = static_cast<double>(snapshot.memory.totalBytes) / kBytesPerGiB;
    const uint64_t usedBytes = (snapshot.memory.totalBytes > snapshot.memory.availableBytes)
        ? (snapshot.memory.totalBytes - snapshot.memory.availableBytes)
        : 0ULL;
    const double usedGiB = static_cast<double>(usedBytes) / kBytesPerGiB;

    m_memoryLabel->setText(
        QString("Memory: %1% (%2 / %3 GiB)")
            .arg(static_cast<double>(snapshot.memory.usagePercent), 0, 'f', 1)
            .arg(usedGiB, 0, 'f', 1)
            .arg(totalGiB, 0, 'f', 1));
}

QString DashboardView::cpuText() const
{
    return m_cpuLabel->text();
}

QString DashboardView::memoryText() const
{
    return m_memoryLabel->text();
}

} // namespace sysmon::ui

