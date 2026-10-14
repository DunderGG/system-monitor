#include "ui/process_formatting.h"

#include "ui/dashboard_formatting.h"

namespace sysmon::ui
{

QString processDisplayName(const domain::ProcessInfo& process)
{
    if (!process.imageName.empty()) {
        return QString::fromStdString(process.imageName);
    }
    if (process.pid == 0) {
        return QString::fromUtf8(kIdleProcessName.data(), static_cast<qsizetype>(kIdleProcessName.size()));
    }
    return kNotAvailable;
}

std::optional<uint64_t> processIoBytesPerSec(const domain::ProcessInfo& process)
{
    if (!process.ioReadBytesPerSec || !process.ioWriteBytesPerSec) {
        return std::nullopt;
    }
    return *process.ioReadBytesPerSec + *process.ioWriteBytesPerSec;
}

QString processIoToolTip(const domain::ProcessInfo& process)
{
    const auto rateText = [](const std::optional<uint64_t>& rate) {
        return rate ? formatByteRate(*rate) : kNotAvailable;
    };
    return QString("Read: %1\nWrite: %2")
        .arg(rateText(process.ioReadBytesPerSec), rateText(process.ioWriteBytesPerSec));
}

QString processListSummary(std::optional<std::size_t> listedCount, bool hasEarlierList)
{
    if (listedCount) {
        return "Processes: " + formatCount(*listedCount);
    }
    return hasEarlierList ? QStringLiteral("Processes: N/A (showing the last list read)")
                          : QStringLiteral("Processes: --");
}

} // namespace sysmon::ui
