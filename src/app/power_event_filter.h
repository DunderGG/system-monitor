#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QtGlobal>

#include "monitoring/sampling_scheduler.h"

namespace sysmon::app
{

/**
 * Passes the system's suspend and resume to the scheduler (DC-129). Windows
 * broadcasts them to every top-level window as WM_POWERBROADCAST, so once
 * installed on the application, the filter sees them as long as one of the
 * monitor's windows exists. It never consumes a message, and removes itself
 * from the application when destroyed.
 */
class PowerEventFilter : public QAbstractNativeEventFilter
{
public:
    explicit PowerEventFilter(monitoring::SamplingScheduler& scheduler);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    monitoring::SamplingScheduler& m_scheduler;
};

} // namespace sysmon::app
