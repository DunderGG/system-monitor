#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "domain/network_sample.h"
#include "domain/system_snapshot.h"

class QCheckBox;
class QLabel;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;

namespace sysmon::ui
{

class AdapterCard;

/**
 * Network tab: one card per network adapter in a scrolling list (see
 * listedAdapters() for which adapters, and in what order). "Show all
 * adapters" also lists the hidden virtual adapters that are not bound to IP.
 *
 * Cards are keyed by interface ID, so an adapter keeps its card when it is
 * renamed or moves in the list. When a read fails, the cards of the last
 * successful read stay.
 *
 * Like the Performance tab, the view records every snapshot and updates its
 * widgets only while it is visible: updateSnapshot() always stores the
 * snapshot, and showEvent() catches up.
 *
 * Not thread-safe; use only on the UI thread.
 */
class NetworkView : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkView(QWidget* parent = nullptr);
    ~NetworkView() override = default;

    /** Records the snapshot, and updates the widgets if the view is visible. */
    void updateSnapshot(const domain::SystemSnapshot& snapshot);

    /** Sets whether hidden virtual adapters that are not bound to IP are listed too. Off by default. */
    void setShowAllAdapters(bool showAll);
    [[nodiscard]] bool showsAllAdapters() const;

    /** Interface IDs of the listed adapters, in display order, as of the last update of the widgets. */
    [[nodiscard]] const std::vector<uint64_t>& listedInterfaceIds() const;
    [[nodiscard]] AdapterCard* adapterCard(std::size_t index) const;

    /** Text shown in place of the cards when there are none, e.g. "No network adapters". */
    [[nodiscard]] QString placeholderText() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void arrangeCards(const std::vector<uint64_t>& interfaceIds);

    QCheckBox* m_showAllCheckBox{nullptr};
    QLabel* m_placeholderLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QVBoxLayout* m_cardLayout{nullptr};
    std::vector<uint64_t> m_listedIds;
    std::vector<AdapterCard*> m_cards;

    bool m_hasSnapshot{false};
    std::optional<std::vector<domain::NetworkSample>> m_latest;
};

} // namespace sysmon::ui
