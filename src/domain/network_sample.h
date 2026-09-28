#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysmon::domain
{

/**
 * Maps directly to IF_OPER_STATUS from the Windows IP Helper API (GetIfTable2).
 * Loopback and NDIS filter interfaces are filtered out before this type is produced.
 */
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

/**
 * A single snapshot of one network adapter's traffic counters, throughput, and link state.
 *
 * inBytesTotal and outBytesTotal are cumulative 64-bit byte counters as
 * returned by GetIfTable2.
 *
 * inBytesPerSec and outBytesPerSec are instantaneous throughput rates computed
 * from byte counter deltas over measured monotonic elapsed time. Each is
 * std::nullopt when no rate can be computed: the first sample for an adapter
 * (no baseline yet), zero elapsed time, or a counter that went backwards (reset).
 * A genuinely idle link reports 0.
 *
 * linkSpeedBps is the negotiated link speed in bits per second.
 *
 * isHardwareInterface is true for a physical adapter and false for tunnels,
 * VPNs, and other virtual interfaces. Traffic through a virtual interface
 * usually also crosses a physical one, so a total across adapters should sum
 * hardware interfaces only.
 */
struct NetworkSample
{
    std::string adapterName;
    std::string friendlyName;
    std::string description;
    uint64_t inBytesTotal{0};
    uint64_t outBytesTotal{0};
    std::optional<uint64_t> inBytesPerSec;
    std::optional<uint64_t> outBytesPerSec;
    uint64_t linkSpeedBps{0};
    OperationalStatus operationalStatus{OperationalStatus::Unknown};
    std::vector<std::string> ipAddresses;
    std::vector<std::string> dnsServers;
    bool isHardwareInterface{false};
};

} // namespace sysmon::domain
