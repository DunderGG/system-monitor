#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <QString>

#include "domain/process_info.h"

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

} // namespace sysmon::ui
