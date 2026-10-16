#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

#include "domain/connectivity_status.h"
#include "domain/disk_sample.h"
#include "domain/health_status.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"

namespace sysmon::ui
{

// Pure presentation helpers for the dashboard. They take domain values and
// return display text, so they can be unit tested without widgets. Byte
// quantities use binary (IEC) units: KiB, MiB, GiB, TiB. The Performance tab's
// helpers build on them.

/** Text shown for a value that could not be read, everywhere in the UI. Never shown as zero. */
inline const QString kNotAvailable = QStringLiteral("N/A");

/**
 * Returns used physical memory: total minus available, or 0 if a reading
 * reports more available than total. Includes the modified page list.
 */
[[nodiscard]] uint64_t usedMemoryBytes(const domain::MemorySample& memory);

/** Formats a percentage with one decimal, e.g. "42.5%". */
[[nodiscard]] QString formatPercent(float percent);

/** Formats a count with comma thousands separators, e.g. "200,157", whatever the locale. */
[[nodiscard]] QString formatCount(uint64_t count);

/** Formats a byte count with binary units, e.g. "512 B", "1.5 KiB", "16.0 GiB". */
[[nodiscard]] QString formatBytes(uint64_t bytes);

/** Formats a byte rate with binary units, e.g. "1.2 MiB/s". */
[[nodiscard]] QString formatByteRate(uint64_t bytesPerSecond);

/**
 * Formats uptime compactly: "3d 4h 12m" from one day, "4h 12m" from one hour,
 * otherwise "12m 5s".
 */
[[nodiscard]] QString formatUptime(std::chrono::milliseconds uptime);

/** Returns the display name of a health level, e.g. "Warning". */
[[nodiscard]] QString healthLevelText(domain::HealthLevel level);

/** Describes a connectivity level in words, e.g. "Internet access"; Unknown is "Unknown", never offline. */
[[nodiscard]] QString connectivityLevelText(domain::ConnectivityLevel level);

/**
 * Describes connectivity in words for one line, e.g. "Internet access, metered":
 * ", metered" and ", roaming" are appended when the connection is known to be
 * metered or roaming, and an Unknown level reads "Connectivity unknown".
 */
[[nodiscard]] QString connectivityText(const domain::ConnectivityStatus& status);

/** Total inbound and outbound throughput across active hardware adapters, in bytes/sec. */
struct ThroughputTotals
{
    uint64_t inBytesPerSec{0};
    uint64_t outBytesPerSec{0};
};

/**
 * Sums throughput over hardware adapters whose operational status is Up.
 * Virtual adapters (VPN tunnels, virtual switches) are left out because their
 * traffic also crosses a hardware adapter and would be counted twice. Returns
 * std::nullopt when there is no such adapter or any of them has no rate yet,
 * because a partial sum would understate the real throughput.
 */
[[nodiscard]] std::optional<ThroughputTotals> sumActiveThroughput(const std::vector<domain::NetworkSample>& networks);

/** Returns true if any hardware adapter's operational status is Up. */
[[nodiscard]] bool hasActiveAdapter(const std::vector<domain::NetworkSample>& networks);

/** Returns the volume with the highest usage percent, or nullptr when there are none. */
[[nodiscard]] const domain::DiskSample* fullestVolume(const std::vector<domain::DiskSample>& disks);

} // namespace sysmon::ui
