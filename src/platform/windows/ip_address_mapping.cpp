#include "platform/windows/ip_address_mapping.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace sysmon::platform
{

namespace
{

// fec0:0:0:ffff::, which the three placeholder DNS servers share; they end in 1, 2 or 3.
constexpr std::array<uint8_t, 15> kPlaceholderDnsServerPrefix{0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

} // namespace

AddressState toAddressState(IP_DAD_STATE state) noexcept
{
    switch (state) {
        case IpDadStatePreferred:
            return AddressState::Preferred;
        case IpDadStateDeprecated:
            return AddressState::Deprecated;
        case IpDadStateTentative:
            return AddressState::Tentative;
        case IpDadStateDuplicate:
            return AddressState::Duplicate;
        case IpDadStateInvalid:
        default:
            return AddressState::Invalid;
    }
}

bool isPlaceholderDnsServer(const SOCKADDR* address) noexcept
{
    if (address == nullptr || address->sa_family != AF_INET6) {
        return false;
    }
    const IN6_ADDR& ipv6 = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
    const uint8_t lastByte = ipv6.u.Byte[15];
    return std::equal(kPlaceholderDnsServerPrefix.begin(), kPlaceholderDnsServerPrefix.end(), ipv6.u.Byte) &&
           lastByte >= 1 && lastByte <= 3;
}

} // namespace sysmon::platform
