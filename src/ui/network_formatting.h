#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QString>

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
 * off: a hardware adapter, or one bound to IP, whatever its operational
 * status. The adapters left out are hidden virtual adapters that are not bound
 * to IP, such as the WAN Miniports and the tunnel pseudo-interfaces, which
 * cannot carry IP traffic.
 */
[[nodiscard]] bool isListedByDefault(const domain::NetworkSample& adapter);

/**
 * Returns the adapters the Network tab lists: every adapter when showAll is
 * set, otherwise those isListedByDefault() accepts. Adapters that are Up come
 * first, then hardware adapters, each group ordered by display name and then
 * by interface ID, so the order does not depend on the order of networks.
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

/**
 * Network tab, for one adapter: type, link speed, IPv4 and IPv6 addresses,
 * and DNS servers. The link speed is "Not connected" unless the adapter is Up,
 * because Windows keeps reporting a speed for a disconnected adapter, and
 * "N/A" when an adapter that is Up reports none. Every value is "N/A" when
 * adapter is std::nullopt (the last read failed).
 */
[[nodiscard]] std::vector<Readout> adapterDetailReadouts(const std::optional<domain::NetworkSample>& adapter);

} // namespace sysmon::ui
