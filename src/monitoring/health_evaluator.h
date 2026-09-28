#pragma once

#include <optional>

#include "domain/connectivity_status.h"
#include "domain/health_status.h"
#include "domain/system_snapshot.h"

namespace sysmon::monitoring
{

/**
 * Usage-percent thresholds for one resource. A value strictly above
 * warningPercent is Warning; strictly above criticalPercent is Critical.
 * Invariant: warningPercent <= criticalPercent.
 */
struct UsageThresholds
{
    float warningPercent{90.0f};
    float criticalPercent{95.0f};
};

/**
 * Thresholds used to derive SystemHealth from a snapshot. Defaults follow the
 * roadmap example (memory > 90% is a warning). CPU uses a higher critical
 * threshold because short full-load bursts are normal.
 */
struct HealthThresholds
{
    UsageThresholds cpu{.warningPercent = 90.0f, .criticalPercent = 98.0f};
    UsageThresholds memory{.warningPercent = 90.0f, .criticalPercent = 95.0f};
    UsageThresholds disk{.warningPercent = 90.0f, .criticalPercent = 95.0f};
};

/** Classifies a usage percent. std::nullopt (no data) yields HealthLevel::Unknown. */
[[nodiscard]] domain::HealthLevel evaluateUsage(std::optional<float> usagePercent, const UsageThresholds& thresholds);

/**
 * Classifies connectivity: InternetAccess is Healthy, Unknown is Unknown, and
 * any lesser level (None, LocalAccess, ConstrainedInternetAccess) is Warning.
 */
[[nodiscard]] domain::HealthLevel evaluateConnectivity(domain::ConnectivityLevel level);

/**
 * Derives per-resource and overall health from a snapshot. Pure function.
 * Overall is Critical if any component is Critical, else Warning if any is
 * Warning, else Unknown if any is Unknown, else Healthy.
 */
[[nodiscard]] domain::SystemHealth evaluateHealth(const domain::SystemSnapshot& snapshot,
                                                  const HealthThresholds& thresholds);

} // namespace sysmon::monitoring
