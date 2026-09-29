#include "platform/windows/system_activity_collector.h"

#include <utility>

namespace sysmon::platform
{

std::optional<domain::SystemActivitySample> calculateSystemActivity(const PerformanceInfoData& data)
{
    if (data.processCount == 0) {
        return std::nullopt;
    }
    return domain::SystemActivitySample{
        .processCount = data.processCount,
        .threadCount = data.threadCount,
        .handleCount = data.handleCount,
    };
}

SystemActivityCollector::SystemActivityCollector() : m_reader(makePerformanceInfoReader()) {}

SystemActivityCollector::SystemActivityCollector(PerformanceInfoReader reader) : m_reader(std::move(reader)) {}

std::optional<domain::SystemActivitySample> SystemActivityCollector::collect()
{
    PerformanceInfoData data{};
    if (!m_reader || !m_reader(data)) {
        return std::nullopt;
    }
    return calculateSystemActivity(data);
}

} // namespace sysmon::platform
