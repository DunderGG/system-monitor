#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "connectivity_status.h"
#include "cpu_sample.h"
#include "disk_sample.h"
#include "health_status.h"
#include "memory_sample.h"
#include "network_profile.h"
#include "network_sample.h"
#include "process_info.h"
#include "system_activity_sample.h"

namespace verdandi::domain
{

/**
 * An immutable snapshot of all monitored system metrics at a single point in time.
 *
 * timestamp uses steady_clock — a monotonic clock with no calendar meaning —
 * because it is used exclusively for elapsed-time calculations (CPU %, network
 * throughput rates). Do not use it for display; format system_clock::now() if
 * a human-readable timestamp is needed in the UI.
 *
 * SystemSnapshot is a value type intended to cross thread boundaries via Qt's
 * queued signal/slot mechanism. It must remain copyable. Collectors on the
 * scheduler thread build a new snapshot each tick; the UI thread receives it
 * and discards the previous one. No shared mutable state persists between ticks.
 * connectivity, networkProfiles, and processes come from slower collectors that
 * refresh every few seconds on their own thread; each snapshot carries their
 * latest result.
 *
 * Optional fields are std::nullopt when no collector is registered or no valid
 * sample was available this tick (never a zero-filled placeholder). For the
 * disks, networks, networkProfiles, and processes collections, std::nullopt
 * means no data, while an empty vector means the query succeeded and found
 * nothing. networkProfiles are the networks the system is connected to. uptime is
 * the time since the system booted, including time spent in sleep or hibernation.
 * health is evaluated by the scheduler from the other fields of this snapshot.
 */
struct SystemSnapshot
{
    std::chrono::steady_clock::time_point timestamp{};
    std::optional<CpuSample> cpu{};
    std::optional<MemorySample> memory{};
    std::optional<std::vector<DiskSample>> disks{};
    std::optional<std::vector<NetworkSample>> networks{};
    ConnectivityStatus connectivity{};
    std::optional<std::vector<NetworkProfile>> networkProfiles{};
    std::optional<std::chrono::milliseconds> uptime{};
    SystemHealth health{};
    std::optional<std::vector<ProcessInfo>> processes{};
    std::optional<SystemActivitySample> activity{};
};

} // namespace verdandi::domain
