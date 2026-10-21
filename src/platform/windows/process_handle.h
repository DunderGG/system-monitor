#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <type_traits>

#include <windows.h>

namespace verdandi::platform
{

// Internal header: it exposes Windows types to the platform/windows sources
// and the integration tests. Other modules must not include it.

/** Closes a kernel object handle (a process, a thread, or a token) with CloseHandle. */
struct HandleCloser
{
    void operator()(HANDLE handle) const noexcept;
};

/**
 * An owned kernel object handle, closed when it goes out of scope. Holds
 * nullptr when the call that should have opened it failed.
 */
using ScopedHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

/**
 * Returns the creation time of the process behind the handle, read with
 * GetProcessTimes, which needs PROCESS_QUERY_LIMITED_INFORMATION. Comparing it
 * with ProcessIdentity::creationTime tells whether the handle is the process
 * the identity names or a later one that reused its PID. Returns std::nullopt
 * if GetProcessTimes fails; GetLastError() then says why.
 */
[[nodiscard]] std::optional<std::chrono::system_clock::time_point> processCreationTime(HANDLE process);

} // namespace verdandi::platform
