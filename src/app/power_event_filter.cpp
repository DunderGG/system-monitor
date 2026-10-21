#include "power_event_filter.h"

#include "platform/windows/power_transition.h"

namespace sysmon::app
{

PowerEventFilter::PowerEventFilter(monitoring::SamplingScheduler& scheduler) : m_scheduler(scheduler) {}

bool PowerEventFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/)
{
    // Windows sends WM_POWERBROADCAST to each of the monitor's top-level
    // windows, and Qt passes each on as a "windows_generic_MSG". With the
    // Settings dialog open a resume arrives twice, and the tick may then
    // discard one more sample.
    if (eventType != "windows_generic_MSG") {
        return false;
    }

    const auto transition = platform::powerTransitionFromMessage(message);
    if (transition == platform::PowerTransition::Suspending) {
        m_scheduler.onSystemSuspending();
    } else if (transition == platform::PowerTransition::Resumed) {
        m_scheduler.onSystemResumed();
    }
    return false;
}

} // namespace sysmon::app
