#include "ui/dashboard_formatting.h"

#include <algorithm>
#include <array>

namespace sysmon::ui
{

QString formatPercent(float percent)
{
    return QString("%1%").arg(static_cast<double>(percent), 0, 'f', 1);
}

QString formatBytes(uint64_t bytes)
{
    constexpr std::array kUnits{"KiB", "MiB", "GiB", "TiB", "PiB"};
    constexpr double kStep = 1024.0;

    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    }

    double value = static_cast<double>(bytes) / kStep;
    std::size_t unitIndex = 0;
    while (value >= kStep && unitIndex + 1 < kUnits.size()) {
        value /= kStep;
        ++unitIndex;
    }
    return QString("%1 %2").arg(value, 0, 'f', 1).arg(kUnits[unitIndex]);
}

QString formatByteRate(uint64_t bytesPerSecond)
{
    return formatBytes(bytesPerSecond) + "/s";
}

QString formatUptime(std::chrono::milliseconds uptime)
{
    using namespace std::chrono;

    const auto totalSeconds = duration_cast<seconds>(uptime).count();
    const auto days = totalSeconds / 86'400;
    const auto hours = (totalSeconds % 86'400) / 3'600;
    const auto minutes = (totalSeconds % 3'600) / 60;
    const auto secs = totalSeconds % 60;

    if (days > 0) {
        return QString("%1d %2h %3m").arg(days).arg(hours).arg(minutes);
    }
    if (hours > 0) {
        return QString("%1h %2m").arg(hours).arg(minutes);
    }
    return QString("%1m %2s").arg(minutes).arg(secs);
}

QString healthLevelText(domain::HealthLevel level)
{
    switch (level) {
        case domain::HealthLevel::Healthy:
            return "Healthy";
        case domain::HealthLevel::Warning:
            return "Warning";
        case domain::HealthLevel::Critical:
            return "Critical";
        case domain::HealthLevel::Unknown:
        default:
            return "Unknown";
    }
}

QString connectivityText(const domain::ConnectivityStatus& status)
{
    QString text;
    switch (status.level) {
        case domain::ConnectivityLevel::InternetAccess:
            text = "Internet access";
            break;
        case domain::ConnectivityLevel::ConstrainedInternetAccess:
            text = "Limited internet (captive portal)";
            break;
        case domain::ConnectivityLevel::LocalAccess:
            text = "Local network only";
            break;
        case domain::ConnectivityLevel::None:
            text = "No connectivity";
            break;
        case domain::ConnectivityLevel::Unknown:
        default:
            text = "Connectivity unknown";
            break;
    }

    if (status.isMetered.value_or(false)) {
        text += ", metered";
    }
    return text;
}

namespace
{

bool isActiveHardwareAdapter(const domain::NetworkSample& adapter)
{
    return adapter.isHardwareInterface && adapter.operationalStatus == domain::OperationalStatus::Up;
}

} // namespace

std::optional<ThroughputTotals> sumActiveThroughput(const std::vector<domain::NetworkSample>& networks)
{
    ThroughputTotals totals;
    bool hasActive = false;

    for (const auto& adapter : networks) {
        if (!isActiveHardwareAdapter(adapter)) {
            continue;
        }
        if (!adapter.inBytesPerSec || !adapter.outBytesPerSec) {
            return std::nullopt;
        }
        hasActive = true;
        totals.inBytesPerSec += *adapter.inBytesPerSec;
        totals.outBytesPerSec += *adapter.outBytesPerSec;
    }

    if (!hasActive) {
        return std::nullopt;
    }
    return totals;
}

bool hasActiveAdapter(const std::vector<domain::NetworkSample>& networks)
{
    return std::ranges::any_of(networks, isActiveHardwareAdapter);
}

const domain::DiskSample* fullestVolume(const std::vector<domain::DiskSample>& disks)
{
    const auto it = std::ranges::max_element(disks, {}, &domain::DiskSample::usagePercent);
    return it != disks.end() ? &*it : nullptr;
}

} // namespace sysmon::ui
