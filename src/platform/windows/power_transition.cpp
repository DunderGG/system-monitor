#include "platform/windows/power_transition.h"

#include <cassert>
#include <optional>

#include <windows.h>

namespace verdandi::platform
{

std::optional<PowerTransition> powerTransitionFromMessage(const void* message)
{
    assert(message != nullptr && "powerTransitionFromMessage needs a MSG");
    const auto* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_POWERBROADCAST) {
        return std::nullopt;
    }

    switch (msg->wParam) {
        case PBT_APMSUSPEND:
            return PowerTransition::Suspending;
        case PBT_APMRESUMEAUTOMATIC:
            return PowerTransition::Resumed;
        default:
            return std::nullopt;
    }
}

} // namespace verdandi::platform
