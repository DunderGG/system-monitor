#pragma once

#include <optional>

#include <QFrame>
#include <QString>

#include "domain/health_status.h"

class QLabel;

namespace sysmon::ui
{

/**
 * Dashboard card for one resource: a title, the current value, a secondary
 * detail line, a colour-coded health status label, and a reserved slot for a
 * mini sparkline chart.
 *
 * Not thread-safe; use only on the UI thread.
 */
class ResourceCard : public QFrame
{
    Q_OBJECT

public:
    explicit ResourceCard(const QString &title, QWidget *parent = nullptr);
    ~ResourceCard() override = default;

    /** Sets the main value (e.g. "42.5%") and the secondary detail line. */
    void setValue(const QString &value, const QString &detail = {});

    /** Shows the health status label for level, or hides it for std::nullopt. */
    void setStatus(std::optional<domain::HealthLevel> level);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString valueText() const;
    [[nodiscard]] QString detailText() const;
    [[nodiscard]] QString statusText() const;

    /** Returns the displayed health level, or std::nullopt when the status label is hidden. */
    [[nodiscard]] std::optional<domain::HealthLevel> status() const;

    /** Places chart in the sparkline slot; the card takes ownership through Qt parenting. */
    void setChart(QWidget *chart);

    /** Container for the card's mini sparkline chart (see setChart). */
    [[nodiscard]] QWidget *sparklineSlot() const;

private:
    QLabel *m_titleLabel{nullptr};
    QLabel *m_valueLabel{nullptr};
    QLabel *m_detailLabel{nullptr};
    QLabel *m_statusLabel{nullptr};
    QWidget *m_sparklineSlot{nullptr};
    std::optional<domain::HealthLevel> m_status;
};

} // namespace sysmon::ui
