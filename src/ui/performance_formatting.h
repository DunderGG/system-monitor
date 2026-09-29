#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"

namespace sysmon::ui
{

// Pure presentation helpers for the Performance tab. Like the dashboard
// helpers in dashboard_formatting.h, they take domain values and return
// display text so they can be unit tested without widgets. Missing data is
// "N/A", never a zero.

/** Formats a chart axis or annotation value as a whole percentage, e.g. "42%". */
[[nodiscard]] QString formatChartPercent(float percent);

/** Formats a chart axis or annotation value as a byte rate, e.g. "1.5 MiB/s". Negative values show as 0 B/s. */
[[nodiscard]] QString formatChartByteRate(float bytesPerSecond);

/**
 * Formats a used and a total byte count in the total's binary unit, e.g.
 * "8.0/16.0 GiB", so the pair reads as one quantity.
 */
[[nodiscard]] QString formatBytesOfTotal(uint64_t used, uint64_t total);

/**
 * Returns the percentage of the commit limit that is committed, or
 * std::nullopt when the limit is zero (no valid reading).
 */
[[nodiscard]] std::optional<float> commitPercent(const domain::MemorySample& memory);

/** Returns the name to show for an adapter: its friendly name, else its description, else its adapter name. */
[[nodiscard]] QString adapterDisplayName(const domain::NetworkSample& adapter);

/**
 * Returns the adapters the network page charts: those whose operational status
 * is Up, hardware adapters first, each group ordered by display name.
 */
[[nodiscard]] std::vector<const domain::NetworkSample*>
chartedAdapters(const std::vector<domain::NetworkSample>& networks);

/** Sidebar summary for the CPU page, e.g. "12.5%". */
[[nodiscard]] QString cpuSummary(const std::optional<domain::CpuSample>& cpu);

/** Sidebar summary for the memory page, e.g. "8.0/16.0 GiB (50.0%)". */
[[nodiscard]] QString memorySummary(const std::optional<domain::MemorySample>& memory);

/** Sidebar summary for the disk page: the fullest volume, e.g. "C:\ 75.0% used". */
[[nodiscard]] QString diskSummary(const std::optional<std::vector<domain::DiskSample>>& disks);

/**
 * Sidebar summary for the network page: receive and send throughput over
 * active hardware adapters on two lines, e.g. "Receive 1.2 MiB/s\nSend 30.0 KiB/s".
 */
[[nodiscard]] QString networkSummary(const std::optional<std::vector<domain::NetworkSample>>& networks);

} // namespace sysmon::ui
