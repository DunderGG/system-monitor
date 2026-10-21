#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "domain/disk_sample.h"
#include "ui/performance_page.h"

namespace verdandi::ui
{

class ReadoutGrid;

/**
 * Disk page of the Performance tab: one chart of space used, with a labelled
 * series per volume, ordered by volume name. A volume that stops appearing in
 * the snapshots records gaps, and is dropped once its history holds no
 * samples. Below the chart, a readout per volume shows its used, free, and
 * total space (see diskReadouts()).
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
    [[nodiscard]] ReadoutGrid* readouts() const;

    /** Names of the volumes with history, in series order. */
    [[nodiscard]] std::vector<std::string> volumeNames() const;

    /** Colour slot of each volume with history, in series order (see SparklineWidget::setSeriesColorSlot()). */
    [[nodiscard]] std::vector<std::size_t> volumeColorSlots() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    struct VolumeHistory
    {
        std::string name;
        // Chart colour slot, kept for as long as the volume has history.
        std::size_t colorSlot{0};
        SampleHistory usage{kHistoryCapacity};
    };

    // Returns the history for name, inserting it in name order if it is new.
    // A new volume takes the lowest colour slot no other volume uses.
    VolumeHistory& volumeHistory(const std::string& name);

    charts::SparklineWidget* m_usageChart{nullptr};
    ReadoutGrid* m_readouts{nullptr};

    std::vector<VolumeHistory> m_volumes;
    std::optional<std::vector<domain::DiskSample>> m_latest;
};

} // namespace verdandi::ui
