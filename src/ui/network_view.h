#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "domain/network_sample.h"
#include "domain/system_snapshot.h"
#include "ui/adapter_throughput_history.h"

class QCheckBox;
class QLabel;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;

namespace sysmon::ui
{

class AdapterCard;
class ReadoutGrid;

/**
 * Network tab: the system's connectivity (see connectivityReadouts()) above
 * one card per network adapter in a scrolling list (see listedAdapters() for
 * which adapters, and in what order). "Show all adapters" also lists the
 * hidden virtual adapters that are not bound to IP.
 *
 * Cards are keyed by interface ID, so an adapter keeps its card when it is
 * renamed or moves in the list. When a read fails, the cards of the last
 * successful read stay, with their details shown as "N/A" and a gap in their
 * charts.
 *
 * Each card charts the adapter's throughput over the last kHistoryCapacity
 * snapshots. Like the Performance tab, the view records every snapshot, so the
 * charts have no holes, and updates its widgets only while it is visible:
 * updateSnapshot() always records, and showEvent() catches up.
 *
 * Not thread-safe; use only on the UI thread.
 */
class NetworkView : public QWidget
{
    Q_OBJECT

public:
    /** Samples of history per chart: one minute at the default 1 Hz sampling rate, as on the dashboard. */
    static constexpr std::size_t kHistoryCapacity = 60;

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

    [[nodiscard]] ReadoutGrid* connectivityReadoutGrid() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void arrangeCards(const std::vector<uint64_t>& interfaceIds);

    ReadoutGrid* m_connectivityGrid{nullptr};
    QCheckBox* m_showAllCheckBox{nullptr};
    QLabel* m_placeholderLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QVBoxLayout* m_cardLayout{nullptr};
    std::vector<uint64_t> m_listedIds;
    std::vector<AdapterCard*> m_cards;

    std::uint64_t m_recordedCount{0};
    domain::ConnectivityStatus m_connectivity;
    std::optional<std::vector<domain::NetworkSample>> m_latest;
    AdapterThroughputHistory m_history{kHistoryCapacity};
};

} // namespace sysmon::ui
