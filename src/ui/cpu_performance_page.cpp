#include "ui/cpu_performance_page.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>

#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QVBoxLayout>

#include "ui/performance_formatting.h"
#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

namespace sysmon::ui
{

namespace
{

constexpr charts::YRange kPercentRange{.min = 0.0f, .max = 100.0f};
constexpr int kReadoutColumns = 4;
constexpr int kCoreGridSpacing = 4;

// A near-square grid: 4 columns for 16 cores, 3 for 8, 12 for 128.
int coreGridColumns(std::size_t coreCount)
{
    return std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(coreCount)))));
}

} // namespace

CpuPerformancePage::CpuPerformancePage(QWidget* parent) : PerformancePage("CPU", parent)
{
    contentLayout()->addWidget(new QLabel("Utilization", this));
    m_totalChart = createHistoryChart(kPercentRange, formatChartPercent, this);
    contentLayout()->addWidget(m_totalChart, 3);

    m_readouts = new ReadoutGrid(kReadoutColumns, this);
    m_readouts->setReadouts(placeholderReadouts(cpuReadouts(std::nullopt, std::nullopt, std::nullopt)));
    contentLayout()->addWidget(m_readouts);

    m_coresLabel = new QLabel("Logical processors", this);
    m_coresLabel->hide();
    contentLayout()->addWidget(m_coresLabel);

    m_coreGrid = new QWidget(this);
    contentLayout()->addWidget(m_coreGrid, 2);
}

void CpuPerformancePage::recordSamples(const domain::SystemSnapshot& snapshot)
{
    m_latest = snapshot.cpu;
    m_latestActivity = snapshot.activity;
    m_latestUptime = snapshot.uptime;
    m_totalHistory.push(snapshot.cpu ? std::optional<float>{snapshot.cpu->totalUsagePercent} : std::nullopt);

    // An empty per-core list means per-core data is unavailable this tick: record gaps.
    const std::vector<float>* cores =
        (snapshot.cpu && !snapshot.cpu->coreUsagePercents.empty()) ? &snapshot.cpu->coreUsagePercents : nullptr;
    if (cores != nullptr && cores->size() != m_coreHistories.size()) {
        m_coreHistories.assign(cores->size(), SampleHistory{kHistoryCapacity});
    }
    for (std::size_t core = 0; core < m_coreHistories.size(); ++core) {
        m_coreHistories[core].push(cores != nullptr ? std::optional<float>{(*cores)[core]} : std::nullopt);
    }
}

void CpuPerformancePage::refresh()
{
    showHistory(m_totalChart, 0, m_totalHistory);
    if (recordedCount() > 0) {
        m_readouts->setReadouts(cpuReadouts(m_latest, m_latestActivity, m_latestUptime));
    }
    if (m_coreCharts.size() != m_coreHistories.size()) {
        rebuildCoreCharts();
    }
    for (std::size_t core = 0; core < m_coreCharts.size(); ++core) {
        showHistory(m_coreCharts[core], 0, m_coreHistories[core]);
    }
}

QString CpuPerformancePage::summary() const
{
    return recordedCount() == 0 ? QString("--") : cpuSummary(m_latest);
}

void CpuPerformancePage::rebuildCoreCharts()
{
    // Build the new grid in a fresh container and swap it in, so the old
    // charts and their grid positions go away together.
    auto* grid = new QWidget(this);
    auto* gridLayout = new QGridLayout(grid);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(kCoreGridSpacing);

    std::vector<charts::SparklineWidget*> coreCharts;
    const int columns = coreGridColumns(m_coreHistories.size());
    for (std::size_t core = 0; core < m_coreHistories.size(); ++core) {
        auto* chart = new charts::SparklineWidget(grid);
        chart->setCapacity(kHistoryCapacity);
        chart->setFixedRange(kPercentRange);
        chart->setGridVisible(true);
        chart->setToolTip(QString("CPU %1").arg(core));
        const int index = static_cast<int>(core);
        gridLayout->addWidget(chart, index / columns, index % columns);
        coreCharts.push_back(chart);
    }

    // replaceWidget() hands back the old layout item, which is not a QObject.
    const std::unique_ptr<QLayoutItem> oldItem{contentLayout()->replaceWidget(m_coreGrid, grid)};
    assert(oldItem && "core grid must be in the content layout");
    m_coreGrid->hide();
    m_coreGrid->deleteLater();
    m_coreGrid = grid;
    m_coreCharts = std::move(coreCharts);
    m_coresLabel->setVisible(!m_coreCharts.empty());
}

charts::SparklineWidget* CpuPerformancePage::totalChart() const
{
    return m_totalChart;
}

std::size_t CpuPerformancePage::coreChartCount() const
{
    return m_coreCharts.size();
}

charts::SparklineWidget* CpuPerformancePage::coreChart(std::size_t core) const
{
    assert(core < m_coreCharts.size() && "core chart index out of range");
    return m_coreCharts[core];
}

ReadoutGrid* CpuPerformancePage::readouts() const
{
    return m_readouts;
}

} // namespace sysmon::ui
