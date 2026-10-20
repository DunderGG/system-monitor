#include "ui/disk_performance_page.h"

#include <algorithm>

#include <QLabel>
#include <QVBoxLayout>

#include "ui/dashboard_formatting.h"
#include "ui/performance_formatting.h"
#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

namespace sysmon::ui
{

namespace
{

constexpr int kReadoutColumns = 3;

} // namespace

DiskPerformancePage::DiskPerformancePage(QWidget* parent) : PerformancePage("Disk", parent)
{
    contentLayout()->addWidget(new QLabel("Space used per volume", this));
    m_usageChart = createHistoryChart(charts::kPercentRange, formatChartPercent, formatPercent, this);
    contentLayout()->addWidget(m_usageChart, 1);

    m_readouts = new ReadoutGrid(kReadoutColumns, this);
    m_readouts->setReadouts(placeholderReadouts(diskReadouts(std::nullopt, displayUnits().bytes)));
    contentLayout()->addWidget(m_readouts);
}

void DiskPerformancePage::recordSamples(const domain::SystemSnapshot& snapshot)
{
    m_latest = snapshot.disks;

    // Record this tick's usage for every volume in the snapshot, and a gap for
    // every known volume that is missing from it (or when the query failed).
    if (snapshot.disks) {
        for (const auto& disk : *snapshot.disks) {
            VolumeHistory& volume = volumeHistory(disk.volumeName);
            volume.usage.push(disk.usagePercent);
        }
    }
    for (auto& volume : m_volumes) {
        const bool inSnapshot =
            snapshot.disks && std::ranges::any_of(*snapshot.disks, [&volume](const domain::DiskSample& disk) {
                return disk.volumeName == volume.name;
            });
        if (!inSnapshot) {
            volume.usage.push(std::nullopt);
        }
    }
    std::erase_if(m_volumes, [](const VolumeHistory& volume) { return !hasAnyValue(volume.usage); });
}

DiskPerformancePage::VolumeHistory& DiskPerformancePage::volumeHistory(const std::string& name)
{
    const auto position = std::ranges::lower_bound(m_volumes, name, {}, &VolumeHistory::name);
    if (position != m_volumes.end() && position->name == name) {
        return *position;
    }
    // Volumes keep their colour while others come and go, so a slot is
    // assigned once rather than following the volume's position.
    std::size_t slot = 0;
    while (std::ranges::find(m_volumes, slot, &VolumeHistory::colorSlot) != m_volumes.end()) {
        ++slot;
    }
    return *m_volumes.insert(position, VolumeHistory{.name = name, .colorSlot = slot});
}

void DiskPerformancePage::refresh()
{
    if (recordedCount() > 0) {
        m_readouts->setReadouts(diskReadouts(m_latest, displayUnits().bytes));
    }
    m_usageChart->setSeriesCount(std::max<std::size_t>(1, m_volumes.size()));
    if (m_volumes.empty()) {
        m_usageChart->setSeriesLabel(0, QString{});
        m_usageChart->setSamples({});
        return;
    }
    for (std::size_t index = 0; index < m_volumes.size(); ++index) {
        m_usageChart->setSeriesLabel(index, QString::fromStdString(m_volumes[index].name));
        m_usageChart->setSeriesColorSlot(index, m_volumes[index].colorSlot);
        showHistory(m_usageChart, index, m_volumes[index].usage);
    }
}

QString DiskPerformancePage::summary() const
{
    return recordedCount() == 0 ? QString("--") : diskSummary(m_latest);
}

charts::SparklineWidget* DiskPerformancePage::usageChart() const
{
    return m_usageChart;
}

ReadoutGrid* DiskPerformancePage::readouts() const
{
    return m_readouts;
}

std::vector<std::string> DiskPerformancePage::volumeNames() const
{
    std::vector<std::string> names;
    names.reserve(m_volumes.size());
    for (const auto& volume : m_volumes) {
        names.push_back(volume.name);
    }
    return names;
}

std::vector<std::size_t> DiskPerformancePage::volumeColorSlots() const
{
    std::vector<std::size_t> colorSlots;
    colorSlots.reserve(m_volumes.size());
    for (const auto& volume : m_volumes) {
        colorSlots.push_back(volume.colorSlot);
    }
    return colorSlots;
}

} // namespace sysmon::ui
