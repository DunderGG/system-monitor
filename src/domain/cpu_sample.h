#pragma once

#include <vector>

namespace sysmon::domain
{

// A single sample of CPU utilization across all logical cores.
// totalUsagePercent is the aggregate across all cores (0–100).
// coreUsagePercents has one entry per logical core, in processor index order, or
// is empty when per-core data is unavailable for this sample (never zero-filled).
struct CpuSample
{
    float totalUsagePercent{0.0f};
    std::vector<float> coreUsagePercents;
    int coreCount{0};
};

} // namespace sysmon::domain
