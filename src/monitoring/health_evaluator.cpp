#include "monitoring/health_evaluator.h"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <optional>
#include <vector>

namespace sysmon::monitoring
{

namespace
{

// Ranks levels for "worst of" comparisons among known values.
int severity(domain::HealthLevel level)
{
    switch (level) {
        case domain::HealthLevel::Healthy:
            return 0;
        case domain::HealthLevel::Warning:
            return 1;
        case domain::HealthLevel::Critical:
            return 2;
        case domain::HealthLevel::Unknown:
        default:
            return -1;
    }
}

domain::HealthLevel worstDiskLevel(const std::optional<std::vector<domain::DiskSample>> &disks,
                                   const UsageThresholds &thresholds)
{
    if (!disks || disks->empty()) {
        return domain::HealthLevel::Unknown;
    }

    auto worst = domain::HealthLevel::Healthy;
    for (const auto &disk : *disks) {
        const auto level = evaluateUsage(disk.usagePercent, thresholds);
        if (severity(level) > severity(worst)) {
            worst = level;
        }
    }
    return worst;
}

domain::HealthLevel combineLevels(std::initializer_list<domain::HealthLevel> levels)
{
    const auto contains = [&levels](domain::HealthLevel wanted) {
        return std::find(levels.begin(), levels.end(), wanted) != levels.end();
    };

    // A known problem is reported even when other components lack data;
    // missing data is never rounded up to Healthy.
    if (contains(domain::HealthLevel::Critical)) {
        return domain::HealthLevel::Critical;
    }
    if (contains(domain::HealthLevel::Warning)) {
        return domain::HealthLevel::Warning;
    }
    if (contains(domain::HealthLevel::Unknown)) {
        return domain::HealthLevel::Unknown;
    }
    return domain::HealthLevel::Healthy;
}

} // namespace

domain::HealthLevel evaluateUsage(std::optional<float> usagePercent, const UsageThresholds &thresholds)
{
    assert(thresholds.warningPercent <= thresholds.criticalPercent && "warning threshold must not exceed critical");

    if (!usagePercent) {
        return domain::HealthLevel::Unknown;
    }
    if (*usagePercent > thresholds.criticalPercent) {
        return domain::HealthLevel::Critical;
    }
    if (*usagePercent > thresholds.warningPercent) {
        return domain::HealthLevel::Warning;
    }
    return domain::HealthLevel::Healthy;
}

domain::HealthLevel evaluateConnectivity(domain::ConnectivityLevel level)
{
    switch (level) {
        case domain::ConnectivityLevel::InternetAccess:
            return domain::HealthLevel::Healthy;
        case domain::ConnectivityLevel::None:
        case domain::ConnectivityLevel::LocalAccess:
        case domain::ConnectivityLevel::ConstrainedInternetAccess:
            return domain::HealthLevel::Warning;
        case domain::ConnectivityLevel::Unknown:
        default:
            return domain::HealthLevel::Unknown;
    }
}

domain::SystemHealth evaluateHealth(const domain::SystemSnapshot &snapshot, const HealthThresholds &thresholds)
{
    const auto cpuUsage =
        snapshot.cpu ? std::optional<float>{snapshot.cpu->totalUsagePercent} : std::optional<float>{};
    const auto memoryUsage =
        snapshot.memory ? std::optional<float>{snapshot.memory->usagePercent} : std::optional<float>{};

    domain::SystemHealth health{
        .cpu = evaluateUsage(cpuUsage, thresholds.cpu),
        .memory = evaluateUsage(memoryUsage, thresholds.memory),
        .disk = worstDiskLevel(snapshot.disks, thresholds.disk),
        .network = evaluateConnectivity(snapshot.connectivity.level),
    };
    health.overall = combineLevels({health.cpu, health.memory, health.disk, health.network});
    return health;
}

} // namespace sysmon::monitoring
