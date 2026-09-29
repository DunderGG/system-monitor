#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "domain/system_activity_sample.h"

namespace sysmon::ui
{

// Pure builders for the numeric readouts shown beside the Performance tab's
// charts. Each returns the same names in the same order whether or not the
// data is available, so the readout layout does not jump; missing values are
// "N/A", never zero.

/** One named value, e.g. {"Handles", "200,157"}. */
struct Readout
{
    QString name;
    QString value;

    friend bool operator==(const Readout&, const Readout&) = default;
};

/** Returns readouts with every value replaced by "--", for display before the first snapshot. */
[[nodiscard]] std::vector<Readout> placeholderReadouts(std::vector<Readout> readouts);

/** Formats a count with comma thousands separators, e.g. "200,157". */
[[nodiscard]] QString formatCount(uint64_t count);

/** Formats a clock speed: "4.70 GHz" from 1000 MHz, otherwise "800 MHz". */
[[nodiscard]] QString formatClockSpeed(uint32_t megahertz);

/** Formats a link speed in decimal bits per second, e.g. "2.5 Gbps", "100 Mbps". */
[[nodiscard]] QString formatLinkSpeed(uint64_t bitsPerSecond);

/** CPU page: utilization, base speed, logical processors, processes, threads, handles, and up time. */
[[nodiscard]] std::vector<Readout> cpuReadouts(const std::optional<domain::CpuSample>& cpu,
                                               const std::optional<domain::SystemActivitySample>& activity,
                                               const std::optional<std::chrono::milliseconds>& uptime);

/** Memory page: in use, available, total, committed, cached, paged pool, and non-paged pool. */
[[nodiscard]] std::vector<Readout> memoryReadouts(const std::optional<domain::MemorySample>& memory);

/**
 * Disk page: one readout per volume in name order, e.g. {"C:\", "750.0/1000.0
 * GiB used (75.0%)\n250.0 GiB free"}. A single "Volumes" readout says "N/A"
 * when the query failed and "None" when it found no volumes.
 */
[[nodiscard]] std::vector<Readout> diskReadouts(const std::optional<std::vector<domain::DiskSample>>& disks);

/**
 * Network page, for one adapter: receive, send, link speed, type, adapter
 * description, IPv4 and IPv6 addresses, DNS servers, and bytes received and
 * sent since the counters started. Every value is "N/A" when adapter is
 * std::nullopt (the last query failed).
 */
[[nodiscard]] std::vector<Readout> adapterReadouts(const std::optional<domain::NetworkSample>& adapter);

} // namespace sysmon::ui
