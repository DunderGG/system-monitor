#include "ui/resource_card.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/dashboard_formatting.h"

namespace sysmon::ui
{

namespace
{

constexpr int kSparklineHeight = 36;

// Text colours chosen to stay readable on both light and dark window backgrounds.
QString statusStyleSheet(domain::HealthLevel level)
{
    switch (level) {
        case domain::HealthLevel::Healthy:
            return "color: #2e8b57; font-weight: bold;";
        case domain::HealthLevel::Warning:
            return "color: #d98200; font-weight: bold;";
        case domain::HealthLevel::Critical:
            return "color: #d32f2f; font-weight: bold;";
        case domain::HealthLevel::Unknown:
        default:
            return "color: palette(mid); font-style: italic;";
    }
}

} // namespace

ResourceCard::ResourceCard(const QString &title, QWidget *parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(180);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(title, this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    m_statusLabel = new QLabel(this);
    headerLayout->addWidget(m_statusLabel);
    layout->addLayout(headerLayout);

    m_valueLabel = new QLabel("--", this);
    QFont valueFont = m_valueLabel->font();
    valueFont.setPointSize(18);
    m_valueLabel->setFont(valueFont);
    m_valueLabel->setWordWrap(true);
    layout->addWidget(m_valueLabel);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    layout->addWidget(m_detailLabel);

    // Keep content top-aligned and the sparkline slot at the bottom when a
    // grid row is taller than this card needs.
    layout->addStretch();

    // Reserved for the mini sparkline so the card layout does not shift when
    // the chart is added.
    m_sparklineSlot = new QWidget(this);
    m_sparklineSlot->setFixedHeight(kSparklineHeight);
    auto *sparklineLayout = new QVBoxLayout(m_sparklineSlot);
    sparklineLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_sparklineSlot);

    setStatus(domain::HealthLevel::Unknown);
}

void ResourceCard::setValue(const QString &value, const QString &detail)
{
    m_valueLabel->setText(value);
    m_detailLabel->setText(detail);
}

void ResourceCard::setStatus(std::optional<domain::HealthLevel> level)
{
    m_status = level;
    if (!level) {
        m_statusLabel->hide();
        return;
    }
    m_statusLabel->setText(healthLevelText(*level));
    m_statusLabel->setStyleSheet(statusStyleSheet(*level));
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

QWidget *ResourceCard::sparklineSlot() const
{
    return m_sparklineSlot;
}

} // namespace sysmon::ui
