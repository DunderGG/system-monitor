#include "ui/network_performance_page.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include <QFont>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ui/performance_formatting.h"
#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"

namespace sysmon::ui
{

namespace
{

constexpr int kAdapterChartMinimumHeight = 160;
constexpr int kReadoutColumns = 5;

std::optional<float> toChartValue(const std::optional<uint64_t>& bytesPerSecond)
{
    return bytesPerSecond ? std::optional<float>{static_cast<float>(*bytesPerSecond)} : std::nullopt;
}

} // namespace

NetworkPerformancePage::NetworkPerformancePage(QWidget* parent) : PerformancePage("Network", parent)
{
    m_placeholderLabel = new QLabel("--", this);
    contentLayout()->addWidget(m_placeholderLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    contentLayout()->addWidget(m_scrollArea, 1);
    rebuildAdapterCharts({});
}

void NetworkPerformancePage::recordSamples(const domain::SystemSnapshot& snapshot)
{
    m_latest = snapshot.networks;

    const auto inSnapshot = [&snapshot](const std::string& adapterName) {
        return snapshot.networks && std::ranges::any_of(*snapshot.networks, [&adapterName](const auto& adapter) {
                   return adapter.adapterName == adapterName;
               });
    };

    if (snapshot.networks) {
        for (const auto& adapter : *snapshot.networks) {
            auto it = std::ranges::find(m_adapters, adapter.adapterName, &AdapterHistory::adapterName);
            if (it == m_adapters.end()) {
                it = m_adapters.insert(m_adapters.end(), AdapterHistory{.adapterName = adapter.adapterName});
            }
            it->receive.push(toChartValue(adapter.inBytesPerSec));
            it->send.push(toChartValue(adapter.outBytesPerSec));
        }
    }
    for (auto& history : m_adapters) {
        if (!inSnapshot(history.adapterName)) {
            history.receive.push(std::nullopt);
            history.send.push(std::nullopt);
        }
    }
    // Adapters still in the snapshot are kept even without values: a new
    // adapter has no rate until its second sample.
    std::erase_if(m_adapters, [&inSnapshot](const AdapterHistory& history) {
        return !inSnapshot(history.adapterName) && !hasAnyValue(history.receive) && !hasAnyValue(history.send);
    });
}

const NetworkPerformancePage::AdapterHistory* NetworkPerformancePage::findHistory(const std::string& adapterName) const
{
    const auto it = std::ranges::find(m_adapters, adapterName, &AdapterHistory::adapterName);
    return it != m_adapters.end() ? &*it : nullptr;
}

void NetworkPerformancePage::refresh()
{
    // Without a successful read, keep charting the adapters from the last one;
    // their histories show gaps for this tick.
    if (m_latest) {
        const auto charted = chartedAdapters(*m_latest);
        std::vector<std::string> names;
        names.reserve(charted.size());
        for (const auto* adapter : charted) {
            names.push_back(adapter->adapterName);
        }
        if (names != m_chartedNames) {
            rebuildAdapterCharts(names);
        }
        for (std::size_t index = 0; index < charted.size(); ++index) {
            m_chartTitles[index]->setText(adapterDisplayName(*charted[index]));
            m_readoutGrids[index]->setReadouts(adapterReadouts(*charted[index]));
        }
    } else {
        for (auto* readoutGrid : m_readoutGrids) {
            readoutGrid->setReadouts(adapterReadouts(std::nullopt));
        }
    }

    for (std::size_t index = 0; index < m_charts.size(); ++index) {
        const AdapterHistory* history = findHistory(m_chartedNames[index]);
        if (history == nullptr) {
            m_charts[index]->setSeriesSamples(0, {});
            m_charts[index]->setSeriesSamples(1, {});
            continue;
        }
        showHistory(m_charts[index], 0, history->receive);
        showHistory(m_charts[index], 1, history->send);
    }

    if (m_charts.empty()) {
        m_placeholderLabel->setText(m_latest ? "No active network adapters" : "N/A");
    }
    m_placeholderLabel->setVisible(m_charts.empty());
}

void NetworkPerformancePage::rebuildAdapterCharts(const std::vector<std::string>& adapterNames)
{
    auto* list = new QWidget(m_scrollArea);
    auto* listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(12);

    std::vector<QLabel*> titles;
    std::vector<charts::SparklineWidget*> adapterCharts;
    std::vector<ReadoutGrid*> readoutGrids;
    for (std::size_t index = 0; index < adapterNames.size(); ++index) {
        auto* title = new QLabel(list);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);
        listLayout->addWidget(title);

        auto* chart = createHistoryChart(std::nullopt, formatChartByteRate, list);
        chart->setMinimumHeight(kAdapterChartMinimumHeight);
        chart->setSeriesCount(2);
        chart->setSeriesLabel(0, "Receive");
        chart->setSeriesLabel(1, "Send");
        listLayout->addWidget(chart, 1);

        auto* readoutGrid = new ReadoutGrid(kReadoutColumns, list);
        listLayout->addWidget(readoutGrid);

        titles.push_back(title);
        adapterCharts.push_back(chart);
        readoutGrids.push_back(readoutGrid);
    }
    if (adapterNames.empty()) {
        listLayout->addStretch();
    }

    // setWidget() deletes the previous list together with its charts.
    m_scrollArea->setWidget(list);
    m_chartedNames = adapterNames;
    m_chartTitles = std::move(titles);
    m_charts = std::move(adapterCharts);
    m_readoutGrids = std::move(readoutGrids);
}

QString NetworkPerformancePage::summary() const
{
    return recordedCount() == 0 ? QString("--") : networkSummary(m_latest);
}

const std::vector<std::string>& NetworkPerformancePage::chartedAdapterNames() const
{
    return m_chartedNames;
}

charts::SparklineWidget* NetworkPerformancePage::adapterChart(std::size_t index) const
{
    assert(index < m_charts.size() && "adapter chart index out of range");
    return m_charts[index];
}

QString NetworkPerformancePage::adapterTitle(std::size_t index) const
{
    assert(index < m_chartTitles.size() && "adapter chart index out of range");
    return m_chartTitles[index]->text();
}

ReadoutGrid* NetworkPerformancePage::adapterReadoutGrid(std::size_t index) const
{
    assert(index < m_readoutGrids.size() && "adapter chart index out of range");
    return m_readoutGrids[index];
}

QString NetworkPerformancePage::placeholderText() const
{
    return m_placeholderLabel->text();
}

} // namespace sysmon::ui
