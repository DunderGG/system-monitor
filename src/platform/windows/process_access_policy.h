#pragma once

#include "domain/process_info.h"
#include "platform/windows/process_identity_cache.h"

namespace sysmon::platform
{

/**
 * Returns true for the processes the monitor never opens: lsass.exe and
 * csrss.exe, by image name ignoring case, and any process without an image
 * name except the System Idle Process (PID 0). Opening lsass.exe or
 * csrss.exe, even with PROCESS_QUERY_LIMITED_INFORMATION, triggers
 * credential-theft heuristics in most EDR products (architecture.md,
 * "Antivirus and EDR considerations"). Their metrics come from the process
 * list, which needs no handle.
 *
 * Matches by name alone, so any process with one of these names is skipped,
 * whatever its path. A process without a name could be one of them, so the
 * check fails closed. The Idle process has no name in the list, and
 * OpenProcess rejects its PID, so it is not skipped.
 *
 * The check is only as good as the name: process.imageName must be the name
 * the process list reported for the process.
 */
[[nodiscard]] bool isNeverOpenedProcess(const domain::ProcessInfo& process);

/**
 * Wraps a details reader so it is never called for a process that
 * isNeverOpenedProcess() names; such a process gets ProcessDetails with
 * status NotQueried instead. A null reader stays null.
 */
[[nodiscard]] ProcessDetailsReader skippingNeverOpenedProcesses(ProcessDetailsReader reader);

} // namespace sysmon::platform
