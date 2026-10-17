#include <array>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "platform/windows/adapter_type_mapping.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(AdapterTypeMapping, ToAdapterType_PhysicalMediumFirstThenInterfaceType)
{
    struct Case
    {
        const char* name;
        IFTYPE interfaceType;
        NDIS_PHYSICAL_MEDIUM medium;
        sysmon::domain::AdapterType expected;
    };
    const std::array cases{
        Case{"Ethernet medium", IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMedium802_3, AdapterType::Ethernet},
        Case{"native Wi-Fi medium", IF_TYPE_IEEE80211, NdisPhysicalMediumNative802_11, AdapterType::WiFi},
        Case{"wireless LAN medium, Ethernet interface type", IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumWirelessLan,
             AdapterType::WiFi},
        Case{"Bluetooth PAN, Ethernet interface type", IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumBluetooth,
             AdapterType::Bluetooth},
        Case{"wireless WAN medium", IF_TYPE_WWANPP, NdisPhysicalMediumWirelessWan, AdapterType::MobileBroadband},
        Case{"wireless WAN interface type, unspecified medium", IF_TYPE_WWANPP2, NdisPhysicalMediumUnspecified,
             AdapterType::MobileBroadband},
        Case{"Ethernet interface type, unspecified medium", IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumUnspecified,
             AdapterType::Ethernet},
        Case{"Wi-Fi interface type, unspecified medium", IF_TYPE_IEEE80211, NdisPhysicalMediumUnspecified,
             AdapterType::WiFi},
        Case{"tunnel", IF_TYPE_TUNNEL, NdisPhysicalMediumUnspecified, AdapterType::Tunnel},
        Case{"PPP", IF_TYPE_PPP, NdisPhysicalMediumUnspecified, AdapterType::Ppp},
        Case{"proprietary virtual", IF_TYPE_PROP_VIRTUAL, NdisPhysicalMediumUnspecified, AdapterType::Other},
        Case{"other", IF_TYPE_OTHER, NdisPhysicalMediumUnspecified, AdapterType::Other},
        Case{"IEEE 1394", IF_TYPE_IEEE1394, NdisPhysicalMedium1394, AdapterType::Other},
    };

    for (const auto& [name, interfaceType, medium, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(toAdapterType(interfaceType, medium), expected);
    }
}
