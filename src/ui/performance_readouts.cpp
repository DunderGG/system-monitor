#include "ui/performance_readouts.h"

#include <algorithm>
#include <array>
#include <string>

#include <QStringList>

#include "ui/dashboard_formatting.h"
#include "ui/performance_formatting.h"

namespace sysmon::ui
{

namespace
{

const QString kNotAvailable = "N/A";

template <typename T, typename Format> QString valueOr(const std::optional<T>& value, Format format)
{
    return value ? format(*value) : kNotAvailable;
}

QString countText(uint64_t count)
{
    return formatCount(count);
}

QString bytesText(uint64_t bytes)
{
    return formatBytes(bytes);
}

QString rateText(uint64_t bytesPerSecond)
{
    return formatByteRate(bytesPerSecond);
}

// Joins addresses one per line, or "None" when there are none.
QString addressList(const std::vector<std::string>& addresses)
{
    if (addresses.empty()) {
        return "None";
    }
    QStringList lines;
    for (const auto& address : addresses) {
        lines.append(QString::fromStdString(address));
    }
    return lines.join('\n');
}

bool isIpv6(const std::string& address)
{
    return address.find(':') != std::string::npos;
}

} // namespace

std::vector<Readout> placeholderReadouts(std::vector<Readout> readouts)
{
    for (auto& readout : readouts) {
        readout.value = "--";
    }
    return readouts;
}

QString formatCount(uint64_t count)
{
    QString digits = QString::number(count);
    for (qsizetype position = digits.size() - 3; position > 0; position -= 3) {
        digits.insert(position, ',');
    }
    return digits;
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
    constexpr std::array kUnits{Unit{1e9, "Gbps"}, Unit{1e6, "Mbps"}, Unit{1e3, "Kbps"}};

    const auto value = static_cast<double>(bitsPerSecond);
    for (const auto& unit : kUnits) {
        if (value >= unit.scale) {
            return QString("%1 %2").arg(QString::number(value / unit.scale, 'g', 3), unit.suffix);
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
        {"Logical processors", valueOr(coreCount, countText)},
        {"Processes", count(&domain::SystemActivitySample::processCount)},
        {"Threads", count(&domain::SystemActivitySample::threadCount)},
        {"Handles", count(&domain::SystemActivitySample::handleCount)},
        {"Up time", valueOr(uptime, formatUptime)},
    };
}

std::vector<Readout> memoryReadouts(const std::optional<domain::MemorySample>& memory)
{
    if (!memory) {
        return {
            {"In use", kNotAvailable},         {"Available", kNotAvailable}, {"Total", kNotAvailable},
            {"Committed", kNotAvailable},      {"Cached", kNotAvailable},    {"Paged pool", kNotAvailable},
            {"Non-paged pool", kNotAvailable},
        };
    }
    const uint64_t usedBytes =
        (memory->totalBytes > memory->availableBytes) ? (memory->totalBytes - memory->availableBytes) : 0ULL;
    return {
        {"In use", QString("%1 (%2)").arg(formatBytes(usedBytes), formatPercent(memory->usagePercent))},
        {"Available", formatBytes(memory->availableBytes)},
        {"Total", formatBytes(memory->totalBytes)},
        {"Committed", formatBytesOfTotal(memory->commitCurrent, memory->commitLimit)},
        {"Cached", valueOr(memory->cachedBytes, bytesText)},
        {"Paged pool", valueOr(memory->pagedPoolBytes, bytesText)},
        {"Non-paged pool", valueOr(memory->nonPagedPoolBytes, bytesText)},
    };
}

std::vector<Readout> diskReadouts(const std::optional<std::vector<domain::DiskSample>>& disks)
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
                .arg(formatBytesOfTotal(usedBytes, volume->totalBytes), formatPercent(volume->usagePercent),
                     formatBytes(volume->freeBytes)),
        });
    }
    return readouts;
}

std::vector<Readout> adapterReadouts(const std::optional<domain::NetworkSample>& adapter)
{
    if (!adapter) {
        return {
            {"Receive", kNotAvailable},      {"Send", kNotAvailable},        {"Link speed", kNotAvailable},
            {"Type", kNotAvailable},         {"Adapter", kNotAvailable},     {"IPv4 address", kNotAvailable},
            {"IPv6 address", kNotAvailable}, {"DNS servers", kNotAvailable}, {"Received", kNotAvailable},
            {"Sent", kNotAvailable},
        };
    }

    std::vector<std::string> ipv4;
    std::vector<std::string> ipv6;
    for (const auto& address : adapter->ipAddresses) {
        (isIpv6(address) ? ipv6 : ipv4).push_back(address);
    }
    // A link speed of 0 means the adapter did not report one.
    const auto linkSpeed = adapter->linkSpeedBps > 0 ? std::optional<uint64_t>{adapter->linkSpeedBps} : std::nullopt;
    return {
        {"Receive", valueOr(adapter->inBytesPerSec, rateText)},
        {"Send", valueOr(adapter->outBytesPerSec, rateText)},
        {"Link speed", valueOr(linkSpeed, formatLinkSpeed)},
        {"Type", adapter->isHardwareInterface ? "Hardware" : "Virtual"},
        {"Adapter", adapter->description.empty() ? kNotAvailable : QString::fromStdString(adapter->description)},
        {"IPv4 address", addressList(ipv4)},
        {"IPv6 address", addressList(ipv6)},
        {"DNS servers", addressList(adapter->dnsServers)},
        {"Received", formatBytes(adapter->inBytesTotal)},
        {"Sent", formatBytes(adapter->outBytesTotal)},
    };
}

} // namespace sysmon::ui
