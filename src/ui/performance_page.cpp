#include "ui/performance_page.h"

#include <algorithm>
#include <utility>

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace sysmon::ui
{

bool hasAnyValue(const SampleHistory& history)
{
    return std::ranges::any_of(history, [](const std::optional<float>& sample) { return sample.has_value(); });
}

PerformancePage::PerformancePage(const QString& title, QWidget* parent) : QWidget(parent), m_title(title)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* heading = new QLabel(title, this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(16);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setSpacing(8);
    layout->addLayout(m_contentLayout, 1);
}

void PerformancePage::recordSnapshot(const domain::SystemSnapshot& snapshot)
{
    ++m_recordedCount;
    recordSamples(snapshot);
}

QString PerformancePage::title() const
{
    return m_title;
}

std::uint64_t PerformancePage::recordedCount() const
{
    return m_recordedCount;
}

QVBoxLayout* PerformancePage::contentLayout() const
{
    return m_contentLayout;
}

charts::SparklineWidget* PerformancePage::createHistoryChart(std::optional<charts::YRange> fixedRange,
                                                             charts::SparklineWidget::ValueFormatter formatter,
                                                             QWidget* parent)
{
    auto* chart = new charts::SparklineWidget(parent);
    chart->setCapacity(kHistoryCapacity);
    if (fixedRange) {
        chart->setFixedRange(*fixedRange);
    } else {
        chart->setAutoRange();
    }
    chart->setValueFormatter(std::move(formatter));
    chart->setGridVisible(true);
    chart->setAxisLabelsVisible(true);
    chart->setAnnotationsVisible(true);
    chart->setMinimumHeight(120);
    return chart;
}

void PerformancePage::showHistory(charts::SparklineWidget* chart, std::size_t series,
                                  const SampleHistory& history) const
{
    chart->setSeriesSamples(series, history.samples());
    chart->setSampleIndex(m_recordedCount);
}

} // namespace sysmon::ui
