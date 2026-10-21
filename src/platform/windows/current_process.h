#pragma once

#include <optional>

#include "domain/process_identity.h"

namespace verdandi::platform
{

/**
 * Returns this process's identity, its PID and its creation time read with
 * GetProcessTimes, which match the entry the process list has for it. Returns
 * std::nullopt, and logs a warning, if GetProcessTimes fails.
 */
[[nodiscard]] std::optional<domain::ProcessIdentity> currentProcessIdentity();

} // namespace verdandi::platform
