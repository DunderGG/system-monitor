#pragma once

#include <QWidget>

namespace sysmon::ui
{

/**
 * Performance tab placeholder view (detailed rolling sparklines and charts).
 * Implemented in Phase 3.
 */
class PerformanceView : public QWidget
{
    Q_OBJECT

public:
    explicit PerformanceView(QWidget* parent = nullptr);
    ~PerformanceView() override = default;
};

} // namespace sysmon::ui

