#include "ui/performance_formatting.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ui/dashboard_formatting.h"

namespace sysmon::ui
{

namespace
{

const QString kNotAvailable = "N/A";

bool isUp(const domain::NetworkSample& adapter)
{
    return adapter.operationalStatus == domain::OperationalStatus::Up;
}

} // namespace

QString formatChartPercent(float percent)
{
    return QString("%1%").arg(std::lround(percent));
}

QString formatChartByteRate(float bytesPerSecond)
{
    return formatByteRate(static_cast<uint64_t>(std::llround(std::max(0.0f, bytesPerSecond))));
}

QString formatBytesOfTotal(uint64_t used, uint64_t total)
{
    constexpr std::array kUnits{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    constexpr double kStep = 1024.0;

    if (total < 1024) {
        return QString("%1/%2 B").arg(used).arg(total);
    }

    double divisor = 1.0;
    std::size_t unitIndex = 0;
    while (static_cast<double>(total) / divisor >= kStep && unitIndex + 1 < kUnits.size()) {
        divisor *= kStep;
        ++unitIndex;
    }
    return QString("%1/%2 %3")
        .arg(static_cast<double>(used) / divisor, 0, 'f', 1)
        .arg(static_cast<double>(total) / divisor, 0, 'f', 1)
        .arg(kUnits[unitIndex]);
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
        if (isUp(adapter)) {
            charted.push_back(&adapter);
        }
    }
    std::ranges::stable_sort(charted, [](const domain::NetworkSample* left, const domain::NetworkSample* right) {
        if (left->isHardwareInterface != right->isHardwareInterface) {
            return left->isHardwareInterface;
        }
        return adapterDisplayName(*left).localeAwareCompare(adapterDisplayName(*right)) < 0;
    });
    return charted;
}

QString cpuSummary(const std::optional<domain::CpuSample>& cpu)
{
    return cpu ? formatPercent(cpu->totalUsagePercent) : kNotAvailable;
}

QString memorySummary(const std::optional<domain::MemorySample>& memory)
{
    if (!memory) {
        return kNotAvailable;
    }
    const uint64_t usedBytes =
        (memory->totalBytes > memory->availableBytes) ? (memory->totalBytes - memory->availableBytes) : 0ULL;
    return QString("%1 (%2)").arg(formatBytesOfTotal(usedBytes, memory->totalBytes),
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

QString networkSummary(const std::optional<std::vector<domain::NetworkSample>>& networks)
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
        .arg(formatByteRate(totals->inBytesPerSec), formatByteRate(totals->outBytesPerSec));
}

} // namespace sysmon::ui
