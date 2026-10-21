#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <QString>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "support/adapter_lists.h"
#include "support/network_samples.h"
#include "support/readouts.h"
#include "ui/network_formatting.h"
#include "ui/performance_readouts.h"

using namespace verdandi::domain;
using namespace verdandi::ui;
using namespace verdandi::tests;

namespace
{

// Returns the interface IDs of adapters in their order.
std::vector<uint64_t> idsOf(const std::vector<NetworkSample>& adapters)
{
    std::vector<uint64_t> result;
    for (const auto& sample : adapters) {
        result.push_back(sample.interfaceId);
    }
    return result;
}

std::weak_ordering naturally(const char* left, const char* right)
{
    return compareNaturally(QString(left), QString(right));
}

} // namespace

TEST(NetworkFormatting, IsListedByDefault_NotHiddenAndHardwareOrIpBound)
{
    EXPECT_TRUE(isListedByDefault(adapter(1, "Ethernet", OperationalStatus::Down, kUnboundPhysicalAdapter)));
    EXPECT_TRUE(isListedByDefault(adapter(2, "Mullvad", OperationalStatus::Up, kVirtualAdapter)));
    EXPECT_FALSE(isListedByDefault(adapter(3, "WAN Miniport (IP)", OperationalStatus::Up, kUnboundVirtualAdapter)));
}

TEST(NetworkFormatting, IsListedByDefault_HiddenByWindows_NotListedEvenIfBoundToIp)
{
    // The Wi-Fi Direct virtual adapters are bound to IP, but Windows hides them.
    NetworkSample wifiDirect = adapter(1, "Local Area Connection* 1", OperationalStatus::Down, kVirtualAdapter);
    wifiDirect.isHidden = true;
    NetworkSample hiddenHardware =
        adapter(2, "Ethernet (Kernel Debugger)", OperationalStatus::NotPresent, kUnboundPhysicalAdapter);
    hiddenHardware.isHidden = true;

    EXPECT_FALSE(isListedByDefault(wifiDirect));
    EXPECT_FALSE(isListedByDefault(hiddenHardware));
    EXPECT_TRUE(
        isListedByDefault(adapter(3, "Bluetooth Network Connection", OperationalStatus::Down, kVirtualAdapter)));
}

TEST(NetworkFormatting, CompareNaturally_NumbersCompareByValue)
{
    EXPECT_EQ(naturally("Local Area Connection* 2", "Local Area Connection* 10"), std::weak_ordering::less);
    EXPECT_EQ(naturally("Local Area Connection* 11", "Local Area Connection* 9"), std::weak_ordering::greater);
    EXPECT_EQ(naturally("Ethernet 2", "Ethernet 10 (Hyper-V)"), std::weak_ordering::less);
    EXPECT_EQ(naturally("vEthernet 007", "vEthernet 7"), std::weak_ordering::equivalent);
    EXPECT_EQ(naturally("a0", "a00b"), std::weak_ordering::less);
}

TEST(NetworkFormatting, CompareNaturally_IgnoresCaseThenComparesText)
{
    EXPECT_EQ(naturally("wi-fi", "Wi-Fi"), std::weak_ordering::equivalent);
    EXPECT_EQ(naturally("bridge", "Ethernet"), std::weak_ordering::less);
    EXPECT_EQ(naturally("Ethernet", "Ethernet 2"), std::weak_ordering::less);
    EXPECT_EQ(naturally("Ethernet 2", "Ethernet"), std::weak_ordering::greater);
    EXPECT_EQ(naturally("WLAN 9", "WLAN a"), std::weak_ordering::less);
    EXPECT_EQ(naturally("", ""), std::weak_ordering::equivalent);
}

TEST(NetworkFormatting, IsListedBefore_ListedByDefaultFirstThenHardwareThenNaturalNameThenId)
{
    NetworkSample wifiDirect = adapter(9, "Local Area Connection* 1", OperationalStatus::Down, kVirtualAdapter);
    wifiDirect.isHidden = true;
    NetworkSample kernelDebugger =
        adapter(10, "Ethernet (Kernel Debugger)", OperationalStatus::NotPresent, kUnboundPhysicalAdapter);
    kernelDebugger.isHidden = true;
    std::vector<NetworkSample> adapters{
        adapter(1, "mullvad", OperationalStatus::Up, kVirtualAdapter),
        wifiDirect,
        adapter(2, "Wi-Fi", OperationalStatus::Up),
        adapter(7, "6to4 Adapter", OperationalStatus::NotPresent, kUnboundVirtualAdapter),
        adapter(3, "Ethernet", OperationalStatus::Down),
        adapter(5, "Local Area Connection* 10", OperationalStatus::Down, kVirtualAdapter),
        adapter(8, "Local Area Connection* 2", OperationalStatus::Down, kVirtualAdapter),
        kernelDebugger,
        adapter(4, "Local Area Connection* 2", OperationalStatus::Down, kVirtualAdapter),
        adapter(6, "Bluetooth", OperationalStatus::Up, kVirtualAdapter),
    };

    std::ranges::sort(adapters, isListedBefore);

    // Listed by default: hardware 3 and 2, then virtual 6, 4 and 8 (one name,
    // so by ID), 5 and 1. Then the others: hardware 10, then virtual 7 and 9.
    EXPECT_EQ(idsOf(adapters), (std::vector<uint64_t>{3, 2, 6, 4, 8, 5, 1, 10, 7, 9}));
}

TEST(NetworkFormatting, IsListedBefore_StatusDoesNotChangeTheOrder)
{
    const NetworkSample wifi = adapter(1, "Wi-Fi", OperationalStatus::Up);
    NetworkSample ethernet = adapter(2, "Ethernet");

    for (const auto status : {OperationalStatus::Up, OperationalStatus::Down, OperationalStatus::NotPresent}) {
        SCOPED_TRACE(operationalStatusText(status).toStdString());
        ethernet.operationalStatus = status;

        EXPECT_TRUE(isListedBefore(ethernet, wifi));
        EXPECT_FALSE(isListedBefore(wifi, ethernet));
    }
}

TEST(NetworkFormatting, ListedAdapters_HidesHiddenAndUnboundVirtualAdaptersUnlessShowAll)
{
    NetworkSample wifiDirect = adapter(4, "Local Area Connection* 1", OperationalStatus::Down, kVirtualAdapter);
    wifiDirect.isHidden = true;
    const std::vector<NetworkSample> networks{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, kUnboundVirtualAdapter),
        adapter(3, "Teredo Tunneling Pseudo-Interface", OperationalStatus::NotPresent, kUnboundVirtualAdapter),
        wifiDirect,
        adapter(5, "Bluetooth Network Connection", OperationalStatus::Down, kVirtualAdapter),
    };

    EXPECT_EQ(sortedInterfaceIds(listedAdapters(networks, false)), (std::vector<uint64_t>{1, 5}));
    EXPECT_EQ(sortedInterfaceIds(listedAdapters(networks, true)), (std::vector<uint64_t>{1, 2, 3, 4, 5}));
}

TEST(NetworkFormatting, ListedAdapters_InIsListedBeforeOrder)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "WAN Miniport (IP)", OperationalStatus::Up, kUnboundVirtualAdapter),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
        adapter(3, "Bluetooth Network Connection", OperationalStatus::Down, kVirtualAdapter),
        adapter(4, "Ethernet", OperationalStatus::Down),
    };
    ASSERT_FALSE(std::ranges::is_sorted(networks, isListedBefore));

    for (const bool showAll : {false, true}) {
        SCOPED_TRACE(showAll ? "show all" : "listed by default");

        EXPECT_TRUE(isInListedOrder(listedAdapters(networks, showAll)));
    }
}

TEST(NetworkFormatting, ListedAdapters_NoAdapters_Empty)
{
    EXPECT_TRUE(listedAdapters({}, false).empty());
}

TEST(NetworkFormatting, OperationalStatusText_NamesEveryStatus)
{
    EXPECT_EQ(operationalStatusText(OperationalStatus::Up), "Up");
    EXPECT_EQ(operationalStatusText(OperationalStatus::Down), "Down");
    EXPECT_EQ(operationalStatusText(OperationalStatus::Testing), "Testing");
    EXPECT_EQ(operationalStatusText(OperationalStatus::Unknown), "Unknown");
    EXPECT_EQ(operationalStatusText(OperationalStatus::Dormant), "Dormant");
    EXPECT_EQ(operationalStatusText(OperationalStatus::NotPresent), "Not present");
    EXPECT_EQ(operationalStatusText(OperationalStatus::LowerLayerDown), "Lower layer down");
}

TEST(NetworkFormatting, AdapterTypeText_HardwareAdapters_NameTheirLink)
{
    NetworkSample sample{.isHardwareInterface = true};

    sample.type = AdapterType::Ethernet;
    EXPECT_EQ(adapterTypeText(sample), "Ethernet");
    sample.type = AdapterType::WiFi;
    EXPECT_EQ(adapterTypeText(sample), "Wi-Fi");
    sample.type = AdapterType::MobileBroadband;
    EXPECT_EQ(adapterTypeText(sample), "Mobile broadband");
    sample.type = AdapterType::Other;
    EXPECT_EQ(adapterTypeText(sample), "Other");
}

TEST(NetworkFormatting, AdapterTypeText_VirtualAdapters_MarkedVirtual)
{
    NetworkSample sample{.isHardwareInterface = false};

    sample.type = AdapterType::Ethernet;
    EXPECT_EQ(adapterTypeText(sample), "Ethernet (virtual)");
    sample.type = AdapterType::WiFi;
    EXPECT_EQ(adapterTypeText(sample), "Wi-Fi (virtual)");
    sample.type = AdapterType::Bluetooth;
    EXPECT_EQ(adapterTypeText(sample), "Bluetooth (virtual)");
    sample.type = AdapterType::Other;
    EXPECT_EQ(adapterTypeText(sample), "Virtual");
    sample.type = AdapterType::Tunnel;
    EXPECT_EQ(adapterTypeText(sample), "Tunnel");
    sample.type = AdapterType::Ppp;
    EXPECT_EQ(adapterTypeText(sample), "PPP");
}

TEST(NetworkFormatting, AddressTexts_SplitByFamilyOnePerLine)
{
    const NetworkSample sample{
        .ipAddresses = {{.text = "192.168.1.20"}, {.text = "fe80::1"}, {.text = "10.0.0.5"}, {.text = "2001:db8::5"}}};

    EXPECT_EQ(ipv4AddressText(sample), "192.168.1.20\n10.0.0.5");
    EXPECT_EQ(ipv6AddressText(sample), "fe80::1\n2001:db8::5");
    EXPECT_EQ(ipv4AddressText(NetworkSample{}), "None");
    EXPECT_EQ(formatAddressList({}), "None");
}

TEST(NetworkFormatting, AddressTexts_TemporaryAndLinkLocalMarkedAsIpconfigNamesThem)
{
    const NetworkSample sample{.ipAddresses = {
                                   {.text = "fd44:555c:2315:4176:c6d0:77e0:5d11:3727"},
                                   {.text = "fd44:555c:2315:4176:483d:5f94:693b:1ea5", .isTemporary = true},
                                   {.text = "fe80::d347:c091:2487:3b32%16", .isLinkLocal = true},
                                   {.text = "169.254.113.81", .isLinkLocal = true},
                               }};

    EXPECT_EQ(ipv6AddressText(sample), "fd44:555c:2315:4176:c6d0:77e0:5d11:3727\n"
                                       "fd44:555c:2315:4176:483d:5f94:693b:1ea5 (temporary)\n"
                                       "fe80::d347:c091:2487:3b32%16 (link-local)");
    EXPECT_EQ(ipv4AddressText(sample), "169.254.113.81 (link-local)");
}

TEST(NetworkFormatting, AdapterDetailReadouts_VirtualAdapter_LinkSpeedHasATooltip)
{
    NetworkSample vpn{
        .interfaceId = 3,
        .linkSpeedBps = 100'000'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
        .isHardwareInterface = false,
        .hasIpInterface = true,
    };

    const auto readouts = adapterDetailReadouts(vpn, std::nullopt);
    const auto linkSpeed = std::ranges::find(readouts, QString("Link speed"), &Readout::name);
    ASSERT_NE(linkSpeed, readouts.end());
    EXPECT_EQ(linkSpeed->value, "100 Gbps");
    EXPECT_EQ(linkSpeed->toolTip, linkSpeedToolTip(vpn));
    EXPECT_FALSE(linkSpeed->toolTip.isEmpty());

    vpn.isHardwareInterface = true;
    EXPECT_TRUE(linkSpeedToolTip(vpn).isEmpty());
}

TEST(NetworkFormatting, AdapterDetailReadouts_UpAdapter_ShowsNetworkLinkSpeedAndAddresses)
{
    const NetworkSample sample{
        .interfaceId = 7,
        .linkSpeedBps = 2'500'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
        .type = AdapterType::Ethernet,
        .ipAddresses = {{.text = "192.168.1.20"}, {.text = "fe80::1"}},
        .dnsServers = {"192.168.1.1", "1.1.1.1"},
        .isHardwareInterface = true,
    };
    const std::vector<NetworkProfile> profiles{
        {.name = "Normandy", .category = NetworkCategory::Private, .interfaceIds = {7}},
    };

    EXPECT_EQ(adapterDetailReadouts(sample, profiles), (std::vector<Readout>{
                                                           {"Network", "Normandy (Private)"},
                                                           {"Type", "Ethernet"},
                                                           {"Link speed", "2.5 Gbps"},
                                                           {"IPv4 address", "192.168.1.20"},
                                                           {"IPv6 address", "fe80::1"},
                                                           {"DNS servers", "192.168.1.1\n1.1.1.1"},
                                                       }));
}

TEST(NetworkFormatting, AdapterDetailReadouts_DisconnectedAdapter_LinkSpeedNotConnected)
{
    const NetworkSample sample{.linkSpeedBps = 144'400'000, .operationalStatus = OperationalStatus::Down};

    EXPECT_EQ(valueOf(adapterDetailReadouts(sample, std::nullopt), "Link speed"), "Not connected");
}

// What Windows reports for a disconnected Bluetooth PAN adapter: a Tentative
// automatic private address, a Deprecated link-local address, and the
// site-local placeholder DNS servers. The collector drops all of them, but a
// disconnected adapter shows no addresses or DNS servers whatever it reports.
TEST(NetworkFormatting, AdapterDetailReadouts_NotUp_AddressesAndDnsServersNotConnected)
{
    NetworkSample sample{
        .linkSpeedBps = 3'000'000,
        .type = AdapterType::Bluetooth,
        .ipAddresses = {{.text = "169.254.113.81"}, {.text = "fe80::bc71:942f:7fb6:cbaa"}},
        .dnsServers = {"fec0:0:0:ffff::1", "fec0:0:0:ffff::2", "fec0:0:0:ffff::3"},
        .isHardwareInterface = false,
    };

    for (const auto status :
         {OperationalStatus::Down, OperationalStatus::Testing, OperationalStatus::Unknown, OperationalStatus::Dormant,
          OperationalStatus::NotPresent, OperationalStatus::LowerLayerDown}) {
        SCOPED_TRACE(operationalStatusText(status).toStdString());
        sample.operationalStatus = status;

        EXPECT_EQ(adapterDetailReadouts(sample, std::vector<NetworkProfile>{}), (std::vector<Readout>{
                                                                                    {"Network", "None"},
                                                                                    {"Type", "Bluetooth (virtual)"},
                                                                                    {"Link speed", "Not connected"},
                                                                                    {"IPv4 address", "Not connected"},
                                                                                    {"IPv6 address", "Not connected"},
                                                                                    {"DNS servers", "Not connected"},
                                                                                }));
    }
}

TEST(NetworkFormatting, AdapterDetailReadouts_UpWithoutAddresses_None)
{
    const NetworkSample sample{.linkSpeedBps = 1'000'000'000, .operationalStatus = OperationalStatus::Up};
    const auto readouts = adapterDetailReadouts(sample, std::nullopt);

    EXPECT_EQ(valueOf(readouts, "IPv4 address"), "None");
    EXPECT_EQ(valueOf(readouts, "IPv6 address"), "None");
    EXPECT_EQ(valueOf(readouts, "DNS servers"), "None");
}

TEST(NetworkFormatting, AdapterDetailReadouts_UpWithoutReportedSpeed_LinkSpeedNotAvailable)
{
    const NetworkSample sample{.linkSpeedBps = 0, .operationalStatus = OperationalStatus::Up};

    EXPECT_EQ(valueOf(adapterDetailReadouts(sample, std::nullopt), "Link speed"), "N/A");
}

TEST(NetworkFormatting, AdapterDetailReadouts_ReadFailed_SameNamesAllNotAvailable)
{
    const auto unavailable = adapterDetailReadouts(std::nullopt, std::nullopt);
    const auto available = adapterDetailReadouts(NetworkSample{}, std::vector<NetworkProfile>{});

    ASSERT_EQ(unavailable.size(), available.size());
    for (std::size_t index = 0; index < unavailable.size(); ++index) {
        EXPECT_EQ(unavailable[index].name, available[index].name);
        EXPECT_EQ(unavailable[index].value, "N/A");
    }
}

TEST(NetworkFormatting, NetworkProfileText_NameWithCategoryWhenKnown)
{
    EXPECT_EQ(networkProfileText({.name = "Home-Wi-Fi", .category = NetworkCategory::Private}), "Home-Wi-Fi (Private)");
    EXPECT_EQ(networkProfileText({.name = "Airport", .category = NetworkCategory::Public}), "Airport (Public)");
    EXPECT_EQ(networkProfileText({.name = "corp.example.com", .category = NetworkCategory::DomainAuthenticated}),
              "corp.example.com (Domain)");
    EXPECT_EQ(networkProfileText({.name = "Network 2", .category = std::nullopt}), "Network 2");
}

TEST(NetworkFormatting, AdapterNetworkText_MatchesProfilesByInterfaceId)
{
    const std::vector<NetworkProfile> profiles{
        {.name = "Normandy", .category = NetworkCategory::Private, .interfaceIds = {1}},
        {.name = "Mullvad", .category = NetworkCategory::Private, .interfaceIds = {2}},
        {.name = "Lab", .category = std::nullopt, .interfaceIds = {1, 3}},
    };

    EXPECT_EQ(adapterNetworkText(1, profiles), "Normandy (Private)\nLab");
    EXPECT_EQ(adapterNetworkText(2, profiles), "Mullvad (Private)");
    EXPECT_EQ(adapterNetworkText(4, profiles), "None");
    EXPECT_EQ(adapterNetworkText(1, std::nullopt), "N/A");
}

TEST(NetworkFormatting, ConnectivityReadouts_KnownState_LevelCostRoamingAndNetworks)
{
    const ConnectivityStatus status{.level = ConnectivityLevel::InternetAccess, .isMetered = false, .isRoaming = false};
    const std::vector<NetworkProfile> profiles{
        {.name = "Normandy", .category = NetworkCategory::Private, .interfaceIds = {1}},
        {.name = "Mullvad", .category = NetworkCategory::Public, .interfaceIds = {2}},
    };

    EXPECT_EQ(connectivityReadouts(status, profiles), (std::vector<Readout>{
                                                          {"Connectivity", "Internet access"},
                                                          {"Cost", "Unmetered"},
                                                          {"Roaming", "No"},
                                                          {"Networks", "Normandy (Private)\nMullvad (Public)"},
                                                      }));
}

TEST(NetworkFormatting, ConnectivityReadouts_MeteredAndRoamingWithNoNetworks)
{
    const ConnectivityStatus status{
        .level = ConnectivityLevel::ConstrainedInternetAccess, .isMetered = true, .isRoaming = true};

    EXPECT_EQ(connectivityReadouts(status, std::vector<NetworkProfile>{}),
              (std::vector<Readout>{
                  {"Connectivity", "Limited internet (captive portal)"},
                  {"Cost", "Metered"},
                  {"Roaming", "Yes"},
                  {"Networks", "None"},
              }));
}

TEST(NetworkFormatting, ConnectivityReadouts_UnknownState_SaysUnknownNotOfflineOrUnmetered)
{
    EXPECT_EQ(connectivityReadouts(ConnectivityStatus{}, std::nullopt), (std::vector<Readout>{
                                                                            {"Connectivity", "Unknown"},
                                                                            {"Cost", "Unknown"},
                                                                            {"Roaming", "Unknown"},
                                                                            {"Networks", "N/A"},
                                                                        }));
}
