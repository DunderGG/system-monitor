#pragma once

#include "platform/windows/process_identity_cache.h"

namespace verdandi::platform
{

/**
 * Returns the production ProcessDetailsReader. For each process it:
 * - opens the process with PROCESS_QUERY_LIMITED_INFORMATION only, through an
 *   RAII handle;
 * - checks with GetProcessTimes that the handle's creation time matches the
 *   process's identity, since the PID may have been reused after the process
 *   list was read;
 * - reads the image path with QueryFullProcessImageNameW;
 * - reads the command line with NtQueryInformationProcess
 *   (ProcessCommandLineInformation), resolved from ntdll.dll once, here.
 *
 * Either detail can fail on its own; the status is AccessDenied if Windows
 * denied anything, Unavailable if anything else failed (the process exited,
 * or it has no image file or command line), and Available otherwise.
 *
 * It refuses lsass.exe and csrss.exe by name, and a process without a name
 * other than the Idle process, before calling OpenProcess, as ProcessCollector
 * already does (isNeverOpenedProcess()), returning NotQueried. The check is
 * only as good as the name, so the ProcessInfo passed in must carry the image
 * name the process list reported. Failures are routine for an unelevated
 * monitor, so they are logged at debug level only. The reader keeps no state
 * between calls.
 */
[[nodiscard]] ProcessDetailsReader makeWindowsProcessDetailsReader();

} // namespace verdandi::platform
