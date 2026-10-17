#include "system_collectors.h"

#include <memory>

#include "platform/windows/connectivity_collector.h"
#include "platform/windows/cpu_collector.h"
#include "platform/windows/disk_collector.h"
#include "platform/windows/memory_collector.h"
#include "platform/windows/network_collector.h"
#include "platform/windows/network_profile_collector.h"
#include "platform/windows/process_collector.h"
#include "platform/windows/system_activity_collector.h"
#include "platform/windows/uptime_collector.h"

namespace sysmon::app
{

void registerSystemCollectors(monitoring::SamplingScheduler& scheduler)
{
    scheduler.setCpuCollector(std::make_unique<platform::CpuCollector>());
    scheduler.setMemoryCollector(std::make_unique<platform::MemoryCollector>());
    scheduler.setDiskCollector(std::make_unique<platform::DiskCollector>());
    scheduler.setNetworkCollector(std::make_unique<platform::NetworkCollector>());
    scheduler.setConnectivityCollector(std::make_unique<platform::ConnectivityCollector>());
    scheduler.setNetworkProfileCollector(std::make_unique<platform::NetworkProfileCollector>());
    scheduler.setUptimeCollector(std::make_unique<platform::UptimeCollector>());
    scheduler.setSystemActivityCollector(std::make_unique<platform::SystemActivityCollector>());
    scheduler.setProcessCollector(std::make_unique<platform::ProcessCollector>());
}

} // namespace sysmon::app
