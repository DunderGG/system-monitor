#pragma once

#include "monitoring/sampling_scheduler.h"

namespace verdandi::app
{

/**
 * Registers the Windows collector of every metric the monitor shows with
 * scheduler: CPU, memory, disks, network adapters, connectivity, network
 * names, uptime, system activity, and processes. main.cpp and the scheduler
 * pipeline integration test both call it, so the test runs the collectors
 * the app runs. Call it only while scheduler is stopped.
 */
void registerSystemCollectors(monitoring::SamplingScheduler& scheduler);

} // namespace verdandi::app
