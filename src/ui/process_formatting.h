#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <QString>

#include "domain/process_info.h"
#include "domain/process_termination.h"

namespace sysmon::ui
{

// Pure presentation helpers for the Processes tab. Like the dashboard helpers
// in dashboard_formatting.h, they take domain values and return display text,
// so they can be unit tested without widgets.

/** Name shown for the Idle process (PID 0), which has no image name in the process list. */
inline constexpr std::string_view kIdleProcessName = "System Idle Process";

/**
 * Returns the name to show for a process: its image name, or
 * kIdleProcessName for PID 0. Any other process without a name shows "N/A".
 */
[[nodiscard]] QString processDisplayName(const domain::ProcessInfo& process);

/**
 * Returns the bytes per second the process read and wrote, or std::nullopt
 * when either rate is missing, because a partial sum would understate it.
 */
[[nodiscard]] std::optional<uint64_t> processIoBytesPerSec(const domain::ProcessInfo& process);

/** Tooltip for the I/O column: the read and write rates on their own, e.g. "Read: 1.0 KiB/s\nWrite: N/A". */
[[nodiscard]] QString processIoToolTip(const domain::ProcessInfo& process);

/**
 * Tooltip for the Memory column: the private working set that the cell shows,
 * then the full working set and the commit size, each with what it counts, e.g.
 * "Private working set: 207.0 MiB\nWorking set: 378.7 MiB (includes memory
 * shared with other processes)\nCommit size: …".
 */
[[nodiscard]] QString processMemoryToolTip(const domain::ProcessInfo& process);

/** Most characters of a path or command line a tooltip shows; the rest is cut off with an ellipsis. */
inline constexpr qsizetype kToolTipDetailLength = 2000;

/**
 * Tooltip for the name column, as rich text paragraphs so Qt wraps long
 * lines: the image path and command line when they could be read, and
 * otherwise why not, from the process's details status, followed by
 * whichever detail could be read. Details are HTML-escaped and cut off after
 * kToolTipDetailLength characters.
 */
[[nodiscard]] QString processDetailsToolTip(const domain::ProcessInfo& process);

/**
 * Summary line above the process table: "Processes: 412" for the latest list,
 * "Processes: --" before the first one, and "Processes: N/A (showing the last
 * list read)" when the latest snapshot has none but an earlier one did.
 */
[[nodiscard]] QString processListSummary(std::optional<std::size_t> listedCount, bool hasEarlierList);

/**
 * Question in the dialog that confirms ending a process, naming the process
 * and its PID: "Do you want to end explorer.exe (PID 1234)?"
 */
[[nodiscard]] QString processTerminationQuestion(const domain::ProcessInfo& process);

/**
 * Message shown when ending a process did not end it. The text names the
 * process and its PID, e.g. "Could not end explorer.exe (PID 1234)." or
 * "explorer.exe (PID 1234) has already exited.", and the informative text
 * says why. Only the text contains the process's name.
 */
struct ProcessTerminationMessage
{
    QString text;
    QString informativeText;
};

/**
 * Returns the message for the result of ending a process, or std::nullopt
 * for Terminated, which needs none: the process's row is marked as exited
 * when the next process list no longer has it.
 */
[[nodiscard]] std::optional<ProcessTerminationMessage>
processTerminationMessage(const domain::ProcessInfo& process, domain::ProcessTerminationResult result);

} // namespace sysmon::ui
