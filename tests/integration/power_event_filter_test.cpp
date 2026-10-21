#include <windows.h>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QWidget>

#include "app/power_event_filter.h"
#include "monitoring/sampling_scheduler.h"

// app::PowerEventFilter, given WM_POWERBROADCAST as Windows sends it to each
// top-level window: through the window procedure that Qt gives its windows.

using verdandi::app::PowerEventFilter;
using verdandi::monitoring::SamplingScheduler;

TEST(PowerEventFilterIntegration, PowerBroadcastToAWindow_SuspendsAndResumesTheScheduler)
{
    SamplingScheduler scheduler;
    PowerEventFilter filter(scheduler);
    QCoreApplication::instance()->installNativeEventFilter(&filter);
    QWidget window;
    const auto hwnd = reinterpret_cast<HWND>(window.winId());

    ::SendMessageW(hwnd, WM_POWERBROADCAST, PBT_APMSUSPEND, 0);
    const bool isSuspended = scheduler.isSystemSuspended();
    ::SendMessageW(hwnd, WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0);

    EXPECT_TRUE(isSuspended);
    EXPECT_FALSE(scheduler.isSystemSuspended());
}
