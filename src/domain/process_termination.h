#pragma once

namespace sysmon::domain
{

/**
 * The outcome of a request to terminate one process, identified by
 * (PID, creation time). Only Terminated means the process is being ended;
 * every other value means it was left running, or had already exited, and
 * says why.
 */
enum class ProcessTerminationResult
{
    // Windows accepted the request. Termination completes asynchronously, so
    // the process can stay in the process list for a moment.
    Terminated,
    // No running process has this identity: the process exited before the
    // request, or its PID now belongs to another process, which was not touched.
    AlreadyExited,
    // The process belongs to another user, such as SYSTEM or a service
    // account. Only the current user's processes are terminated, so
    // termination was not attempted.
    NotOwnedByCurrentUser,
    // A process the monitor never opens: lsass.exe and csrss.exe (see
    // architecture.md, "Antivirus and EDR considerations"). Termination was
    // not attempted.
    NeverOpened,
    // Windows denied access: to open the process, to read who owns it, or to
    // terminate it (an elevated process, for an unelevated monitor).
    AccessDenied,
    // Any other failure; the log has the error code.
    Failed,
};

} // namespace sysmon::domain
