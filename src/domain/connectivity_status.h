#pragma once

namespace sysmon::domain
{

// Maps to NLM_CONNECTIVITY / GetNetworkConnectivityHint ConnectivityLevel.
// None                    — no usable network path.
// LocalAccess             — link-local or LAN reachable, no internet path.
// ConstrainedInternetAccess — internet reachable but captive portal or
//                             limited connectivity detected.
// InternetAccess          — full internet connectivity.
enum class ConnectivityLevel
{
    None,
    LocalAccess,
    ConstrainedInternetAccess,
    InternetAccess,
};

// Snapshot of the system's current network connectivity state.
// Produced by GetNetworkConnectivityHint (Windows 10 2004+).
// isMetered reflects the ConnectionCost metered flag; the UI uses this to
// warn users before triggering network-heavy operations.
struct ConnectivityStatus
{
    ConnectivityLevel level{ConnectivityLevel::None};
    bool              isMetered{false};
};

} // namespace sysmon::domain

