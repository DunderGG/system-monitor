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
 * interfaceId identifies the interface while it exists: its LUID, which does
 * not change when the connection is renamed (or, when Windows reports no LUID,
 * its interface index, and failing that a hash of its name). Samples with the same
 * interfaceId describe the same interface. adapterName and the other names are
 * for display only; they can change, and descriptions are not unique.
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
 *
 * hasIpInterface is true when IP is bound to the adapter, so it can carry IP
 * traffic. Windows keeps some hidden virtual adapters, such as the WAN
 * Miniports, Up without an IP interface; they never carry traffic. It is also
 * false for every adapter while the adapter details (names, addresses, DNS
 * servers) cannot be read.
 */
struct NetworkSample
{
    uint64_t interfaceId{0};
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
    bool hasIpInterface{false};
};

} // namespace sysmon::domain
