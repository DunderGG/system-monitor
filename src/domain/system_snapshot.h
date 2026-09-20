#pragma once

#include <chrono>
#include <vector>

#include "connectivity_status.h"
#include "cpu_sample.h"
#include "disk_sample.h"
#include "memory_sample.h"
#include "network_sample.h"
#include "process_info.h"

namespace sysmon::domain
{

// An immutable snapshot of all monitored system metrics at a single point in time.
//
// timestamp uses steady_clock — a monotonic clock with no calendar meaning —
// because it is used exclusively for elapsed-time calculations (CPU %, network
// throughput rates). Do not use it for display; format system_clock::now() if
// a human-readable timestamp is needed in the UI.
//
// SystemSnapshot is a value type intended to cross thread boundaries via Qt's
// queued signal/slot mechanism. It must remain copyable. Collectors on the
// scheduler thread build a new snapshot each tick; the UI thread receives it
// and discards the previous one. No shared mutable state persists between ticks.
struct SystemSnapshot
{
    std::chrono::steady_clock::time_point timestamp;
    CpuSample                             cpu;
    MemorySample                          memory;
    std::vector<DiskSample>               disks;
    std::vector<NetworkSample>            networks;
    ConnectivityStatus                    connectivity;
    std::vector<ProcessInfo>              processes;
};

} // namespace sysmon::domain

