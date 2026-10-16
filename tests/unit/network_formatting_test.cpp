#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QString>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "ui/network_formatting.h"
#include "ui/performance_readouts.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

NetworkSample adapter(uint64_t interfaceId, std::string name, OperationalStatus status, bool isHardware = true,
                      bool hasIpInterface = true)
{
    return NetworkSample{
        .interfaceId = interfaceId,
        .adapterName = name,
        .friendlyName = std::move(name),
        .operationalStatus = status,
        .isHardwareInterface = isHardware,
        .hasIpInterface = hasIpInterface,
    };
}

std::vector<uint64_t> ids(const std::vector<const NetworkSample*>& adapters)
{
    std::vector<uint64_t> result;
    for (const auto* sample : adapters) {
        result.push_back(sample->interfaceId);
    }
    return result;
}

QString valueOf(const std::vector<Readout>& readouts, const QString& name)
{
    const auto it = std::ranges::find(readouts, name, &Readout::name);
    return it != readouts.end() ? it->value : QString("<missing>");
}

} // namespace

TEST(NetworkFormatting, IsListedByDefault_HardwareOrIpBound)
{
    EXPECT_TRUE(isListedByDefault(adapter(1, "Ethernet", OperationalStatus::Down, true, false)));
    EXPECT_TRUE(isListedByDefault(adapter(2, "Mullvad", OperationalStatus::Up, false, true)));
    EXPECT_FALSE(isListedByDefault(adapter(3, "WAN Miniport (IP)", OperationalStatus::Up, false, false)));
}

TEST(NetworkFormatting, ListedAdapters_HidesUnboundVirtualAdaptersUnlessShowAll)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, false, false),
        adapter(3, "Teredo Tunneling Pseudo-Interface", OperationalStatus::NotPresent, false, false),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{1}));
    EXPECT_EQ(ids(listedAdapters(networks, true)), (std::vector<uint64_t>{1, 2, 3}));
}

TEST(NetworkFormatting, ListedAdapters_DownAdaptersListedAfterActiveOnes)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "Wi-Fi", OperationalStatus::Down),
        adapter(2, "Ethernet", OperationalStatus::Up),
        adapter(3, "Bluetooth Network Connection", OperationalStatus::LowerLayerDown, false, true),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{2, 1, 3}));
}

TEST(NetworkFormatting, ListedAdapters_UpThenHardwareThenNameThenId)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "mullvad", OperationalStatus::Up, false, true),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
        adapter(3, "Ethernet", OperationalStatus::Up),
        adapter(5, "Local Area Connection* 1", OperationalStatus::Down, false, true),
        adapter(4, "Local Area Connection* 1", OperationalStatus::Down, false, true),
        adapter(6, "Bluetooth", OperationalStatus::Up, false, true),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{3, 2, 6, 1, 4, 5}));
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
    const NetworkSample sample{.ipAddresses = {"192.168.1.20", "fe80::1", "10.0.0.5", "2001:db8::5"}};

    EXPECT_EQ(ipv4AddressText(sample), "192.168.1.20\n10.0.0.5");
    EXPECT_EQ(ipv6AddressText(sample), "fe80::1\n2001:db8::5");
    EXPECT_EQ(ipv4AddressText(NetworkSample{}), "None");
    EXPECT_EQ(formatAddressList({}), "None");
}

TEST(NetworkFormatting, AdapterDetailReadouts_UpAdapter_ShowsNetworkLinkSpeedAndAddresses)
{
    const NetworkSample sample{
        .interfaceId = 7,
        .linkSpeedBps = 2'500'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
        .type = AdapterType::Ethernet,
        .ipAddresses = {"192.168.1.20", "fe80::1"},
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
