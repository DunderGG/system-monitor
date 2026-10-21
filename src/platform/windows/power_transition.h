#pragma once

#include <optional>

namespace verdandi::platform
{

/** A change of the system's power state that the monitor acts on (DC-129). */
enum class PowerTransition
{
    // PBT_APMSUSPEND: the system is about to sleep or hibernate.
    Suspending,
    // PBT_APMRESUMEAUTOMATIC: the system has resumed. Windows sends it on every resume.
    Resumed,
};

/**
 * Returns the power transition that a native Windows message reports, or
 * std::nullopt if it reports none. message points to a MSG, as Qt passes it to
 * a native event filter for a "windows_generic_MSG". Only WM_POWERBROADCAST
 * with PBT_APMSUSPEND or PBT_APMRESUMEAUTOMATIC reports one.
 * PBT_APMRESUMESUSPEND, which follows PBT_APMRESUMEAUTOMATIC when the user's
 * input woke the system, does not, so that each resume is reported once.
 */
[[nodiscard]] std::optional<PowerTransition> powerTransitionFromMessage(const void* message);

} // namespace verdandi::platform
