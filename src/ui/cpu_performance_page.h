#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <QString>

#include "domain/cpu_sample.h"
#include "ui/performance_page.h"

class QLabel;

namespace sysmon::ui
{

/**
 * CPU page of the Performance tab: a total utilization chart above a grid of
 * small per-core charts, one per logical processor. The grid is rebuilt when
 * the number of cores in the samples changes, and the per-core history
 * restarts then, because core indices no longer line up.
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

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    void rebuildCoreCharts();

    charts::SparklineWidget* m_totalChart{nullptr};
    QLabel* m_coresLabel{nullptr};
    QWidget* m_coreGrid{nullptr};
    std::vector<charts::SparklineWidget*> m_coreCharts;

    SampleHistory m_totalHistory{kHistoryCapacity};
    std::vector<SampleHistory> m_coreHistories;
    std::optional<domain::CpuSample> m_latest;
};

} // namespace sysmon::ui
