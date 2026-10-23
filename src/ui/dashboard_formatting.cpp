#include "ui/dashboard_formatting.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

#include <QDateTime>
#include <QTimeZone>

namespace verdandi::ui
{

namespace
{

// A unit and its multiples, from the unit up, and the step from each to the next.
struct UnitScale
{
    uint64_t step{};
    std::array<const char*, 6> names{};
};

constexpr UnitScale kBinaryBytes{1024, {"B", "KiB", "MiB", "GiB", "TiB", "PiB"}};
constexpr UnitScale kDecimalBytes{1000, {"B", "KB", "MB", "GB", "TB", "PB"}};
constexpr UnitScale kBitsPerSecond{1000, {"bps", "Kbps", "Mbps", "Gbps", "Tbps", "Pbps"}};

constexpr uint64_t kBitsPerByte = 8;

const UnitScale& byteScale(domain::ByteUnits units)
{
    return units == domain::ByteUnits::Decimal ? kDecimalBytes : kBinaryBytes;
}

// The unit count is shown in, and its size in the first unit.
struct ScaledUnit
{
    uint64_t size{1};
    const char* name{};
};

// Returns whether count, shown in the unit of unitSize, reaches the next unit:
// a whole number of the first unit from step, and one decimal of a larger
// unit from step - 0.05, which rounds to step.
bool reachesNextUnit(uint64_t count, uint64_t unitSize, uint64_t step)
{
    if (count / unitSize >= step) {
        return true;
    }
    // count is below step units here, so neither product overflows.
    return unitSize > 1 && 20 * count >= (20 * step - 1) * unitSize;
}

// The largest of scale's units that count reaches, so that it never shows as
// a whole step of a smaller one, such as "1024.0 KiB" for 1,048,575 bytes.
ScaledUnit unitFor(uint64_t count, const UnitScale& scale)
{
    ScaledUnit unit{.size = 1, .name = scale.names.front()};
    for (std::size_t index = 1; index < scale.names.size() && reachesNextUnit(count, unit.size, scale.step); ++index) {
        unit = {.size = unit.size * scale.step, .name = scale.names[index]};
    }
    return unit;
}

// Formats count in its unitFor() unit: a whole number of the first unit, or else to one decimal.
QString formatScaled(uint64_t count, const UnitScale& scale)
{
    const ScaledUnit unit = unitFor(count, scale);
    if (unit.size == 1) {
        return QString("%1 %2").arg(count).arg(unit.name);
    }
    return QString("%1 %2").arg(static_cast<double>(count) / static_cast<double>(unit.size), 0, 'f', 1).arg(unit.name);
}

} // namespace

uint64_t usedMemoryBytes(const domain::MemorySample& memory)
{
    return memory.totalBytes > memory.availableBytes ? memory.totalBytes - memory.availableBytes : 0ULL;
}

QString formatPercent(float percent)
{
    return QString("%1%").arg(static_cast<double>(percent), 0, 'f', 1);
}

QString formatCount(uint64_t count)
{
    QString digits = QString::number(count);
    for (qsizetype position = digits.size() - 3; position > 0; position -= 3) {
        digits.insert(position, ',');
    }
    return digits;
}

QString formatCoreCount(int coreCount)
{
    return coreCount == 1 ? QString("1 core") : QString("%1 cores").arg(coreCount);
}

ByteUnit byteUnitFor(uint64_t bytes, domain::ByteUnits units)
{
    const ScaledUnit unit = unitFor(bytes, byteScale(units));
    return ByteUnit{.bytes = unit.size, .name = QString::fromLatin1(unit.name)};
}

QString formatBytes(uint64_t bytes, domain::ByteUnits units)
{
    return formatScaled(bytes, byteScale(units));
}

QString formatByteRate(uint64_t bytesPerSecond, domain::ByteUnits units)
{
    return formatBytes(bytesPerSecond, units) + "/s";
}

QString formatNetworkRate(uint64_t bytesPerSecond, domain::DisplayUnits units)
{
    if (units.networkRates == domain::NetworkRateUnits::BytesPerSecond) {
        return formatByteRate(bytesPerSecond, units.bytes);
    }
    // A rate beyond any link saturates rather than wraps.
    constexpr uint64_t kLargestInBits = std::numeric_limits<uint64_t>::max() / kBitsPerByte;
    const uint64_t bitsPerSecond = std::min(bytesPerSecond, kLargestInBits) * kBitsPerByte;
    return formatScaled(bitsPerSecond, kBitsPerSecond);
}

QString formatUptime(std::chrono::milliseconds uptime)
{
    const auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
    const auto days = totalSeconds / 86'400;
    const auto hours = (totalSeconds % 86'400) / 3'600;
    const auto minutes = (totalSeconds % 3'600) / 60;
    const auto seconds = totalSeconds % 60;

    if (days > 0) {
        return QString("%1d %2h %3m").arg(days).arg(hours).arg(minutes);
    }
    if (hours > 0) {
        return QString("%1h %2m").arg(hours).arg(minutes);
    }
    return QString("%1m %2s").arg(minutes).arg(seconds);
}

QString formatUptimeStart(std::chrono::system_clock::time_point start)
{
    const auto sinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch());
    // QDateTime's own formats name the months in English whatever the locale.
    const QDateTime local = QDateTime::fromMSecsSinceEpoch(sinceEpoch.count(), QTimeZone::LocalTime);
    return QString("Since Windows started, %1").arg(local.toString("d MMM HH:mm"));
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

QString connectivityLevelText(domain::ConnectivityLevel level)
{
    switch (level) {
        case domain::ConnectivityLevel::InternetAccess:
            return "Internet access";
        case domain::ConnectivityLevel::ConstrainedInternetAccess:
            return "Limited internet (captive portal)";
        case domain::ConnectivityLevel::LocalAccess:
            return "Local network only";
        case domain::ConnectivityLevel::None:
            return "No connectivity";
        case domain::ConnectivityLevel::Unknown:
        default:
            return "Unknown";
    }
}

QString connectivityText(const domain::ConnectivityStatus& status)
{
    QString text = status.level == domain::ConnectivityLevel::Unknown ? QString("Connectivity unknown")
                                                                      : connectivityLevelText(status.level);
    if (status.isMetered.value_or(false)) {
        text += ", metered";
    }
    if (status.isRoaming.value_or(false)) {
        text += ", roaming";
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

} // namespace verdandi::ui
