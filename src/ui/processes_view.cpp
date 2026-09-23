#include "ui/processes_view.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace sysmon::ui
{

ProcessesView::ProcessesView(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    auto *label = new QLabel("Processes view placeholder (Phase 4)", this);
    QFont font = label->font();
    font.setPointSize(12);
    label->setFont(font);

    layout->addWidget(label);
    layout->addStretch();
}

} // namespace sysmon::ui
