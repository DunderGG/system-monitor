#include "ui/adapter_card.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/performance_formatting.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

AdapterCard::AdapterCard(QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(kItemSpacing);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
}

void AdapterCard::setAdapter(const domain::NetworkSample& adapter)
{
    m_titleLabel->setText(adapterDisplayName(adapter));
}

QString AdapterCard::title() const
{
    return m_titleLabel->text();
}

} // namespace sysmon::ui
