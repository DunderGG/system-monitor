#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "platform/windows/instance_mutex.h"

namespace verdandi::app
{

/** How long a later start keeps trying to reach the running monitor before it gives up (DC-142). */
inline constexpr std::chrono::milliseconds kRunningInstanceTimeout{5000};

/** What claimSingleInstance() found. */
enum class InstanceClaimResult
{
    // This process created the instance mutex. It is the monitor, and listens
    // for later starts on a server of the mutex's name.
    Claimed,
    // The mutex could neither be created nor found held, so this process runs
    // as the monitor without it.
    Unchecked,
    // Another process holds the mutex, and answered that it brought its window
    // to the front.
    BroughtToFront,
    // Another process holds the mutex, but did not answer before the timeout.
    NotAnswered,
};

/** What claimSingleInstance() found, and the mutex it created. */
struct InstanceClaim
{
    InstanceClaimResult result{InstanceClaimResult::Unchecked};
    /** The mutex this process created, when result is Claimed. */
    std::optional<platform::InstanceMutex> mutex{};
    /** Why the result is Unchecked; empty otherwise. */
    std::string problem{};
};

/**
 * Makes this process the instance of application for the current user in the
 * current logon session, or asks the process that is to bring its window to
 * the front, through the InstanceServer named after the mutex (DC-142). Until
 * timeout passes, it tries again while that process cannot be reached, as
 * when it is starting or closing, and claims the mutex once it has closed.
 * Blocks the calling thread, which needs no event loop.
 */
[[nodiscard]] InstanceClaim claimSingleInstance(std::string_view application, std::chrono::milliseconds timeout);

} // namespace verdandi::app
