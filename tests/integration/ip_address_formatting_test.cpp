#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <gtest/gtest.h>

#include "platform/windows/ip_address_mapping.h"

// formatIpAddress() calls inet_ntop, a Windows API, so its tests are
// integration tests. They need no particular host state.

using namespace verdandi::platform;

namespace
{

std::optional<std::string> formatIpv6(const std::array<uint8_t, 16>& bytes, ULONG scopeId)
{
    sockaddr_in6 address{};
    address.sin6_family = AF_INET6;
    std::ranges::copy(bytes, address.sin6_addr.u.Byte);
    address.sin6_scope_id = scopeId;
    return formatIpAddress(reinterpret_cast<const SOCKADDR*>(&address));
}

} // namespace

TEST(IpAddressFormatting, LinkLocalIpv6_HasItsZoneIndex)
{
    EXPECT_EQ(formatIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0xd3, 0x47, 0xc0, 0x91, 0x24, 0x87, 0x3b, 0x32}, 16),
              std::optional<std::string>{"fe80::d347:c091:2487:3b32%16"});
}

TEST(IpAddressFormatting, OtherIpv6_NoZoneIndexEvenWithAScopeId)
{
    EXPECT_EQ(
        formatIpv6({0xfd, 0x44, 0x55, 0x5c, 0x23, 0x15, 0x41, 0x76, 0xc6, 0xd0, 0x77, 0xe0, 0x5d, 0x11, 0x37, 0x27}, 0),
        std::optional<std::string>{"fd44:555c:2315:4176:c6d0:77e0:5d11:3727"});
    // Windows gives the placeholder DNS servers a scope ID, which ipconfig shows, but they are not link-local.
    EXPECT_EQ(formatIpv6({0xfe, 0xc0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 1}, 4),
              std::optional<std::string>{"fec0:0:0:ffff::1"});
}

TEST(IpAddressFormatting, LinkLocalIpv6WithoutScopeId_NoZoneIndex)
{
    EXPECT_EQ(formatIpv6({0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 0),
              std::optional<std::string>{"fe80::1"});
}

TEST(IpAddressFormatting, Ipv4_DottedQuad)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.S_un.S_un_b = {192, 168, 1, 119};

    EXPECT_EQ(formatIpAddress(reinterpret_cast<const SOCKADDR*>(&address)),
              std::optional<std::string>{"192.168.1.119"});
}

TEST(IpAddressFormatting, NullOrOtherFamily_Nullopt)
{
    SOCKADDR other{};
    other.sa_family = AF_UNSPEC;

    EXPECT_FALSE(formatIpAddress(nullptr).has_value());
    EXPECT_FALSE(formatIpAddress(&other).has_value());
}
