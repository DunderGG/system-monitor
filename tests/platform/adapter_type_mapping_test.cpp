#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "platform/windows/adapter_type_mapping.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(AdapterTypeMapping, ToAdapterType_EthernetMedium_ReturnsEthernet)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMedium802_3), AdapterType::Ethernet);
}

TEST(AdapterTypeMapping, ToAdapterType_WiFiMedium_ReturnsWiFi)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_IEEE80211, NdisPhysicalMediumNative802_11), AdapterType::WiFi);
    EXPECT_EQ(toAdapterType(IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumWirelessLan), AdapterType::WiFi);
}

TEST(AdapterTypeMapping, ToAdapterType_BluetoothPanWithEthernetInterfaceType_ReturnsBluetooth)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumBluetooth), AdapterType::Bluetooth);
}

TEST(AdapterTypeMapping, ToAdapterType_WirelessWanMediumOrInterfaceType_ReturnsMobileBroadband)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_WWANPP, NdisPhysicalMediumWirelessWan), AdapterType::MobileBroadband);
    EXPECT_EQ(toAdapterType(IF_TYPE_WWANPP2, NdisPhysicalMediumUnspecified), AdapterType::MobileBroadband);
}

TEST(AdapterTypeMapping, ToAdapterType_UnspecifiedMedium_UsesInterfaceType)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_ETHERNET_CSMACD, NdisPhysicalMediumUnspecified), AdapterType::Ethernet);
    EXPECT_EQ(toAdapterType(IF_TYPE_IEEE80211, NdisPhysicalMediumUnspecified), AdapterType::WiFi);
    EXPECT_EQ(toAdapterType(IF_TYPE_TUNNEL, NdisPhysicalMediumUnspecified), AdapterType::Tunnel);
    EXPECT_EQ(toAdapterType(IF_TYPE_PPP, NdisPhysicalMediumUnspecified), AdapterType::Ppp);
}

TEST(AdapterTypeMapping, ToAdapterType_ProprietaryVirtualOrOther_ReturnsOther)
{
    EXPECT_EQ(toAdapterType(IF_TYPE_PROP_VIRTUAL, NdisPhysicalMediumUnspecified), AdapterType::Other);
    EXPECT_EQ(toAdapterType(IF_TYPE_OTHER, NdisPhysicalMediumUnspecified), AdapterType::Other);
    EXPECT_EQ(toAdapterType(IF_TYPE_IEEE1394, NdisPhysicalMedium1394), AdapterType::Other);
}
