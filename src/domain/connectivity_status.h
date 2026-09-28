#pragma once

#include <optional>

namespace sysmon::domain
{

// Maps to NLM_CONNECTIVITY / GetNetworkConnectivityHint ConnectivityLevel.
// Unknown                 — connectivity could not be determined (no successful
//                           read yet, the query failed, or the OS reported an
//                           unknown/hidden level). Distinct from None.
// None                    — no usable network path.
// LocalAccess             — link-local or LAN reachable, no internet path.
// ConstrainedInternetAccess — internet reachable but captive portal or
//                             limited connectivity detected.
// InternetAccess          — full internet connectivity.
enum class ConnectivityLevel
{
    Unknown,
    None,
    LocalAccess,
    ConstrainedInternetAccess,
    InternetAccess,
};

// Snapshot of the system's current network connectivity state.
// Produced by GetNetworkConnectivityHint (Windows 10 2004+).
// isMetered reflects the ConnectionCost metered flag; the UI uses this to
// warn users before triggering network-heavy operations. std::nullopt means
// the cost could not be determined.
struct ConnectivityStatus
{
    ConnectivityLevel level{ConnectivityLevel::Unknown};
    std::optional<bool> isMetered;

    bool operator==(const ConnectivityStatus &) const = default;
};

} // namespace sysmon::domain
