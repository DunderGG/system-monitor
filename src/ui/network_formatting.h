#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <QString>
#include <QStringView>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "ui/performance_readouts.h"

namespace sysmon::ui
{

// Pure presentation helpers for the Network tab, and the adapter texts the
// Performance tab's network page shares with it. Like the dashboard helpers in
// dashboard_formatting.h and the Performance tab's in performance_formatting.h,
// they take domain values and return display text or selections, so they can
// be unit tested without widgets. Missing data is "N/A", never a zero.

/**
 * Returns true if the Network tab lists adapter while "Show all adapters" is
 * off: an adapter that Windows does not hide (NetworkSample::isHidden) and
 * that is hardware or bound to IP, whatever its operational status. The
 * adapters left out are those Network Connections leaves out too, such as the
 * WAN Miniports and the Wi-Fi Direct virtual adapters, and virtual adapters
 * that are not bound to IP, such as the tunnel pseudo-interfaces, which cannot
 * carry IP traffic.
 */
[[nodiscard]] bool isListedByDefault(const domain::NetworkSample& adapter);

/**
 * Compares two names in natural order: case-insensitively, with each run of
 * ASCII digits compared as a number, so "Connection* 2" comes before
 * "Connection* 10". Case folding is Unicode's and does not depend on the
 * locale, so neither does the order.
 */
[[nodiscard]] std::weak_ordering compareNaturally(QStringView left, QStringView right);

/**
 * The order in which both tabs list adapters: those isListedByDefault()
 * accepts first, then hardware adapters, each group by display name in natural
 * order (compareNaturally()), and then by interface ID. It does not depend on
 * the operational status, so an adapter keeps its place when it goes up or
 * down, nor on the order in which Windows reports the adapters.
 */
[[nodiscard]] bool isListedBefore(const domain::NetworkSample& left, const domain::NetworkSample& right);

/**
 * Returns the adapters the Network tab lists, in isListedBefore() order: every
 * adapter when showAll is set, otherwise those isListedByDefault() accepts.
 * With showAll, the adapters it adds therefore come after the others.
 */
[[nodiscard]] std::vector<const domain::NetworkSample*>
listedAdapters(const std::vector<domain::NetworkSample>& networks, bool showAll);

/** Returns the display name of an operational status, e.g. "Up" or "Not present". */
[[nodiscard]] QString operationalStatusText(domain::OperationalStatus status);

/**
 * Describes the kind of adapter, e.g. "Ethernet", "Wi-Fi (virtual)", or
 * "Tunnel". Ethernet, Wi-Fi, Bluetooth, and mobile broadband adapters that are
 * not hardware are marked virtual. An adapter of type Other is "Virtual"
 * unless it is hardware.
 */
[[nodiscard]] QString adapterTypeText(const domain::NetworkSample& adapter);

/** Joins addresses one per line, or returns "None" when there are none. */
[[nodiscard]] QString formatAddressList(const std::vector<std::string>& addresses);

/** Returns the adapter's IPv4 addresses with formatAddressList(). */
[[nodiscard]] QString ipv4AddressText(const domain::NetworkSample& adapter);

/** Returns the adapter's IPv6 addresses with formatAddressList(). */
[[nodiscard]] QString ipv6AddressText(const domain::NetworkSample& adapter);

/** Returns the display name of a network category: "Public", "Private", or "Domain". */
[[nodiscard]] QString networkCategoryText(domain::NetworkCategory category);

/** Names a connected network with its category, e.g. "Home-Wi-Fi (Private)", or just its name if the category is
 * unknown. */
[[nodiscard]] QString networkProfileText(const domain::NetworkProfile& profile);

/**
 * Returns the networks the adapter with interfaceId is connected to, one per
 * line with networkProfileText(); "None" when it is connected to none, and
 * "N/A" when profiles is std::nullopt (the networks could not be read).
 */
[[nodiscard]] QString adapterNetworkText(uint64_t interfaceId,
                                         const std::optional<std::vector<domain::NetworkProfile>>& profiles);

/**
 * Network tab, for one adapter: the network it is connected to (see
 * adapterNetworkText()), type, link speed, IPv4 and IPv6 addresses, and DNS
 * servers. The link speed, the addresses and the DNS servers are "Not
 * connected" unless the adapter is Up, because Windows keeps reporting a
 * speed, leftover addresses and DNS servers for a disconnected adapter. The
 * link speed is "N/A" when an adapter that is Up reports none. Every value is
 * "N/A" when adapter is std::nullopt (the last read failed).
 */
[[nodiscard]] std::vector<Readout>
adapterDetailReadouts(const std::optional<domain::NetworkSample>& adapter,
                      const std::optional<std::vector<domain::NetworkProfile>>& profiles);

/**
 * Network tab, for the system's connectivity: the level, the cost ("Metered"
 * or "Unmetered"), whether the connection is roaming ("Yes" or "No"), and the
 * connected networks, one per line with networkProfileText(). The first three
 * read "Unknown" when Windows could not say, never offline, unmetered, or not
 * roaming. The networks are "None" when there are none and "N/A" when
 * profiles is std::nullopt.
 */
[[nodiscard]] std::vector<Readout>
connectivityReadouts(const domain::ConnectivityStatus& status,
                     const std::optional<std::vector<domain::NetworkProfile>>& profiles);

} // namespace sysmon::ui
