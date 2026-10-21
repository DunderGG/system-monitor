#include <array>
#include <optional>

#include <windows.h>

#include <gtest/gtest.h>

#include "platform/windows/power_transition.h"

using verdandi::platform::PowerTransition;
using verdandi::platform::powerTransitionFromMessage;

TEST(PowerTransition, PowerTransitionFromMessage_OnlySuspendAndAutomaticResume)
{
    struct Case
    {
        const char* name;
        UINT message;
        WPARAM event;
        std::optional<PowerTransition> expected;
    };
    const std::array cases{
        Case{"suspend", WM_POWERBROADCAST, PBT_APMSUSPEND, PowerTransition::Suspending},
        Case{"automatic resume", WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, PowerTransition::Resumed},
        Case{"resume after the user's input", WM_POWERBROADCAST, PBT_APMRESUMESUSPEND, std::nullopt},
        Case{"power status change", WM_POWERBROADCAST, PBT_APMPOWERSTATUSCHANGE, std::nullopt},
        Case{"power setting change", WM_POWERBROADCAST, PBT_POWERSETTINGCHANGE, std::nullopt},
        Case{"another message with the suspend code", WM_ACTIVATEAPP, PBT_APMSUSPEND, std::nullopt},
    };

    for (const auto& [name, message, event, expected] : cases) {
        SCOPED_TRACE(name);
        MSG msg{};
        msg.message = message;
        msg.wParam = event;
        EXPECT_EQ(powerTransitionFromMessage(&msg), expected);
    }
}
