#include "ui/memory_performance_page.h"

#include <QLabel>
#include <QVBoxLayout>

#include "ui/performance_formatting.h"

namespace sysmon::ui
{

namespace
{

constexpr charts::YRange kPercentRange{.min = 0.0f, .max = 100.0f};

} // namespace

MemoryPerformancePage::MemoryPerformancePage(QWidget* parent) : PerformancePage("Memory", parent)
{
    contentLayout()->addWidget(new QLabel("Memory usage", this));
    m_usageChart = createHistoryChart(kPercentRange, formatChartPercent, this);
    contentLayout()->addWidget(m_usageChart, 1);

    contentLayout()->addWidget(new QLabel("Committed (percent of commit limit)", this));
    m_commitChart = createHistoryChart(kPercentRange, formatChartPercent, this);
    contentLayout()->addWidget(m_commitChart, 1);
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
}

QString MemoryPerformancePage::summary() const
{
    return recordedCount() == 0 ? QString("--") : memorySummary(m_latest);
}

charts::SparklineWidget* MemoryPerformancePage::usageChart() const
{
    return m_usageChart;
}

charts::SparklineWidget* MemoryPerformancePage::commitChart() const
{
    return m_commitChart;
}

} // namespace sysmon::ui
