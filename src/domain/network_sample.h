#pragma once

#include <cstdint>
#include <string>

namespace sysmon::domain
{

// Maps directly to IF_OPER_STATUS from the Windows IP Helper API (GetIfTable2).
// Loopback adapters are filtered out before this type is produced.
enum class OperationalStatus
{
    Up,
    Down,
    Testing,
    Unknown,
    Dormant,
    NotPresent,
    LowerLayerDown,
};

// A single snapshot of one network adapter's traffic counters and link state.
//
// inBytesTotal and outBytesTotal are cumulative 64-bit byte counters as
// returned by GetIfTable2. Rate calculation (bytes/sec) is the responsibility
// of the collector, which computes deltas between successive snapshots using
// measured elapsed time from std::chrono::steady_clock.
//
// linkSpeedBps is the negotiated link speed in bits per second.
struct NetworkSample
{
    std::string adapterName;
    uint64_t inBytesTotal{0};
    uint64_t outBytesTotal{0};
    uint64_t linkSpeedBps{0};
    OperationalStatus operationalStatus{OperationalStatus::Unknown};
};

} // namespace sysmon::domain
