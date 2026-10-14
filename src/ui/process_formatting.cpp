#include "ui/process_formatting.h"

#include <string>

#include <QChar>

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

namespace
{

QString paragraph(const QString& html)
{
    return "<p>" + html + "</p>";
}

QString detailParagraph(const QString& label, const std::optional<std::string>& detail)
{
    if (!detail) {
        return {};
    }
    QString text = QString::fromStdString(*detail);
    if (text.size() > kToolTipDetailLength) {
        text.truncate(kToolTipDetailLength);
        text += QChar(0x2026); // Horizontal ellipsis.
    }
    return paragraph("<b>" + label + ":</b> " + text.toHtmlEscaped());
}

QString statusExplanation(domain::ProcessDetailsStatus status)
{
    switch (status) {
        case domain::ProcessDetailsStatus::Available:
            return {};
        case domain::ProcessDetailsStatus::AccessDenied:
            return QStringLiteral("Access denied: Windows did not let the monitor read this process's path and command "
                                  "line. Without administrator rights that is usual for system processes and for other "
                                  "users' processes.");
        case domain::ProcessDetailsStatus::NotQueried:
            return QStringLiteral("Not read: the monitor never opens this process, because security software treats "
                                  "opening it as suspicious, so its path and command line are not known.");
        case domain::ProcessDetailsStatus::Unavailable:
            return QStringLiteral("Unavailable: the process has no path or command line to read, as with the Idle "
                                  "process and other kernel processes, or it exited before they could be read.");
    }
    return {};
}

} // namespace

QString processDetailsToolTip(const domain::ProcessInfo& process)
{
    QString text;
    if (const QString explanation = statusExplanation(process.detailsStatus); !explanation.isEmpty()) {
        text += paragraph(explanation.toHtmlEscaped());
    }
    text += detailParagraph("Path", process.imagePath);
    text += detailParagraph("Command line", process.commandLine);
    return text;
}

QString processListSummary(std::optional<std::size_t> listedCount, bool hasEarlierList)
{
    if (listedCount) {
        return "Processes: " + formatCount(*listedCount);
    }
    return hasEarlierList ? QStringLiteral("Processes: N/A (showing the last list read)")
                          : QStringLiteral("Processes: --");
}

QString processTerminationQuestion(const domain::ProcessInfo& process)
{
    // One arg() call, so a "%1" or "%2" in the name stays literal.
    return QString("Do you want to end %1 (PID %2)?").arg(processDisplayName(process), QString::number(process.pid));
}

} // namespace sysmon::ui
