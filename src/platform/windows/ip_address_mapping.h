#pragma once

#include <optional>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include "platform/windows/unicast_address.h"

namespace sysmon::platform
{

// Pure conversions of the addresses GetAdaptersAddresses reports.
// Internal to platform/windows: exposed in a header only so the mapping can be
// unit tested without calling the OS, by tests/platform/ only.
// Do not include from other modules or from tests/unit/.

/**
 * Maps a unicast address's duplicate address detection state to its
 * AddressState. A value this mapping does not know is Invalid, so an address
 * in a state that may not be usable is never shown as one.
 */
[[nodiscard]] AddressState toAddressState(IP_DAD_STATE state) noexcept;

/**
 * Returns true for fec0:0:0:ffff::1, ::2 and ::3, whatever the scope ID.
 * Windows lists these site-local addresses, from an expired draft for finding
 * DNS servers over IPv6, as the DNS servers of an adapter that has none
 * configured. RFC 3879 deprecated site-local addresses, so no network uses
 * them. Returns false for null, IPv4, and every other address.
 */
[[nodiscard]] bool isPlaceholderDnsServer(const SOCKADDR* address) noexcept;

/**
 * Formats an IPv4 or IPv6 socket address as text. A link-local IPv6 address
 * with a scope ID gets it as its zone index, e.g. "fe80::1%16", as ipconfig
 * shows it. Returns std::nullopt for null, other families, or on failure.
 */
[[nodiscard]] std::optional<std::string> formatIpAddress(const SOCKADDR* address);

/** Returns true for a link-local address: IPv6 fe80::/10, or IPv4 169.254.0.0/16. */
[[nodiscard]] bool isLinkLocalAddress(const SOCKADDR* address) noexcept;

/**
 * Returns true if a unicast address with this suffix origin is a temporary
 * IPv6 address. Windows reports a temporary address's suffix as random, and a
 * stable address's as derived from the link-layer address even when its
 * identifier is randomized; netsh shows the two as Temporary and Public.
 */
[[nodiscard]] bool isTemporaryAddress(IP_SUFFIX_ORIGIN suffixOrigin) noexcept;

} // namespace sysmon::platform
