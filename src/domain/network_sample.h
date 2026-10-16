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
 * The kind of link a network adapter uses, from its physical medium where
 * Windows reports one and otherwise from its interface type (GetIfTable2).
 * Whether the adapter is physical or virtual is NetworkSample::isHardwareInterface:
 * a Hyper-V or kernel debugger adapter is a virtual Ethernet adapter.
 * Other covers interface types with no category here, such as the
 * proprietary virtual interfaces of many VPN clients.
 */
enum class AdapterType
{
    Ethernet,
    WiFi,
    Bluetooth,
    MobileBroadband,
    Tunnel,
    Ppp,
    Other,
};

/**
 * A unicast address of a network adapter.
 *
 * text is the address as text. A link-local IPv6 address carries its zone
 * index, e.g. "fe80::1%16", as ipconfig shows it, because the address cannot
 * be used without it.
 *
 * isTemporary marks a temporary IPv6 address, which Windows generates from a
 * random suffix and uses for outgoing connections in place of the stable one.
 * isLinkLocal marks an address that is valid only on its link: IPv6
 * fe80::/10, and IPv4 169.254.0.0/16, which Windows assigns when no DHCP
 * server answers.
 */
struct IpAddress
{
    std::string text;
    bool isTemporary{false};
    bool isLinkLocal{false};

    bool operator==(const IpAddress&) const = default;
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
 * linkSpeedBps is the negotiated link speed in bits per second. Windows keeps
 * reporting the last speed, or a nominal one, while the adapter is not Up.
 *
 * type is the kind of link (see AdapterType); it is always known.
 *
 * ipAddresses are the unicast addresses the adapter can use: those in normal
 * use, and while the adapter is Up also deprecated ones, which can still carry
 * existing connections. Windows keeps addresses on a disconnected adapter,
 * such as the lease from its last connection, but they are left out.
 * dnsServers are the configured DNS servers, without the placeholders Windows
 * lists when there are none. Windows keeps them while the adapter is not Up.
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
 *
 * isHidden is true for an adapter that Windows hides from Network Connections,
 * because its driver marks it hidden: the WAN Miniports, the Wi-Fi Direct
 * virtual adapters, and the kernel debugger adapter. It is false when Windows
 * could not say, and for interfaces without a network adapter device, such as
 * the tunnel pseudo-interfaces, which hasIpInterface already sets apart.
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
    AdapterType type{AdapterType::Other};
    std::vector<IpAddress> ipAddresses;
    std::vector<std::string> dnsServers;
    bool isHardwareInterface{false};
    bool hasIpInterface{false};
    bool isHidden{false};
};

} // namespace sysmon::domain
