#pragma once

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

} // namespace sysmon::platform
