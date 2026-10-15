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

QString processMemoryToolTip(const domain::ProcessInfo& process)
{
    return QString("Private working set: %1\n"
                   "Working set: %2 (includes memory shared with other processes)\n"
                   "Commit size: %3 (private memory committed, which counts toward the commit limit)")
        .arg(formatBytes(process.privateWorkingSetBytes), formatBytes(process.workingSetBytes),
             formatBytes(process.privateBytes));
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

// "explorer.exe (PID 1234)". One arg() call, so a "%1" or "%2" in the name
// stays literal; arg() does not look for placeholders in what it inserts.
QString nameAndPid(const domain::ProcessInfo& process)
{
    return QString("%1 (PID %2)").arg(processDisplayName(process), QString::number(process.pid));
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
    return QString("Do you want to end %1?").arg(nameAndPid(process));
}

std::optional<ProcessTerminationMessage> processTerminationMessage(const domain::ProcessInfo& process,
                                                                   domain::ProcessTerminationResult result)
{
    const auto notEnded = [&process](const QString& reason) {
        return ProcessTerminationMessage{
            .text = QString("Could not end %1.").arg(nameAndPid(process)),
            .informativeText = reason,
        };
    };

    switch (result) {
        case domain::ProcessTerminationResult::Terminated:
            return std::nullopt;
        case domain::ProcessTerminationResult::AlreadyExited:
            return ProcessTerminationMessage{
                .text = QString("%1 has already exited.").arg(nameAndPid(process)),
                .informativeText = QStringLiteral("It exited before it could be ended. If another process now has its "
                                                  "PID, that process was left running."),
            };
        case domain::ProcessTerminationResult::NotOwnedByCurrentUser:
            return notEnded(QStringLiteral("It runs as another user, such as SYSTEM or a service account. The monitor "
                                           "ends only processes that run as you."));
        case domain::ProcessTerminationResult::Elevated:
            return notEnded(QStringLiteral("It runs elevated, with administrator or system rights. The monitor ends "
                                           "only processes that run with standard user rights."));
        case domain::ProcessTerminationResult::NeverOpened:
            return notEnded(QStringLiteral("It is a critical Windows process, which the monitor never opens."));
        case domain::ProcessTerminationResult::AccessDenied:
            return notEnded(QStringLiteral("Windows denied access to it. That happens for a system process or a "
                                           "service when the monitor runs without administrator rights, and for a "
                                           "process that Windows protects."));
        case domain::ProcessTerminationResult::Failed:
            return notEnded(
                QStringLiteral("Windows reported an unexpected error. The monitor's log has the error code."));
    }
    return std::nullopt;
}

} // namespace sysmon::ui
