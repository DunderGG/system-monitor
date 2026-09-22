#include "ui/performance_view.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace sysmon::ui
{

PerformanceView::PerformanceView(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* label = new QLabel("Performance view placeholder (Phase 3)", this);
    QFont font = label->font();
    font.setPointSize(12);
    label->setFont(font);

    layout->addWidget(label);
    layout->addStretch();
}

} // namespace sysmon::ui

