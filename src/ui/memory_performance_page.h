#pragma once

#include <optional>

#include <QString>

#include "domain/memory_sample.h"
#include "ui/performance_page.h"

namespace sysmon::ui
{

/**
 * Memory page of the Performance tab: physical memory usage and commit charge,
 * each as a percentage chart. Commit charge is shown as a percentage of the
 * commit limit, which moves as the page file grows.
 */
class MemoryPerformancePage : public PerformancePage
{
    Q_OBJECT

public:
    explicit MemoryPerformancePage(QWidget* parent = nullptr);
    ~MemoryPerformancePage() override = default;

    void refresh() override;
    [[nodiscard]] QString summary() const override;

    [[nodiscard]] charts::SparklineWidget* usageChart() const;
    [[nodiscard]] charts::SparklineWidget* commitChart() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    charts::SparklineWidget* m_usageChart{nullptr};
    charts::SparklineWidget* m_commitChart{nullptr};

    SampleHistory m_usageHistory{kHistoryCapacity};
    SampleHistory m_commitHistory{kHistoryCapacity};
    std::optional<domain::MemorySample> m_latest;
};

} // namespace sysmon::ui
