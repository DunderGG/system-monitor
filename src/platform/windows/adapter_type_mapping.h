#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include "domain/network_sample.h"

namespace verdandi::platform
{

// Pure conversion from GetIfTable2 interface types to domain types.
// Internal to platform/windows: exposed in a header only so the mapping can be
// unit tested without calling the OS, by tests/platform/ only.
// Do not include from other modules or from tests/unit/.

/**
 * Maps an interface's physical medium and interface type (MIB_IF_ROW2's
 * PhysicalMediumType and Type) to the kind of link. The physical medium
 * decides when it names one: a Bluetooth PAN adapter has the Ethernet
 * interface type. Otherwise the interface type decides, and types with no
 * category, such as IF_TYPE_PROP_VIRTUAL, map to AdapterType::Other.
 */
[[nodiscard]] domain::AdapterType toAdapterType(IFTYPE interfaceType, NDIS_PHYSICAL_MEDIUM physicalMedium) noexcept;

} // namespace verdandi::platform
