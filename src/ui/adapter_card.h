#pragma once

#include <QFrame>
#include <QString>

#include "domain/network_sample.h"

class QLabel;

namespace sysmon::ui
{

/**
 * Network tab card for one network adapter, headed by its display name.
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

    [[nodiscard]] QString title() const;

private:
    QLabel* m_titleLabel{nullptr};
};

} // namespace sysmon::ui
