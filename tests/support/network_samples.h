#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "domain/network_sample.h"

namespace sysmon::tests
{

/** What an adapter is, beside its name and status. */
struct AdapterTraits
{
    bool isHardwareInterface{true};
    bool hasIpInterface{true};
};

/** A physical adapter bound to IP, such as an Ethernet or Wi-Fi adapter. */
inline constexpr AdapterTraits kPhysicalAdapter{};
/** A virtual adapter bound to IP, such as a VPN tunnel or a Bluetooth PAN adapter. */
inline constexpr AdapterTraits kVirtualAdapter{.isHardwareInterface = false};
/** A virtual adapter without IP, such as a WAN Miniport, which never carries traffic. */
inline constexpr AdapterTraits kUnboundVirtualAdapter{.isHardwareInterface = false, .hasIpInterface = false};
/** A physical adapter with IP unbound, or whose details could not be read. */
inline constexpr AdapterTraits kUnboundPhysicalAdapter{.hasIpInterface = false};

/**
 * An adapter whose alias is name and whose adapterName is "{name}", standing
 * in for the GUID Windows uses. It receives 2 KiB/s and sends 1 KiB/s over a
 * 1 Gbps Ethernet link. Tests set any other field on the result.
 */
[[nodiscard]] inline domain::NetworkSample adapter(uint64_t interfaceId, std::string name,
                                                   domain::OperationalStatus status = domain::OperationalStatus::Up,
                                                   AdapterTraits traits = kPhysicalAdapter)
{
    return domain::NetworkSample{
        .interfaceId = interfaceId,
        .adapterName = "{" + name + "}",
        .friendlyName = std::move(name),
        .inBytesPerSec = 2048,
        .outBytesPerSec = 1024,
        .linkSpeedBps = 1'000'000'000,
        .operationalStatus = status,
        .type = domain::AdapterType::Ethernet,
        .isHardwareInterface = traits.isHardwareInterface,
        .hasIpInterface = traits.hasIpInterface,
    };
}

/** Returns sample with its receive and send rates replaced. */
[[nodiscard]] inline domain::NetworkSample
withRates(domain::NetworkSample sample, std::optional<uint64_t> inBytesPerSec, std::optional<uint64_t> outBytesPerSec)
{
    sample.inBytesPerSec = inBytesPerSec;
    sample.outBytesPerSec = outBytesPerSec;
    return sample;
}

} // namespace sysmon::tests
