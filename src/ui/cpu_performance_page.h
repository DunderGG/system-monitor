#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <QString>

#include "domain/cpu_sample.h"
#include "domain/system_activity_sample.h"
#include "ui/performance_page.h"

class QLabel;

namespace sysmon::ui
{

class ReadoutGrid;

/**
 * CPU page of the Performance tab: a total utilization chart and its readouts
 * (see cpuReadouts()) above a grid of small per-core charts, one per logical
 * processor. The grid is rebuilt when the number of cores in the samples
 * changes, and the per-core history restarts then, because core indices no
 * longer line up.
 */
class CpuPerformancePage : public PerformancePage
{
    Q_OBJECT

public:
    explicit CpuPerformancePage(QWidget* parent = nullptr);
    ~CpuPerformancePage() override = default;

    void refresh() override;
    [[nodiscard]] QString summary() const override;

    [[nodiscard]] charts::SparklineWidget* totalChart() const;
    [[nodiscard]] std::size_t coreChartCount() const;
    [[nodiscard]] charts::SparklineWidget* coreChart(std::size_t core) const;
    [[nodiscard]] ReadoutGrid* readouts() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    void rebuildCoreCharts();

    charts::SparklineWidget* m_totalChart{nullptr};
    ReadoutGrid* m_readouts{nullptr};
    QLabel* m_coresLabel{nullptr};
    QWidget* m_coreGrid{nullptr};
    std::vector<charts::SparklineWidget*> m_coreCharts;

    SampleHistory m_totalHistory{kHistoryCapacity};
    std::vector<SampleHistory> m_coreHistories;
    std::optional<domain::CpuSample> m_latest;
    std::optional<domain::SystemActivitySample> m_latestActivity;
    std::optional<std::chrono::milliseconds> m_latestUptime;
};

} // namespace sysmon::ui
