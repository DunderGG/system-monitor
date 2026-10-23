#pragma once

#include <optional>

namespace verdandi::domain
{

/**
 * Maps to NLM_CONNECTIVITY / GetNetworkConnectivityHint ConnectivityLevel.
 * Unknown                 — connectivity could not be determined (no successful
 *                           read yet, the query failed, or the OS reported an
 *                           unknown/hidden level). Distinct from None.
 * None                    — no usable network path.
 * LocalAccess             — link-local or LAN reachable, no internet path.
 * ConstrainedInternetAccess — internet reachable but captive portal or
 *                             limited connectivity detected.
 * InternetAccess          — full internet connectivity.
 */
enum class ConnectivityLevel
{
    Unknown,
    None,
    LocalAccess,
    ConstrainedInternetAccess,
    InternetAccess,
};

/**
 * Snapshot of the system's current network connectivity state.
 * Produced by GetNetworkConnectivityHint (Windows 10 2004+).
 * isMetered reflects the ConnectionCost metered flag. std::nullopt means the
 * cost could not be determined.
 * isRoaming is true when the connection is roaming outside its provider's
 * home network, as mobile broadband can. Windows reports it as part of the
 * connection cost, so it is std::nullopt whenever isMetered is.
 */
struct ConnectivityStatus
{
    ConnectivityLevel level{ConnectivityLevel::Unknown};
    std::optional<bool> isMetered{};
    std::optional<bool> isRoaming{};

    bool operator==(const ConnectivityStatus&) const = default;
};

} // namespace verdandi::domain
