#pragma once

#include <optional>
#include <vector>

#include <windows.h>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

// Internal to platform/windows: exposes Windows types. Do not include from
// other modules or from tests/unit/.

/**
 * Reads the interface GUIDs of the network adapters whose driver marks them
 * hidden (the NCF_HIDDEN characteristic), which Network Connections and
 * Get-NetAdapter leave out: the WAN Miniports, the Wi-Fi Direct virtual
 * adapters and the kernel debugger adapter, among others. Adapters that are
 * not present are included. An adapter whose driver key cannot be read is left
 * out. Returns std::nullopt, logged through failureLog, if the network
 * adapters cannot be listed.
 *
 * Whether an adapter is hidden is set when its driver is installed, so callers
 * read this again only when an interface appears.
 */
[[nodiscard]] std::optional<std::vector<GUID>> queryHiddenAdapterGuids(RepeatedFailureLog& failureLog);

} // namespace sysmon::platform
