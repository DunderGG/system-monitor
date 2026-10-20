#include "ui/performance_readouts.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ui/dashboard_formatting.h"
#include "ui/network_formatting.h"
#include "ui/performance_formatting.h"

namespace sysmon::ui
{

namespace
{

// Formats value with format, which may be any callable taking a T, or returns "N/A".
template <typename T, typename Format> QString valueOr(const std::optional<T>& value, Format format)
{
    return value ? format(*value) : kNotAvailable;
}

} // namespace

std::vector<Readout> placeholderReadouts(std::vector<Readout> readouts)
{
    for (auto& readout : readouts) {
        readout.value = "--";
        readout.toolTip.clear();
    }
    return readouts;
}

QString formatClockSpeed(uint32_t megahertz)
{
    if (megahertz >= 1000) {
        return QString("%1 GHz").arg(static_cast<double>(megahertz) / 1000.0, 0, 'f', 2);
    }
    return QString("%1 MHz").arg(megahertz);
}

QString formatLinkSpeed(uint64_t bitsPerSecond)
{
    struct Unit
    {
        double scale;
        const char* suffix;
    };
    constexpr std::array kUnits{Unit{1e12, "Tbps"}, Unit{1e9, "Gbps"}, Unit{1e6, "Mbps"}, Unit{1e3, "Kbps"}};
    constexpr int kSignificantDigits = 3;

    if (bitsPerSecond < 1000) {
        return QString("%1 bps").arg(bitsPerSecond);
    }
    // Round to three significant digits before choosing the unit, so a value
    // just below a unit boundary shows as "1 Gbps" rather than "1e+03 Mbps".
    const auto exact = static_cast<double>(bitsPerSecond);
    const double step = std::pow(10.0, std::floor(std::log10(exact)) - (kSignificantDigits - 1));
    const double value = std::round(exact / step) * step;
    for (const auto& unit : kUnits) {
        if (value >= unit.scale) {
            const double scaled = value / unit.scale;
            // Only the largest unit can reach 1000, where 'g' would switch to an exponent.
            const QString number =
                scaled < 1000.0 ? QString::number(scaled, 'g', kSignificantDigits) : QString::number(scaled, 'f', 0);
            return QString("%1 %2").arg(number, unit.suffix);
        }
    }
    return QString("%1 bps").arg(bitsPerSecond);
}

std::vector<Readout> cpuReadouts(const std::optional<domain::CpuSample>& cpu,
                                 const std::optional<domain::SystemActivitySample>& activity,
                                 const std::optional<std::chrono::milliseconds>& uptime)
{
    const auto baseSpeed = cpu ? cpu->baseSpeedMhz : std::nullopt;
    const auto coreCount =
        cpu && cpu->coreCount > 0 ? std::optional<uint64_t>{static_cast<uint64_t>(cpu->coreCount)} : std::nullopt;
    const auto count = [&activity](uint32_t domain::SystemActivitySample::* field) {
        return activity ? formatCount((*activity).*field) : kNotAvailable;
    };
    return {
        {"Utilization", cpu ? formatPercent(cpu->totalUsagePercent) : kNotAvailable},
        {"Base speed", valueOr(baseSpeed, formatClockSpeed)},
        {"Logical processors", valueOr(coreCount, formatCount)},
        {"Processes", count(&domain::SystemActivitySample::processCount)},
        {"Threads", count(&domain::SystemActivitySample::threadCount)},
        {"Handles", count(&domain::SystemActivitySample::handleCount)},
        {"Up time", valueOr(uptime, formatUptime)},
    };
}

std::vector<Readout> memoryReadouts(const std::optional<domain::MemorySample>& memory, domain::ByteUnits units)
{
    if (!memory) {
        return {
            {"Used", kNotAvailable},           {"Available", kNotAvailable},    {"Total", kNotAvailable},
            {"Committed", kNotAvailable},      {"System cache", kNotAvailable}, {"Paged pool", kNotAvailable},
            {"Non-paged pool", kNotAvailable},
        };
    }
    const auto bytes = [units](uint64_t count) { return formatBytes(count, units); };
    const uint64_t usedBytes = usedMemoryBytes(*memory);
    return {
        {"Used", QString("%1 (%2)").arg(bytes(usedBytes), formatPercent(memory->usagePercent))},
        {"Available", bytes(memory->availableBytes)},
        {"Total", bytes(memory->totalBytes)},
        {"Committed", formatBytesOfTotal(memory->commitCurrent, memory->commitLimit, units)},
        {"System cache", valueOr(memory->systemCacheBytes, bytes)},
        {"Paged pool", valueOr(memory->pagedPoolBytes, bytes)},
        {"Non-paged pool", valueOr(memory->nonPagedPoolBytes, bytes)},
    };
}

std::vector<Readout> diskReadouts(const std::optional<std::vector<domain::DiskSample>>& disks, domain::ByteUnits units)
{
    if (!disks) {
        return {{"Volumes", kNotAvailable}};
    }
    if (disks->empty()) {
        return {{"Volumes", "None"}};
    }

    std::vector<const domain::DiskSample*> volumes;
    for (const auto& disk : *disks) {
        volumes.push_back(&disk);
    }
    std::ranges::sort(volumes, {}, &domain::DiskSample::volumeName);

    std::vector<Readout> readouts;
    for (const auto* volume : volumes) {
        const uint64_t usedBytes = volume->totalBytes > volume->freeBytes ? volume->totalBytes - volume->freeBytes : 0;
        readouts.push_back({
            QString::fromStdString(volume->volumeName),
            QString("%1 used (%2)\n%3 free")
                .arg(formatBytesOfTotal(usedBytes, volume->totalBytes, units), formatPercent(volume->usagePercent),
                     formatBytes(volume->freeBytes, units)),
        });
    }
    return readouts;
}

std::vector<Readout> adapterReadouts(const std::optional<domain::NetworkSample>& adapter, domain::DisplayUnits units)
{
    if (!adapter) {
        return {
            {"Receive", kNotAvailable},      {"Send", kNotAvailable},        {"Link speed", kNotAvailable},
            {"Type", kNotAvailable},         {"Adapter", kNotAvailable},     {"IPv4 address", kNotAvailable},
            {"IPv6 address", kNotAvailable}, {"DNS servers", kNotAvailable}, {"Received", kNotAvailable},
            {"Sent", kNotAvailable},
        };
    }

    // A link speed of 0 means the adapter did not report one.
    const auto linkSpeed = adapter->linkSpeedBps > 0 ? std::optional<uint64_t>{adapter->linkSpeedBps} : std::nullopt;
    const auto rate = [units](uint64_t bytesPerSecond) { return formatNetworkRate(bytesPerSecond, units); };
    return {
        {"Receive", valueOr(adapter->inBytesPerSec, rate)},
        {"Send", valueOr(adapter->outBytesPerSec, rate)},
        {"Link speed", valueOr(linkSpeed, formatLinkSpeed), linkSpeed ? linkSpeedToolTip(*adapter) : QString{}},
        {"Type", adapterTypeText(*adapter)},
        {"Adapter", adapter->description.empty() ? kNotAvailable : QString::fromStdString(adapter->description)},
        {"IPv4 address", ipv4AddressText(*adapter)},
        {"IPv6 address", ipv6AddressText(*adapter)},
        {"DNS servers", formatAddressList(adapter->dnsServers)},
        {"Received", formatBytes(adapter->inBytesTotal, units.bytes)},
        {"Sent", formatBytes(adapter->outBytesTotal, units.bytes)},
    };
}

} // namespace sysmon::ui
