#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include "domain/network_sample.h"
#include "ui/performance_page.h"

class QLabel;
class QScrollArea;

namespace sysmon::ui
{

/**
 * Network page of the Performance tab: a throughput chart per active adapter
 * (see chartedAdapters()), each with a receive and a send series on an
 * auto-scaled byte-rate axis. History is recorded for every adapter in the
 * snapshots, so an adapter that comes up shows what it did while it was
 * down. An adapter that leaves the snapshots records gaps and is dropped once
 * its history holds no samples.
 */
class NetworkPerformancePage : public PerformancePage
{
    Q_OBJECT

public:
    explicit NetworkPerformancePage(QWidget* parent = nullptr);
    ~NetworkPerformancePage() override = default;

    void refresh() override;
    [[nodiscard]] QString summary() const override;

    /** Adapter names (not display names) of the charted adapters, in display order. */
    [[nodiscard]] const std::vector<std::string>& chartedAdapterNames() const;
    [[nodiscard]] charts::SparklineWidget* adapterChart(std::size_t index) const;
    [[nodiscard]] QString adapterTitle(std::size_t index) const;

    /** Text shown in place of the charts when there are none, e.g. "No active network adapters". */
    [[nodiscard]] QString placeholderText() const;

protected:
    void recordSamples(const domain::SystemSnapshot& snapshot) override;

private:
    struct AdapterHistory
    {
        std::string adapterName;
        SampleHistory receive{kHistoryCapacity};
        SampleHistory send{kHistoryCapacity};
    };

    [[nodiscard]] const AdapterHistory* findHistory(const std::string& adapterName) const;
    void rebuildAdapterCharts(const std::vector<std::string>& adapterNames);

    QLabel* m_placeholderLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    std::vector<std::string> m_chartedNames;
    std::vector<QLabel*> m_chartTitles;
    std::vector<charts::SparklineWidget*> m_charts;

    std::vector<AdapterHistory> m_adapters;
    std::optional<std::vector<domain::NetworkSample>> m_latest;
};

} // namespace sysmon::ui
