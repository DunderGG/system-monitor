#include <algorithm>
#include <array>
#include <cstdint>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <gtest/gtest.h>

#include "platform/windows/ip_address_mapping.h"
#include "platform/windows/unicast_address.h"

using namespace sysmon::platform;

namespace
{

sockaddr_in6 ipv6Address(const std::array<uint8_t, 16>& bytes, ULONG scopeId = 0)
{
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    std::ranges::copy(bytes, address.sin6_addr.u.Byte);
    address.sin6_scope_id = scopeId;
    return address;
}

// fec0:0:0:ffff::<last>
sockaddr_in6 siteLocalDnsAddress(uint8_t last, ULONG scopeId = 0)
{
    return ipv6Address({0xfe, 0xc0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, last}, scopeId);
}

bool isPlaceholder(const sockaddr_in6& address)
{
    return isPlaceholderDnsServer(reinterpret_cast<const SOCKADDR*>(&address));
}

} // namespace

TEST(IpAddressMapping, ToAddressState_EachDadState_MapsToTheSameState)
{
    EXPECT_EQ(toAddressState(IpDadStatePreferred), AddressState::Preferred);
    EXPECT_EQ(toAddressState(IpDadStateDeprecated), AddressState::Deprecated);
    EXPECT_EQ(toAddressState(IpDadStateTentative), AddressState::Tentative);
    EXPECT_EQ(toAddressState(IpDadStateDuplicate), AddressState::Duplicate);
    EXPECT_EQ(toAddressState(IpDadStateInvalid), AddressState::Invalid);
}

TEST(IpAddressMapping, ToAddressState_UnknownValue_ReturnsInvalid)
{
    // The largest value the enumeration can hold, which names no state.
    EXPECT_EQ(toAddressState(static_cast<IP_DAD_STATE>(7)), AddressState::Invalid);
}

TEST(IpAddressMapping, IsPlaceholderDnsServer_SiteLocalDefaults_ReturnsTrue)
{
    EXPECT_TRUE(isPlaceholder(siteLocalDnsAddress(1)));
    EXPECT_TRUE(isPlaceholder(siteLocalDnsAddress(2)));
    EXPECT_TRUE(isPlaceholder(siteLocalDnsAddress(3)));
    // ipconfig shows them with the adapter's scope ID, e.g. fec0:0:0:ffff::1%4.
    EXPECT_TRUE(isPlaceholder(siteLocalDnsAddress(1, 4)));
}

TEST(IpAddressMapping, IsPlaceholderDnsServer_OtherIpv6Addresses_ReturnsFalse)
{
    EXPECT_FALSE(isPlaceholder(siteLocalDnsAddress(0)));
    EXPECT_FALSE(isPlaceholder(siteLocalDnsAddress(4)));
    // fec0::1, without the ffff group.
    EXPECT_FALSE(isPlaceholder(ipv6Address({0xfe, 0xc0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})));
    // 2001:4860:4860::8888
    EXPECT_FALSE(isPlaceholder(ipv6Address({0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88})));
}

TEST(IpAddressMapping, IsPlaceholderDnsServer_Ipv4OrNull_ReturnsFalse)
{
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_addr.S_un.S_un_b = {192, 168, 1, 1};

    EXPECT_FALSE(isPlaceholderDnsServer(reinterpret_cast<const SOCKADDR*>(&ipv4)));
    EXPECT_FALSE(isPlaceholderDnsServer(nullptr));
}
