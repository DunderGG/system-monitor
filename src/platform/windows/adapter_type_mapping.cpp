#include "platform/windows/adapter_type_mapping.h"

namespace sysmon::platform
{

domain::AdapterType toAdapterType(IFTYPE interfaceType, NDIS_PHYSICAL_MEDIUM physicalMedium) noexcept
{
    switch (physicalMedium) {
        case NdisPhysicalMediumNative802_11:
        case NdisPhysicalMediumWirelessLan:
            return domain::AdapterType::WiFi;
        case NdisPhysicalMediumBluetooth:
            return domain::AdapterType::Bluetooth;
        case NdisPhysicalMediumWirelessWan:
            return domain::AdapterType::MobileBroadband;
        case NdisPhysicalMedium802_3:
            return domain::AdapterType::Ethernet;
        default:
            break;
    }

    switch (interfaceType) {
        case IF_TYPE_ETHERNET_CSMACD:
            return domain::AdapterType::Ethernet;
        case IF_TYPE_IEEE80211:
            return domain::AdapterType::WiFi;
        case IF_TYPE_WWANPP:
        case IF_TYPE_WWANPP2:
            return domain::AdapterType::MobileBroadband;
        case IF_TYPE_TUNNEL:
            return domain::AdapterType::Tunnel;
        case IF_TYPE_PPP:
            return domain::AdapterType::Ppp;
        default:
            return domain::AdapterType::Other;
    }
}

} // namespace sysmon::platform
