#pragma once

namespace sysmon::domain
{

/**
 * Health of a resource, derived from thresholds applied to the latest sample.
 * Unknown  — no data to evaluate (collector missing or sample unavailable).
 *            Never treated as Healthy.
 * Healthy  — within normal limits.
 * Warning  — above the warning threshold, or degraded (e.g. no internet access).
 * Critical — above the critical threshold.
 */
enum class HealthLevel
{
    Unknown,
    Healthy,
    Warning,
    Critical,
};

/**
 * Per-resource and overall health for one snapshot.
 * disk is the worst level across all fixed volumes. network is derived from
 * connectivity. overall is the worst known problem; if nothing is wrong but
 * some component is Unknown, overall is Unknown rather than Healthy.
 */
struct SystemHealth
{
    HealthLevel cpu{HealthLevel::Unknown};
    HealthLevel memory{HealthLevel::Unknown};
    HealthLevel disk{HealthLevel::Unknown};
    HealthLevel network{HealthLevel::Unknown};
    HealthLevel overall{HealthLevel::Unknown};

    bool operator==(const SystemHealth&) const = default;
};

} // namespace sysmon::domain
