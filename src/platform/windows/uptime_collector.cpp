#include "platform/windows/uptime_collector.h"

#include <utility>

#include <windows.h>

namespace sysmon::platform
{

UptimeCollector::UptimeCollector() : m_reader([] { return static_cast<uint64_t>(::GetTickCount64()); }) {}

UptimeCollector::UptimeCollector(TickCountReader reader) : m_reader(std::move(reader)) {}

std::chrono::milliseconds UptimeCollector::collect()
{
    return std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(m_reader())};
}

} // namespace sysmon::platform
