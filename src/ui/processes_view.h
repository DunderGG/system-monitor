#pragma once

#include <QWidget>

namespace sysmon::ui
{

/**
 * Processes tab placeholder view (process table, tree, CPU/memory introspection).
 * Implemented in Phase 4.
 */
class ProcessesView : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessesView(QWidget *parent = nullptr);
    ~ProcessesView() override = default;
};

} // namespace sysmon::ui
