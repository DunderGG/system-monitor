#include "ui/resource_card.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/dashboard_formatting.h"
#include "ui/text_tone.h"
#include "ui/view_metrics.h"

namespace verdandi::ui
{

namespace
{

constexpr int kSparklineHeight = 36;

TextTone healthTone(domain::HealthLevel level)
{
    switch (level) {
        case domain::HealthLevel::Healthy:
            return TextTone::Good;
        case domain::HealthLevel::Warning:
            return TextTone::Warning;
        case domain::HealthLevel::Critical:
            return TextTone::Critical;
        case domain::HealthLevel::Unknown:
        default:
            return TextTone::Dimmed;
    }
}

} // namespace

ResourceCard::ResourceCard(const QString& title, QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(180);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kCardMargins);
    layout->setSpacing(4);

    auto* headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(title, this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    m_statusLabel = new TonedLabel(TextTone::Dimmed, this);
    headerLayout->addWidget(m_statusLabel);
    layout->addLayout(headerLayout);

    // The value and detail can name things from the system, such as a volume.
    m_valueLabel = new QLabel("--", this);
    m_valueLabel->setTextFormat(Qt::PlainText);
    QFont valueFont = m_valueLabel->font();
    valueFont.setPointSize(18);
    m_valueLabel->setFont(valueFont);
    m_valueLabel->setWordWrap(true);
    layout->addWidget(m_valueLabel);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setTextFormat(Qt::PlainText);
    m_detailLabel->setWordWrap(true);
    layout->addWidget(m_detailLabel);

    // Keep content top-aligned and the sparkline slot at the bottom when a
    // grid row is taller than this card needs.
    layout->addStretch();

    // Reserved for the mini sparkline so the card layout does not shift when
    // the chart is added.
    m_sparklineSlot = new QWidget(this);
    m_sparklineSlot->setFixedHeight(kSparklineHeight);
    auto* sparklineLayout = new QVBoxLayout(m_sparklineSlot);
    sparklineLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_sparklineSlot);

    setStatus(domain::HealthLevel::Unknown);
}

void ResourceCard::setValue(const QString& value, const QString& detail)
{
    m_valueLabel->setText(value);
    m_detailLabel->setText(detail);
}

void ResourceCard::setStatus(std::optional<domain::HealthLevel> level)
{
    // Called on every snapshot, so skip it when nothing changed. The
    // constructor's setStatus(Unknown) always applies, because m_status starts
    // as std::nullopt.
    if (level == m_status) {
        return;
    }
    m_status = level;
    if (!level) {
        m_statusLabel->hide();
        return;
    }
    m_statusLabel->setText(healthLevelText(*level));
    m_statusLabel->setTone(healthTone(*level));
    // Unknown is dimmed like an inactive status, and in italics because it is not a state.
    QFont statusFont = m_statusLabel->font();
    statusFont.setItalic(*level == domain::HealthLevel::Unknown);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->show();
}

QString ResourceCard::title() const
{
    return m_titleLabel->text();
}

QString ResourceCard::valueText() const
{
    return m_valueLabel->text();
}

QString ResourceCard::detailText() const
{
    return m_detailLabel->text();
}

QString ResourceCard::statusText() const
{
    return m_status ? m_statusLabel->text() : QString{};
}

std::optional<domain::HealthLevel> ResourceCard::status() const
{
    return m_status;
}

void ResourceCard::setChart(QWidget* chart)
{
    m_sparklineSlot->layout()->addWidget(chart);
}

QWidget* ResourceCard::sparklineSlot() const
{
    return m_sparklineSlot;
}

TonedLabel* ResourceCard::statusLabel() const
{
    return m_statusLabel;
}

} // namespace verdandi::ui
