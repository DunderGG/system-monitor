#pragma once

#include <string_view>

#include "platform/windows/process_identity_cache.h"

namespace sysmon::platform
{

/**
 * Returns true for the processes the monitor never opens, by image name,
 * ignoring case: lsass.exe and csrss.exe. Opening them, even with
 * PROCESS_QUERY_LIMITED_INFORMATION, triggers credential-theft heuristics in
 * most EDR products (architecture.md, "Antivirus and EDR considerations").
 * Their metrics come from the process list, which needs no handle.
 *
 * Matches by name alone, so any process with one of these names is skipped,
 * whatever its path.
 */
[[nodiscard]] bool isNeverOpenedProcess(std::string_view imageName);

/**
 * Wraps a details reader so it is never called for a process that
 * isNeverOpenedProcess() names; such a process gets ProcessDetails with
 * status NotQueried instead. A null reader stays null.
 */
[[nodiscard]] ProcessDetailsReader skippingNeverOpenedProcesses(ProcessDetailsReader reader);

} // namespace sysmon::platform
