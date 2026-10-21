#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "ui/adapter_throughput_history.h"
#include "ui/sample_history.h"

class QCheckBox;
class QLabel;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;

namespace verdandi::ui
{

class AdapterCard;
class ReadoutGrid;

/**
 * Network tab: the system's connectivity and the connected networks (see
 * connectivityReadouts()) above one card per network adapter in a scrolling
 * list (see listedAdapters() for which adapters, and in what order). "Show all
 * adapters" also lists the adapters that Windows hides and the virtual
 * adapters that are not bound to IP, after the others and under a "Hidden
 * adapters" heading.
 *
 * Cards are keyed by interface ID, so an adapter keeps its card when it is
 * renamed or moves in the list. When a read fails, the cards of the last
 * successful read stay, with their details shown as "N/A" and a gap in their
 * charts.
 *
 * Each card charts the adapter's throughput over the last kHistoryCapacity
 * snapshots, and its X-axis label tells the time they cover at the sample
 * interval (DC-61). Like the Performance tab, the view records every
 * snapshot, so the charts have no holes, and updates its widgets only while
 * it is visible: updateSnapshot() always records, and showEvent() catches up.
 *
 * Not thread-safe; use only on the UI thread.
 */
class NetworkView : public QWidget
{
    Q_OBJECT

public:
    /** Samples of history per chart: one minute at the default interval (kOneMinuteOfSamples), as on the dashboard. */
    static constexpr std::size_t kHistoryCapacity = kOneMinuteOfSamples;

    explicit NetworkView(QWidget* parent = nullptr);
    ~NetworkView() override = default;

    /** Records the snapshot, and updates the widgets if the view is visible. */
    void updateSnapshot(const domain::SystemSnapshot& snapshot);

    /**
     * Sets whether the adapters that Windows hides, and virtual adapters that
     * are not bound to IP, are listed too. Off by default.
     */
    void setShowAllAdapters(bool showAll);
    [[nodiscard]] bool showsAllAdapters() const;

    /**
     * Sets the time between samples, the refresh interval, from which the
     * cards' charts tell the time their samples cover. One second until set.
     */
    void setSampleInterval(std::chrono::milliseconds interval);
    [[nodiscard]] std::chrono::milliseconds sampleInterval() const;

    /** Sets the units of the cards' throughput charts. Binary bytes per second until set. */
    void setDisplayUnits(domain::DisplayUnits units);
    [[nodiscard]] domain::DisplayUnits displayUnits() const;

    /** Interface IDs of the listed adapters, in display order, as of the last update of the widgets. */
    [[nodiscard]] const std::vector<uint64_t>& listedInterfaceIds() const;
    [[nodiscard]] AdapterCard* adapterCard(std::size_t index) const;

    /** Text shown in place of the cards when there are none, e.g. "No network adapters". */
    [[nodiscard]] QString placeholderText() const;

    /**
     * The number of cards above the "Hidden adapters" heading, which precedes
     * the cards that only "Show all adapters" lists; std::nullopt when the
     * heading is not shown.
     */
    [[nodiscard]] std::optional<std::size_t> hiddenAdaptersHeadingPosition() const;

    [[nodiscard]] ReadoutGrid* connectivityReadoutGrid() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    // Lists a card per interface ID, in order, with the "Hidden adapters"
    // heading before the card at hiddenStart, if there is one.
    void arrangeCards(const std::vector<uint64_t>& interfaceIds, std::size_t hiddenStart);

    ReadoutGrid* m_connectivityGrid{nullptr};
    QCheckBox* m_showAllCheckBox{nullptr};
    QLabel* m_placeholderLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
    QVBoxLayout* m_cardLayout{nullptr};
    QLabel* m_hiddenHeading{nullptr};
    std::vector<uint64_t> m_listedIds;
    std::size_t m_hiddenStart{0};
    std::vector<AdapterCard*> m_cards;
    std::chrono::milliseconds m_sampleInterval{domain::kDefaultRefreshInterval};
    domain::DisplayUnits m_displayUnits{};

    std::uint64_t m_recordedCount{0};
    domain::ConnectivityStatus m_connectivity;
    std::optional<std::vector<domain::NetworkProfile>> m_networkProfiles;
    std::optional<std::vector<domain::NetworkSample>> m_latest;
    AdapterThroughputHistory m_history{kHistoryCapacity};
};

} // namespace verdandi::ui
