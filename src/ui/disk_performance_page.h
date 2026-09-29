#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "domain/disk_sample.h"
#include "ui/performance_page.h"

namespace sysmon::ui
{

/**
 * Disk page of the Performance tab: one chart of space used, with a labelled
 * series per volume, ordered by volume name. A volume that stops appearing in
 * the snapshots records gaps, and is dropped once its history holds no
 * samples.
 */
class DiskPerformancePage : public PerformancePage
{
    Q_OBJECT

public:
    explicit DiskPerformancePage(QWidget* parent = nullptr);
    ~DiskPerformancePage() override = default;

    void refresh() override;
    [[nodiscard]] QString summary() const override;

    [[nodiscard]] charts::SparklineWidget* usageChart() const;

    /** Names of the volumes with history, in series order. */
    [[nodiscard]] std::vector<std::string> volumeNames() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    struct VolumeHistory
    {
        std::string name;
        SampleHistory usage{kHistoryCapacity};
    };

    // Returns the history for name, inserting it in name order if it is new.
    VolumeHistory& volumeHistory(const std::string& name);

    charts::SparklineWidget* m_usageChart{nullptr};

    std::vector<VolumeHistory> m_volumes;
    std::optional<std::vector<domain::DiskSample>> m_latest;
};

} // namespace sysmon::ui
