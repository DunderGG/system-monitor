#pragma once

#include <string>

namespace verdandi::platform
{

/**
 * The state of a unicast address, from duplicate address detection
 * (IP_DAD_STATE in GetAdaptersAddresses). Only a Preferred address is in
 * normal use. A Deprecated address can still carry the connections that
 * already use it, such as an expired temporary IPv6 address, but Windows also
 * keeps the addresses of an adapter whose media is disconnected as Deprecated.
 * Tentative, Duplicate and Invalid addresses cannot be used: the automatic
 * private (169.254) address of a disconnected adapter stays Tentative.
 */
enum class AddressState
{
    Preferred,
    Deprecated,
    Tentative,
    Duplicate,
    Invalid,
};

/**
 * A unicast address of an adapter, as text (with the zone index of a
 * link-local IPv6 address), with its state, and whether it is temporary or
 * link-local (see domain::IpAddress).
 */
struct UnicastAddress
{
    std::string address{};
    AddressState state{AddressState::Preferred};
    bool isTemporary{false};
    bool isLinkLocal{false};

    bool operator==(const UnicastAddress&) const = default;
};

} // namespace verdandi::platform
