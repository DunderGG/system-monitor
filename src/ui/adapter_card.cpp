#include "ui/adapter_card.h"

#include <optional>

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/charts/sparkline_widget.h"
#include "ui/dashboard_formatting.h"
#include "ui/network_formatting.h"
#include "ui/performance_formatting.h"
#include "ui/readout_grid.h"
#include "ui/text_tone.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

constexpr int kReadoutColumns = 6;
// Room for the legend line, a short plot, and the X-axis labels.
constexpr int kChartHeight = 110;

} // namespace

AdapterCard::AdapterCard(std::size_t chartCapacity, QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kCardMargins);
    layout->setSpacing(kItemSpacing);

    // The alias and description are chosen by drivers and VPN clients, so
    // every label shows plain text, never rich text.
    auto* headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    m_titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_titleLabel, 1);

    // Up is the dashboard's Healthy green, in bold; every other status is dimmed.
    m_statusLabel = new TonedLabel(TextTone::Dimmed, this);
    m_statusLabel->setTextFormat(Qt::PlainText);
    headerLayout->addWidget(m_statusLabel);
    layout->addLayout(headerLayout);

    m_descriptionLabel = new TonedLabel(TextTone::Dimmed, this);
    m_descriptionLabel->setTextFormat(Qt::PlainText);
    m_descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->hide();
    layout->addWidget(m_descriptionLabel);

    m_readoutGrid = new ReadoutGrid(kReadoutColumns, this);
    m_readoutGrid->setReadouts(placeholderReadouts(adapterDetailReadouts(std::nullopt, std::nullopt)));
    layout->addWidget(m_readoutGrid);

    // Set up like the Performance tab's network charts, so the two read alike.
    m_chart = new charts::SparklineWidget(this);
    m_chart->setCapacity(chartCapacity);
    m_chart->setAutoRange();
    m_chart->setSeriesCount(2);
    m_chart->setSeriesLabel(0, "Receive");
    m_chart->setSeriesLabel(1, "Send");
    m_chart->setValueFormatter(formatChartByteRate);
    m_chart->setAxisLabelTemplates(chartByteRateAxisTemplates());
    m_chart->setGridVisible(true);
    m_chart->setAxisLabelsVisible(true);
    m_chart->setAnnotationsVisible(true);
    m_chart->setFixedHeight(kChartHeight);
    layout->addWidget(m_chart);
}

void AdapterCard::setAdapter(const domain::NetworkSample& adapter,
                             const std::optional<std::vector<domain::NetworkProfile>>& networkProfiles)
{
    const QString title = adapterDisplayName(adapter);
    m_titleLabel->setText(title);

    const QString description = QString::fromStdString(adapter.description);
    const bool showsDescription = !description.isEmpty() && description != title;
    m_descriptionLabel->setText(showsDescription ? description : QString{});
    m_descriptionLabel->setVisible(showsDescription);

    setStatus(operationalStatusText(adapter.operationalStatus),
              adapter.operationalStatus == domain::OperationalStatus::Up);
    m_readoutGrid->setReadouts(adapterDetailReadouts(adapter, networkProfiles));
}

void AdapterCard::setUnavailable()
{
    setStatus(kNotAvailable, false);
    m_readoutGrid->setReadouts(adapterDetailReadouts(std::nullopt, std::nullopt));
}

void AdapterCard::setThroughput(const AdapterThroughputHistory::Adapter* history, std::uint64_t sampleIndex)
{
    if (history == nullptr) {
        m_chart->setSeriesSamples(0, {});
        m_chart->setSeriesSamples(1, {});
    } else {
        m_chart->setSeriesSamples(0, history->receive.samples());
        m_chart->setSeriesSamples(1, history->send.samples());
    }
    m_chart->setSampleIndex(sampleIndex);
}

QString AdapterCard::title() const
{
    return m_titleLabel->text();
}

QString AdapterCard::description() const
{
    return m_descriptionLabel->text();
}

QString AdapterCard::statusText() const
{
    return m_statusLabel->text();
}

ReadoutGrid* AdapterCard::readoutGrid() const
{
    return m_readoutGrid;
}

charts::SparklineWidget* AdapterCard::chart() const
{
    return m_chart;
}

TonedLabel* AdapterCard::statusLabel() const
{
    return m_statusLabel;
}

TonedLabel* AdapterCard::descriptionLabel() const
{
    return m_descriptionLabel;
}

void AdapterCard::setStatus(const QString& text, bool isUp)
{
    m_statusLabel->setText(text);
    m_statusLabel->setTone(isUp ? TextTone::Good : TextTone::Dimmed);
}

} // namespace sysmon::ui
