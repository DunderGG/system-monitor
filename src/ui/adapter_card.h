#pragma once

#include <QFrame>
#include <QString>

#include "domain/network_sample.h"

class QLabel;

namespace sysmon::ui
{

class ReadoutGrid;

/**
 * Network tab card for one network adapter: its display name (the alias)
 * with its operational status, the device description below it when that
 * differs from the name, and readouts for the type, link speed, addresses,
 * and DNS servers (see adapterDetailReadouts()).
 *
 * Not thread-safe; use only on the UI thread.
 */
class AdapterCard : public QFrame
{
    Q_OBJECT

public:
    explicit AdapterCard(QWidget* parent = nullptr);
    ~AdapterCard() override = default;

    /** Shows the adapter's latest sample. */
    void setAdapter(const domain::NetworkSample& adapter);

    /** Shows the status and readouts as "N/A" because the last read failed. Keeps the name and description. */
    void setUnavailable();

    [[nodiscard]] QString title() const;
    /** The device description shown below the name; empty when it is hidden. */
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] ReadoutGrid* readoutGrid() const;

private:
    void setStatus(const QString& text, bool isUp);

    QLabel* m_titleLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_descriptionLabel{nullptr};
    ReadoutGrid* m_readoutGrid{nullptr};
    bool m_isUp{false};
};

} // namespace sysmon::ui
