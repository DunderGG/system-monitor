#include "ui/adapter_card.h"

#include <optional>

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/dashboard_formatting.h"
#include "ui/network_formatting.h"
#include "ui/performance_formatting.h"
#include "ui/readout_grid.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

constexpr int kReadoutColumns = 6;

// The Up colour is the dashboard's Healthy green; other states are dimmed.
// Both stay readable on light and dark window backgrounds.
constexpr const char* kUpStyleSheet = "color: #2e8b57; font-weight: bold;";
constexpr const char* kNotUpStyleSheet = "color: palette(mid);";

} // namespace

AdapterCard::AdapterCard(QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(kItemSpacing);

    auto* headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(m_titleLabel, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(kNotUpStyleSheet);
    headerLayout->addWidget(m_statusLabel);
    layout->addLayout(headerLayout);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setStyleSheet("color: palette(mid);");
    m_descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->hide();
    layout->addWidget(m_descriptionLabel);

    m_readoutGrid = new ReadoutGrid(kReadoutColumns, this);
    m_readoutGrid->setReadouts(placeholderReadouts(adapterDetailReadouts(std::nullopt)));
    layout->addWidget(m_readoutGrid);
}

void AdapterCard::setAdapter(const domain::NetworkSample& adapter)
{
    const QString title = adapterDisplayName(adapter);
    m_titleLabel->setText(title);

    const QString description = QString::fromStdString(adapter.description);
    const bool showsDescription = !description.isEmpty() && description != title;
    m_descriptionLabel->setText(showsDescription ? description : QString{});
    m_descriptionLabel->setVisible(showsDescription);

    setStatus(operationalStatusText(adapter.operationalStatus),
              adapter.operationalStatus == domain::OperationalStatus::Up);
    m_readoutGrid->setReadouts(adapterDetailReadouts(adapter));
}

void AdapterCard::setUnavailable()
{
    setStatus(kNotAvailable, false);
    m_readoutGrid->setReadouts(adapterDetailReadouts(std::nullopt));
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

void AdapterCard::setStatus(const QString& text, bool isUp)
{
    m_statusLabel->setText(text);
    // setStyleSheet() re-polishes the label, so only call it on a change.
    if (isUp != m_isUp) {
        m_isUp = isUp;
        m_statusLabel->setStyleSheet(isUp ? kUpStyleSheet : kNotUpStyleSheet);
    }
}

} // namespace sysmon::ui
