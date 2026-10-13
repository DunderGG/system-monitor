#include "ui/performance_page.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

constexpr int kHistoryChartMinimumHeight = 120;

} // namespace

bool hasAnyValue(const SampleHistory& history)
{
    return std::ranges::any_of(history, [](const std::optional<float>& sample) { return sample.has_value(); });
}

PerformancePage::PerformancePage(const QString& title, QWidget* parent) : QWidget(parent), m_title(title)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kItemSpacing);

    auto* heading = new QLabel(title, this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(kTitlePointSize);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setSpacing(kItemSpacing);
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

void PerformancePage::setVisibleWindow(std::size_t samples)
{
    assert(samples > 0 && samples <= kHistoryCapacity && "visible window must fit in the history");
    m_visibleWindow = samples;
}

std::size_t PerformancePage::visibleWindow() const
{
    return m_visibleWindow;
}

QVBoxLayout* PerformancePage::contentLayout() const
{
    return m_contentLayout;
}

charts::SparklineWidget*
PerformancePage::createHistoryChart(std::optional<charts::YRange> fixedRange,
                                    charts::SparklineWidget::ValueFormatter axisFormatter,
                                    charts::SparklineWidget::ValueFormatter annotationFormatter, QWidget* parent)
{
    auto* chart = new charts::SparklineWidget(parent);
    chart->setCapacity(kDefaultVisibleWindow);
    if (fixedRange) {
        chart->setFixedRange(*fixedRange);
    } else {
        chart->setAutoRange();
    }
    chart->setValueFormatter(std::move(axisFormatter));
    chart->setAnnotationFormatter(std::move(annotationFormatter));
    chart->setGridVisible(true);
    chart->setAxisLabelsVisible(true);
    chart->setAnnotationsVisible(true);
    chart->setMinimumHeight(kHistoryChartMinimumHeight);
    return chart;
}

void PerformancePage::showHistory(charts::SparklineWidget* chart, std::size_t series,
                                  const SampleHistory& history) const
{
    if (chart->capacity() != m_visibleWindow) {
        chart->setCapacity(m_visibleWindow);
    }
    const auto samples = history.samples();
    chart->setSeriesSamples(series, samples.last(std::min(samples.size(), m_visibleWindow)));
    chart->setSampleIndex(m_recordedCount);
}

} // namespace sysmon::ui
