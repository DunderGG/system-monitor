#pragma once

#include <optional>

#include <windows.h>
#include <netlistmgr.h>

#include "domain/network_profile.h"

namespace verdandi::platform
{

// Pure conversion from Network List Manager types to domain types.
// Internal to platform/windows: exposed in a header only so the mapping can be
// unit tested without calling the OS, by tests/platform/ only.
// Do not include from other modules or from tests/unit/.

/** Maps a network category; a value outside NLM_NETWORK_CATEGORY maps to std::nullopt. */
[[nodiscard]] std::optional<domain::NetworkCategory> toNetworkCategory(NLM_NETWORK_CATEGORY category) noexcept;

} // namespace verdandi::platform
