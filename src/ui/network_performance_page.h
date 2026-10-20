#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "domain/network_sample.h"
#include "domain/settings.h"
#include "ui/adapter_throughput_history.h"
#include "ui/performance_page.h"

class QLabel;
class QScrollArea;

namespace sysmon::ui
{

class ReadoutGrid;

/**
 * Network page of the Performance tab: a throughput chart per active adapter
 * that can carry traffic (see chartedAdapters()), each with a receive and a send series on an
 * auto-scaled throughput axis in the display units. History is recorded for every adapter in the
 * snapshots (see AdapterThroughputHistory), so an adapter that comes up shows
 * what it did while it was down. Each chart has the adapter's readouts below
 * it (see adapterReadouts()).
 */
class NetworkPerformancePage : public PerformancePage
{
    Q_OBJECT

public:
    explicit NetworkPerformancePage(QWidget* parent = nullptr);
    ~NetworkPerformancePage() override = default;

    void refresh() override;
    [[nodiscard]] QString summary() const override;

    /** Interface IDs of the charted adapters, in display order. Charts and histories are keyed by them. */
    [[nodiscard]] const std::vector<uint64_t>& chartedInterfaceIds() const;
    /** Adapter names (not display names) of the charted adapters, in display order, as of the last read. */
    [[nodiscard]] const std::vector<std::string>& chartedAdapterNames() const;
    [[nodiscard]] charts::SparklineWidget* adapterChart(std::size_t index) const;
    [[nodiscard]] QString adapterTitle(std::size_t index) const;
    [[nodiscard]] ReadoutGrid* adapterReadoutGrid(std::size_t index) const;

    /** Text shown in place of the charts when there are none, e.g. "No active network adapters". */
    [[nodiscard]] QString placeholderText() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    void rebuildAdapterCharts(const std::vector<uint64_t>& interfaceIds);

    QLabel* m_placeholderLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    std::vector<uint64_t> m_chartedIds;
    std::vector<std::string> m_chartedNames;
    std::vector<QLabel*> m_chartTitles;
    std::vector<charts::SparklineWidget*> m_charts;
    std::vector<ReadoutGrid*> m_readoutGrids;
    // The units the charts format their values in, which refresh() keeps at displayUnits().
    domain::DisplayUnits m_chartUnits{};

    AdapterThroughputHistory m_history{kHistoryCapacity};
    std::optional<std::vector<domain::NetworkSample>> m_latest;
};

} // namespace sysmon::ui
