#pragma once

#include <optional>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include "domain/connectivity_status.h"

namespace sysmon::platform
{

// Pure conversions from GetNetworkConnectivityHint types to domain types.
// Internal to platform/windows: exposed in a header only so the mapping can be
// unit tested without calling the OS, by tests/platform/ only.
// Do not include from other modules or from tests/unit/.

/** Maps a Windows connectivity level; Unknown and Hidden map to ConnectivityLevel::Unknown. */
[[nodiscard]] domain::ConnectivityLevel toConnectivityLevel(NL_NETWORK_CONNECTIVITY_LEVEL_HINT level) noexcept;

/**
 * Maps a Windows connectivity cost to a metered flag. Fixed (capped plan) and
 * Variable (pay-per-byte) are metered; Unknown maps to std::nullopt.
 */
[[nodiscard]] std::optional<bool> toIsMetered(NL_NETWORK_CONNECTIVITY_COST_HINT cost) noexcept;

[[nodiscard]] domain::ConnectivityStatus toConnectivityStatus(const NL_NETWORK_CONNECTIVITY_HINT &hint) noexcept;

} // namespace sysmon::platform
