#include "ui/network_view.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/view_metrics.h"

namespace sysmon::ui
{

NetworkView::NetworkView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kViewMargin, kViewMargin, kViewMargin, kViewMargin);

    auto* label = new QLabel("Network view placeholder (Phase 5)", this);
    QFont font = label->font();
    font.setPointSize(12);
    label->setFont(font);

    layout->addWidget(label);
    layout->addStretch();
}

} // namespace sysmon::ui
