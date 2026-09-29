#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace sysmon::domain
{

/**
 * A single sample of CPU utilization across all logical cores.
 * totalUsagePercent is the aggregate across all cores (0–100).
 * coreUsagePercents has one entry per logical core, in processor index order, or
 * is empty when per-core data is unavailable for this sample (never zero-filled).
 * baseSpeedMhz is the processor's rated (base) clock speed in megahertz, the
 * highest across logical processors on hybrid designs; std::nullopt when unknown.
 * It is not the current clock speed.
 */
struct CpuSample
{
    float totalUsagePercent{0.0f};
    std::vector<float> coreUsagePercents;
    int coreCount{0};
    std::optional<uint32_t> baseSpeedMhz;
};

} // namespace sysmon::domain
