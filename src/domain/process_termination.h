#pragma once

namespace verdandi::domain
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
    // The process runs elevated: its token is elevated, as for a program run
    // as administrator, or it runs at high or system integrity, as a system
    // process does. Only unelevated processes are terminated, even when the
    // monitor itself runs elevated, so termination was not attempted.
    Elevated,
    // A process the monitor never opens: lsass.exe and csrss.exe (see
    // architecture.md, "Antivirus and EDR considerations"). Termination was
    // not attempted.
    NeverOpened,
    // Windows denied access: to open the process, to read its token, or to
    // terminate it (a system process or a service, for an unelevated monitor,
    // or a process that Windows protects).
    AccessDenied,
    // Any other failure; the log has the error code.
    Failed,
};

} // namespace verdandi::domain
