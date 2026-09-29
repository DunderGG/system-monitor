#pragma once

#include <optional>

#include "domain/system_activity_sample.h"
#include "monitoring/collector.h"
#include "platform/windows/performance_info_reader.h"

namespace sysmon::platform
{

/**
 * Converts GetPerformanceInfo output to a SystemActivitySample. Returns
 * std::nullopt for a process count of zero, which is never a valid reading.
 * Pure function for deterministic unit testing.
 */
[[nodiscard]] std::optional<domain::SystemActivitySample> calculateSystemActivity(const PerformanceInfoData& data);

/**
 * Collects the system-wide process, thread, and handle counts with
 * GetPerformanceInfo. The call reads kernel counters (about 20 microseconds on
 * a desktop) and does not enumerate processes, so it runs on the scheduler
 * tick as a fast collector. collect() returns std::nullopt when the query
 * fails or returns an invalid reading.
 */
class SystemActivityCollector : public monitoring::ISystemActivityCollector
{
public:
    SystemActivityCollector();
    explicit SystemActivityCollector(PerformanceInfoReader reader);
    ~SystemActivityCollector() override = default;

    [[nodiscard]] std::optional<domain::SystemActivitySample> collect() override;

private:
    PerformanceInfoReader m_reader;
};

} // namespace sysmon::platform
