#include "ui/process_formatting.h"

#include "ui/dashboard_formatting.h"

namespace sysmon::ui
{

QString processDisplayName(const domain::ProcessInfo& process)
{
    if (!process.imageName.empty()) {
        return QString::fromStdString(process.imageName);
    }
    return process.pid == 0 ? QStringLiteral("System Idle Process") : kNotAvailable;
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
