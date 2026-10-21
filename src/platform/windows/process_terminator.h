#pragma once

#include "domain/process_info.h"
#include "domain/process_termination.h"

namespace verdandi::platform
{

/**
 * Terminates the process that `process` identifies, by (pid, creationTime),
 * if the current user owns it and it does not run elevated. Its imageName is
 * used only to refuse the processes the monitor never opens, so it must be the
 * name the process list reported for the process. In order, it:
 * 1. refuses lsass.exe and csrss.exe by name, and any other process without
 *    a name (NeverOpened), before any OpenProcess call, and the System Idle
 *    Process (PID 0), which belongs to the system (NotOwnedByCurrentUser);
 * 2. opens the process with PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE
 *    and checks its creation time, so a process that reused the PID is never
 *    touched, and that it has not exited (AlreadyExited);
 * 3. compares the user of the process's token with the user of this
 *    process's token (NotOwnedByCurrentUser if they differ);
 * 4. reads the token's elevation and integrity level, and refuses a process
 *    whose token is elevated or whose integrity level is high or system
 *    (Elevated), even when this process runs elevated itself;
 * 5. only then opens the process again with PROCESS_TERMINATE added, checks
 *    its creation time again, and calls TerminateProcess with exit code 1.
 *
 * Windows ends the process asynchronously: Terminated means it accepted the
 * request. Logs the outcome: info for Terminated and AlreadyExited, warn for
 * a refusal or a denial, error for any other failure. Keeps no state and does
 * not wait for the process to exit, so it returns quickly on any thread.
 */
[[nodiscard]] domain::ProcessTerminationResult terminateOwnedProcess(const domain::ProcessInfo& process);

} // namespace verdandi::platform
