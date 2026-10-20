#include "ui/memory_performance_page.h"

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

constexpr int kReadoutColumns = 4;

} // namespace

MemoryPerformancePage::MemoryPerformancePage(QWidget* parent) : PerformancePage("Memory", parent)
{
    contentLayout()->addWidget(new QLabel("Memory usage", this));
    m_usageChart = createHistoryChart(charts::kPercentRange, formatChartPercent, formatPercent, this);
    contentLayout()->addWidget(m_usageChart, 1);

    contentLayout()->addWidget(new QLabel("Committed (percent of commit limit)", this));
    m_commitChart = createHistoryChart(charts::kPercentRange, formatChartPercent, formatPercent, this);
    contentLayout()->addWidget(m_commitChart, 1);

    m_readouts = new ReadoutGrid(kReadoutColumns, this);
    m_readouts->setReadouts(placeholderReadouts(memoryReadouts(std::nullopt, displayUnits().bytes)));
    contentLayout()->addWidget(m_readouts);
}

void MemoryPerformancePage::recordSamples(const domain::SystemSnapshot& snapshot)
{
    m_latest = snapshot.memory;
    m_usageHistory.push(snapshot.memory ? std::optional<float>{snapshot.memory->usagePercent} : std::nullopt);
    m_commitHistory.push(snapshot.memory ? commitPercent(*snapshot.memory) : std::nullopt);
}

void MemoryPerformancePage::refresh()
{
    showHistory(m_usageChart, 0, m_usageHistory);
    showHistory(m_commitChart, 0, m_commitHistory);
    if (recordedCount() > 0) {
        m_readouts->setReadouts(memoryReadouts(m_latest, displayUnits().bytes));
    }
}

QString MemoryPerformancePage::summary() const
{
    return recordedCount() == 0 ? QString("--") : memorySummary(m_latest, displayUnits().bytes);
}

charts::SparklineWidget* MemoryPerformancePage::usageChart() const
{
    return m_usageChart;
}

charts::SparklineWidget* MemoryPerformancePage::commitChart() const
{
    return m_commitChart;
}

ReadoutGrid* MemoryPerformancePage::readouts() const
{
    return m_readouts;
}

} // namespace sysmon::ui
