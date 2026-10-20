#include "ui/performance_formatting.h"

#include <algorithm>
#include <cmath>

#include "ui/dashboard_formatting.h"
#include "ui/network_formatting.h"

namespace sysmon::ui
{

namespace
{

bool isUp(const domain::NetworkSample& adapter)
{
    return adapter.operationalStatus == domain::OperationalStatus::Up;
}

// A virtual adapter is charted only when it is bound to IP. Hidden adapters
// such as the WAN Miniports are Up without an IP interface and never carry
// traffic.
bool isChartable(const domain::NetworkSample& adapter)
{
    return isUp(adapter) && (adapter.isHardwareInterface || adapter.hasIpInterface);
}

} // namespace

QString formatChartPercent(float percent)
{
    return QString("%1%").arg(std::lround(percent));
}

QString formatChartNetworkRate(float bytesPerSecond, domain::DisplayUnits units)
{
    return formatNetworkRate(static_cast<uint64_t>(std::llround(std::max(0.0f, bytesPerSecond))), units);
}

QStringList chartNetworkRateAxisTemplates(domain::DisplayUnits units)
{
    // One byte per second below each unit boundary: the most digits each unit
    // shows. Bits per second step by 1,000, and 1,000^n bits are 1,000^n / 8 bytes.
    constexpr int kUnitCount = 4;
    constexpr uint64_t kBitsPerByte = 8;
    const bool inBits = units.networkRates == domain::NetworkRateUnits::BitsPerSecond;
    const uint64_t step = inBits || units.bytes == domain::ByteUnits::Decimal ? 1000 : 1024;

    QStringList templates;
    uint64_t boundary = 1;
    for (int unit = 0; unit < kUnitCount; ++unit) {
        boundary *= step;
        templates.push_back(formatNetworkRate((inBits ? boundary / kBitsPerByte : boundary) - 1, units));
    }
    return templates;
}

QString formatBytesOfTotal(uint64_t used, uint64_t total, domain::ByteUnits units)
{
    const ByteUnit unit = byteUnitFor(total, units);
    if (unit.bytes == 1) {
        return QString("%1/%2 %3").arg(used).arg(total).arg(unit.name);
    }
    const auto size = static_cast<double>(unit.bytes);
    return QString("%1/%2 %3")
        .arg(static_cast<double>(used) / size, 0, 'f', 1)
        .arg(static_cast<double>(total) / size, 0, 'f', 1)
        .arg(unit.name);
}

std::optional<float> commitPercent(const domain::MemorySample& memory)
{
    if (memory.commitLimit == 0) {
        return std::nullopt;
    }
    const float percent =
        static_cast<float>(static_cast<double>(memory.commitCurrent) * 100.0 / static_cast<double>(memory.commitLimit));
    return std::clamp(percent, 0.0f, 100.0f);
}

QString adapterDisplayName(const domain::NetworkSample& adapter)
{
    if (!adapter.friendlyName.empty()) {
        return QString::fromStdString(adapter.friendlyName);
    }
    if (!adapter.description.empty()) {
        return QString::fromStdString(adapter.description);
    }
    return QString::fromStdString(adapter.adapterName);
}

std::vector<const domain::NetworkSample*> chartedAdapters(const std::vector<domain::NetworkSample>& networks)
{
    std::vector<const domain::NetworkSample*> charted;
    for (const auto& adapter : networks) {
        if (isChartable(adapter)) {
            charted.push_back(&adapter);
        }
    }
    // The Network tab's order, so the two tabs list the same adapters alike.
    std::ranges::sort(charted, [](const domain::NetworkSample* left, const domain::NetworkSample* right) {
        return isListedBefore(*left, *right);
    });
    return charted;
}

QString cpuSummary(const std::optional<domain::CpuSample>& cpu)
{
    return cpu ? formatPercent(cpu->totalUsagePercent) : kNotAvailable;
}

QString memorySummary(const std::optional<domain::MemorySample>& memory, domain::ByteUnits units)
{
    if (!memory) {
        return kNotAvailable;
    }
    const uint64_t usedBytes = usedMemoryBytes(*memory);
    return QString("%1 (%2)").arg(formatBytesOfTotal(usedBytes, memory->totalBytes, units),
                                  formatPercent(memory->usagePercent));
}

QString diskSummary(const std::optional<std::vector<domain::DiskSample>>& disks)
{
    if (!disks) {
        return kNotAvailable;
    }
    const auto* fullest = fullestVolume(*disks);
    if (fullest == nullptr) {
        return "No volumes";
    }
    return QString("%1 %2 used").arg(QString::fromStdString(fullest->volumeName), formatPercent(fullest->usagePercent));
}

QString networkSummary(const std::optional<std::vector<domain::NetworkSample>>& networks, domain::DisplayUnits units)
{
    if (!networks) {
        return kNotAvailable;
    }
    if (!hasActiveAdapter(*networks)) {
        return "No active adapter";
    }
    const auto totals = sumActiveThroughput(*networks);
    if (!totals) {
        return kNotAvailable;
    }
    return QString("Receive %1\nSend %2")
        .arg(formatNetworkRate(totals->inBytesPerSec, units), formatNetworkRate(totals->outBytesPerSec, units));
}

} // namespace sysmon::ui
