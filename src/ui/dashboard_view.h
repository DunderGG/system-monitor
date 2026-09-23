#pragma once

#include <QWidget>

#include "domain/system_snapshot.h"

class QLabel;

namespace sysmon::ui
{

/**
 * Dashboard tab view displaying high-level system overview cards and metrics.
 */
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardView(QWidget *parent = nullptr);
    ~DashboardView() override = default;

    /** Updates the displayed metric values from a newly received system snapshot. */
    void updateSnapshot(const domain::SystemSnapshot &snapshot);

    /** Returns the current text displayed on the CPU label. */
    [[nodiscard]] QString cpuText() const;

    /** Returns the current text displayed on the Memory label. */
    [[nodiscard]] QString memoryText() const;

private:
    QLabel *m_cpuLabel{nullptr};
    QLabel *m_memoryLabel{nullptr};
};

} // namespace sysmon::ui
