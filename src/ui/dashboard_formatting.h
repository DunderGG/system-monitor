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
#include "domain/settings.h"

namespace verdandi::ui
{

// Pure presentation helpers for the dashboard. They take domain values and
// return display text, so they can be unit tested without widgets. Byte
// quantities are in the byte units the settings choose (DC-126): binary (IEC)
// units, KiB, MiB, GiB, TiB, or decimal (SI) units, KB, MB, GB, TB. Network
// throughput is in bytes per second in those units, or in bits per second,
// which are always decimal. The Performance tab's helpers build on them.

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

/** Formats a number of logical processors for the dashboard's CPU card, e.g. "1 core" or "16 cores". */
[[nodiscard]] QString formatCoreCount(int coreCount);

/** A unit that byte counts are shown in, such as "KiB" or "MB", and its size in bytes. "B" is one byte. */
struct ByteUnit
{
    uint64_t bytes{1};
    QString name{};
};

/**
 * Returns the unit that formatBytes() shows bytes in: the largest of B and
 * the multiples that units names, up to PiB or PB, that is not above bytes,
 * or the next one when bytes rounds to a whole step of it at one decimal, so
 * that 1,048,575 bytes are "1.0 MiB", not "1024.0 KiB".
 */
[[nodiscard]] ByteUnit byteUnitFor(uint64_t bytes, domain::ByteUnits units);

/**
 * Formats a byte count in its byteUnitFor() unit: a whole number of bytes,
 * e.g. "512 B", and otherwise to one decimal, e.g. "1.5 KiB" or "16.0 GiB" in
 * binary units, and "1.5 KB" in decimal units.
 */
[[nodiscard]] QString formatBytes(uint64_t bytes, domain::ByteUnits units);

/** Formats a byte rate as formatBytes() does, per second, e.g. "1.2 MiB/s". */
[[nodiscard]] QString formatByteRate(uint64_t bytesPerSecond, domain::ByteUnits units);

/**
 * Formats network throughput in the units' network rate units: bytes per
 * second as formatByteRate() does in the units' byte units, e.g. "1.2 MiB/s",
 * or bits per second, whole below 1 Kbps and otherwise to one decimal in
 * powers of 1,000, e.g. "800 bps" or "10.1 Mbps".
 */
[[nodiscard]] QString formatNetworkRate(uint64_t bytesPerSecond, domain::DisplayUnits units);

/**
 * Formats uptime compactly: "3d 4h 12m" from one day, "4h 12m" from one hour,
 * otherwise "12m 5s".
 */
[[nodiscard]] QString formatUptime(std::chrono::milliseconds uptime);

/**
 * Says when the uptime count started, to the minute in local time, e.g.
 * "Since Windows started, 16 Oct 06:12", in English whatever the locale.
 */
[[nodiscard]] QString formatUptimeStart(std::chrono::system_clock::time_point start);

/**
 * Says what starts a new uptime count and what does not (DC-42), for the
 * tooltips of the values that show uptime.
 */
inline const QString kUptimeToolTip = QStringLiteral(
    "Counts from when Windows last started. Restart starts a new count. Sleep and hibernation do not, and "
    "neither does Shut down while Fast Startup is on, because Windows then resumes rather than starts.");

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

} // namespace verdandi::ui
