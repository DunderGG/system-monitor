#pragma once

#include <QWidget>

namespace sysmon::ui
{

/**
 * Network tab placeholder view (adapters, throughput, connectivity status).
 * Implemented in Phase 5.
 */
class NetworkView : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkView(QWidget* parent = nullptr);
    ~NetworkView() override = default;
};

} // namespace sysmon::ui

