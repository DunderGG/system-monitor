#pragma once

#include <vector>

#include "domain/network_sample.h"

namespace sysmon::ui
{

// Pure presentation helpers for the Network tab. Like the dashboard helpers in
// dashboard_formatting.h and the Performance tab's in performance_formatting.h,
// they take domain values and return display text or selections, so they can
// be unit tested without widgets.

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

} // namespace sysmon::ui
