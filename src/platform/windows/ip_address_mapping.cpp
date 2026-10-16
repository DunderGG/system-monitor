#include "platform/windows/ip_address_mapping.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

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

std::optional<std::string> formatIpAddress(const SOCKADDR* address)
{
    if (address == nullptr) {
        return std::nullopt;
    }

    std::array<char, INET6_ADDRSTRLEN> text{};
    if (address->sa_family == AF_INET) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr) {
            return std::nullopt;
        }
        return std::string(text.data());
    }
    if (address->sa_family == AF_INET6) {
        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
        if (inet_ntop(AF_INET6, &ipv6->sin6_addr, text.data(), text.size()) == nullptr) {
            return std::nullopt;
        }
        std::string formatted(text.data());
        if (IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr) && ipv6->sin6_scope_id != 0) {
            formatted += '%' + std::to_string(ipv6->sin6_scope_id);
        }
        return formatted;
    }
    return std::nullopt;
}

bool isLinkLocalAddress(const SOCKADDR* address) noexcept
{
    if (address == nullptr) {
        return false;
    }
    if (address->sa_family == AF_INET) {
        const IN_ADDR& ipv4 = reinterpret_cast<const sockaddr_in*>(address)->sin_addr;
        return ipv4.S_un.S_un_b.s_b1 == 169 && ipv4.S_un.S_un_b.s_b2 == 254;
    }
    if (address->sa_family == AF_INET6) {
        return IN6_IS_ADDR_LINKLOCAL(&reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr) != FALSE;
    }
    return false;
}

bool isTemporaryAddress(IP_SUFFIX_ORIGIN suffixOrigin) noexcept
{
    return suffixOrigin == IpSuffixOriginRandom;
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
