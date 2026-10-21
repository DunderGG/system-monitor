#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "platform/windows/network_collector.h"
#include "platform/windows/unicast_address.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

std::vector<std::string> texts(const std::vector<IpAddress>& addresses)
{
    std::vector<std::string> result;
    for (const auto& address : addresses) {
        result.push_back(address.text);
    }
    return result;
}

} // namespace

TEST(NetworkCollector, CalculateNetworkSamples_FirstTick_RatesUnavailableAndBaselineRecorded)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

    const std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .ifIndex = 1,
        .adapterName = "Ethernet",
        .friendlyName = "Ethernet",
        .description = "Intel Gigabit",
        .inBytesTotal = 50'000,
        .outBytesTotal = 25'000,
        .linkSpeedBps = 1'000'000'000,
        .operationalStatus = OperationalStatus::Up,
        .ipAddresses = {{.address = "192.168.1.10"}},
        .dnsServers = {"1.1.1.1"},
        .isLoopback = false,
    }};

    const auto samples = calculateNetworkSamples(adapters, baselines, t0);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Ethernet");
    EXPECT_EQ(samples[0].inBytesTotal, 50'000u);
    EXPECT_EQ(samples[0].outBytesTotal, 25'000u);
    EXPECT_FALSE(samples[0].inBytesPerSec.has_value());
    EXPECT_FALSE(samples[0].outBytesPerSec.has_value());
    EXPECT_EQ(samples[0].linkSpeedBps, 1'000'000'000u);
    EXPECT_EQ(samples[0].operationalStatus, OperationalStatus::Up);

    ASSERT_TRUE(baselines.contains(101));
    EXPECT_EQ(baselines[101].inBytes, 50'000u);
    EXPECT_EQ(baselines[101].outBytes, 25'000u);
    EXPECT_EQ(baselines[101].timestamp, t0);
}

TEST(NetworkCollector, CalculateNetworkSamples_SecondTick_CalculatesAccurateThroughput)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{102}}; // 2.0 seconds elapsed

    std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .ifIndex = 1,
        .adapterName = "Ethernet",
        .friendlyName = "Ethernet",
        .description = "Intel Gigabit",
        .inBytesTotal = 100'000,
        .outBytesTotal = 50'000,
        .linkSpeedBps = 1'000'000'000,
        .operationalStatus = OperationalStatus::Up,
        .isLoopback = false,
    }};

    // First tick to set baseline
    calculateNetworkSamples(adapters, baselines, t0);

    // Second tick: +20,000 inBytes, +10,000 outBytes over 2 seconds
    adapters[0].inBytesTotal = 120'000;
    adapters[0].outBytesTotal = 60'000;

    const auto samples = calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].inBytesTotal, 120'000u);
    EXPECT_EQ(samples[0].outBytesTotal, 60'000u);
    EXPECT_EQ(samples[0].inBytesPerSec, std::optional<uint64_t>{10'000}); // 20,000 / 2s
    EXPECT_EQ(samples[0].outBytesPerSec, std::optional<uint64_t>{5'000}); // 10,000 / 2s
}

TEST(NetworkCollector, CalculateNetworkSamples_LoopbackAdapter_FilteredOut)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 1,
            .adapterName = "Loopback Pseudo-Interface",
            .isLoopback = true,
        },
        RawNetworkAdapter{
            .luid = 2,
            .adapterName = "Wi-Fi",
            .isLoopback = false,
        },
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Wi-Fi");
    EXPECT_FALSE(baselines.contains(1));
    EXPECT_TRUE(baselines.contains(2));
}

TEST(NetworkCollector, CalculateNetworkSamples_FilterInterface_FilteredOut)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    // GetIfTable2 reports each NDIS filter bound to an adapter as its own Up
    // row with the same counters as the adapter.
    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 1,
            .adapterName = "Ethernet",
            .inBytesTotal = 5'000,
            .operationalStatus = OperationalStatus::Up,
            .isHardwareInterface = true,
        },
        RawNetworkAdapter{
            .luid = 2,
            .adapterName = "Ethernet-WFP Native MAC Layer LightWeight Filter-0000",
            .inBytesTotal = 5'000,
            .operationalStatus = OperationalStatus::Up,
            .isFilterInterface = true,
        },
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Ethernet");
    EXPECT_FALSE(baselines.contains(2));
}

TEST(NetworkCollector, CalculateNetworkSamples_HardwareFlag_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{.luid = 1, .adapterName = "Ethernet", .isHardwareInterface = true},
        RawNetworkAdapter{.luid = 2, .adapterName = "VPN Tunnel", .isHardwareInterface = false},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_TRUE(samples[0].isHardwareInterface);
    EXPECT_FALSE(samples[1].isHardwareInterface);
}

TEST(NetworkCollector, CalculateNetworkSamples_AdapterType_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{.luid = 1, .adapterName = "Wi-Fi", .type = AdapterType::WiFi},
        RawNetworkAdapter{.luid = 2, .adapterName = "Teredo", .type = AdapterType::Tunnel},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].type, AdapterType::WiFi);
    EXPECT_EQ(samples[1].type, AdapterType::Tunnel);
}

TEST(NetworkCollector, CalculateNetworkSamples_InterfaceId_LuidThenIfIndex)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    // Two adapters with the same name stay distinct by their interface.
    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{.luid = 0x1234'0000'0001, .ifIndex = 7, .adapterName = "WAN Miniport (IP)"},
        RawNetworkAdapter{.luid = 0, .ifIndex = 9, .adapterName = "WAN Miniport (IP)"},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].interfaceId, 0x1234'0000'0001u);
    EXPECT_EQ(samples[1].interfaceId, 9u);
}

TEST(NetworkCollector, CalculateNetworkSamples_IpInterfaceFlag_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{.luid = 1, .adapterName = "Mullvad", .hasIpInterface = true},
        RawNetworkAdapter{.luid = 2,
                          .adapterName = "Local Area Connection* 7",
                          .operationalStatus = OperationalStatus::Up,
                          .hasIpInterface = false},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_TRUE(samples[0].hasIpInterface);
    EXPECT_FALSE(samples[1].hasIpInterface);
}

TEST(NetworkCollector, CalculateNetworkSamples_HiddenFlag_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 1, .adapterName = "Local Area Connection* 1", .hasIpInterface = true, .isHidden = true},
        RawNetworkAdapter{.luid = 2, .adapterName = "Bluetooth Network Connection", .hasIpInterface = true},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_TRUE(samples[0].isHidden);
    EXPECT_FALSE(samples[1].isHidden);
}

TEST(NetworkCollector, CalculateNetworkSamples_CounterResetOrUnderflow_RatesUnavailable)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};

    std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .adapterName = "Ethernet",
        .inBytesTotal = 50'000,
        .outBytesTotal = 25'000,
        .isLoopback = false,
    }};

    calculateNetworkSamples(adapters, baselines, t0);

    // Counter resets to a lower value (e.g. adapter reset)
    adapters[0].inBytesTotal = 1'000;
    adapters[0].outBytesTotal = 500;

    const auto samples = calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_FALSE(samples[0].inBytesPerSec.has_value());
    EXPECT_FALSE(samples[0].outBytesPerSec.has_value());
    EXPECT_EQ(baselines[101].inBytes, 1'000u);
    EXPECT_EQ(baselines[101].outBytes, 500u);
}

TEST(NetworkCollector, CalculateNetworkSamples_ZeroElapsedDuration_RatesUnavailable)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

    std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .adapterName = "Ethernet",
        .inBytesTotal = 50'000,
        .outBytesTotal = 25'000,
        .isLoopback = false,
    }};

    calculateNetworkSamples(adapters, baselines, t0);

    // Same timestamp
    adapters[0].inBytesTotal = 60'000;
    adapters[0].outBytesTotal = 30'000;
    const auto samples = calculateNetworkSamples(adapters, baselines, t0);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_FALSE(samples[0].inBytesPerSec.has_value());
    EXPECT_FALSE(samples[0].outBytesPerSec.has_value());
}

TEST(NetworkCollector, CalculateNetworkSamples_IdleLink_ReportsZeroNotUnavailable)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};

    const std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .adapterName = "Ethernet",
        .inBytesTotal = 50'000,
        .outBytesTotal = 25'000,
        .isLoopback = false,
    }};

    calculateNetworkSamples(adapters, baselines, t0);
    const auto samples = calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].inBytesPerSec, std::optional<uint64_t>{0});
    EXPECT_EQ(samples[0].outBytesPerSec, std::optional<uint64_t>{0});
}

TEST(NetworkCollector, CalculateNetworkSamples_ResetInOneDirection_OtherDirectionStillReported)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};

    std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .adapterName = "Ethernet",
        .inBytesTotal = 50'000,
        .outBytesTotal = 25'000,
        .isLoopback = false,
    }};

    calculateNetworkSamples(adapters, baselines, t0);

    adapters[0].inBytesTotal = 1'000;   // inbound counter reset
    adapters[0].outBytesTotal = 27'000; // outbound advanced by 2,000 bytes
    const auto samples = calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_FALSE(samples[0].inBytesPerSec.has_value());
    EXPECT_EQ(samples[0].outBytesPerSec, std::optional<uint64_t>{2'000});
}

TEST(NetworkCollector, CalculateNetworkSamples_RetiredAdapter_PurgedFromBaselines)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};

    const std::vector<RawNetworkAdapter> adaptersTick0 = {
        RawNetworkAdapter{.luid = 101, .adapterName = "Ethernet", .isLoopback = false},
        RawNetworkAdapter{.luid = 102, .adapterName = "USB-NIC", .isLoopback = false},
    };

    calculateNetworkSamples(adaptersTick0, baselines, t0);
    EXPECT_EQ(baselines.size(), 2u);

    // USB-NIC disconnected
    const std::vector<RawNetworkAdapter> adaptersTick1 = {
        RawNetworkAdapter{.luid = 101, .adapterName = "Ethernet", .isLoopback = false},
    };

    const auto samples = calculateNetworkSamples(adaptersTick1, baselines, t1);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(baselines.size(), 1u);
    EXPECT_TRUE(baselines.contains(101));
    EXPECT_FALSE(baselines.contains(102));
}

TEST(NetworkCollector, CalculateNetworkSamples_IpAndDnsAddresses_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .adapterName = "Ethernet",
        .friendlyName = "Ethernet 1",
        .description = "Realtek Controller",
        .ipAddresses = {{.address = "192.168.1.50"}, {.address = "2001:db8::1"}},
        .dnsServers = {"1.1.1.1", "1.0.0.1"},
        .isLoopback = false,
    }};

    const auto samples = calculateNetworkSamples(adapters, baselines, now);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].friendlyName, "Ethernet 1");
    EXPECT_EQ(samples[0].description, "Realtek Controller");
    ASSERT_EQ(samples[0].ipAddresses.size(), 2u);
    EXPECT_EQ(samples[0].ipAddresses[0].text, "192.168.1.50");
    EXPECT_EQ(samples[0].ipAddresses[1].text, "2001:db8::1");
    ASSERT_EQ(samples[0].dnsServers.size(), 2u);
    EXPECT_EQ(samples[0].dnsServers[0], "1.1.1.1");
    EXPECT_EQ(samples[0].dnsServers[1], "1.0.0.1");
}

TEST(NetworkCollector, CalculateNetworkSamples_AddressStates_KeepsPreferredAndDeprecatedOnlyWhileUp)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();
    const std::vector<UnicastAddress> addresses = {
        {.address = "192.168.1.119", .state = AddressState::Preferred},
        {.address = "fd44:555c:2315:4176:483d:5f94:693b:1ea5", .state = AddressState::Deprecated},
        {.address = "169.254.47.250", .state = AddressState::Tentative},
        {.address = "192.168.1.50", .state = AddressState::Duplicate},
        {.address = "10.0.0.5", .state = AddressState::Invalid},
    };

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{.luid = 101, .operationalStatus = OperationalStatus::Up, .ipAddresses = addresses},
        RawNetworkAdapter{.luid = 102, .operationalStatus = OperationalStatus::Down, .ipAddresses = addresses},
        RawNetworkAdapter{.luid = 103, .operationalStatus = OperationalStatus::Dormant, .ipAddresses = addresses},
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 3u);
    EXPECT_EQ(texts(samples[0].ipAddresses),
              (std::vector<std::string>{"192.168.1.119", "fd44:555c:2315:4176:483d:5f94:693b:1ea5"}));
    EXPECT_EQ(texts(samples[1].ipAddresses), (std::vector<std::string>{"192.168.1.119"}));
    EXPECT_EQ(texts(samples[2].ipAddresses), (std::vector<std::string>{"192.168.1.119"}));
}

// What Windows reports for two disconnected adapters on the development
// machine: the Wi-Fi adapter keeps the lease from its last connection as
// Deprecated, and the Bluetooth PAN adapter keeps a Tentative automatic
// private address. Neither can be used.
TEST(NetworkCollector, CalculateNetworkSamples_DisconnectedAdapters_LeftoverAddressesDropped)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 101,
            .adapterName = "WiFi",
            .operationalStatus = OperationalStatus::Down,
            .ipAddresses = {{.address = "fe80::cd5b:b3cd:b185:d66b", .state = AddressState::Deprecated},
                            {.address = "192.168.1.16", .state = AddressState::Deprecated}},
            .dnsServers = {"192.168.1.1"},
        },
        RawNetworkAdapter{
            .luid = 102,
            .adapterName = "Bluetooth Network Connection",
            .operationalStatus = OperationalStatus::Down,
            .ipAddresses = {{.address = "169.254.113.81", .state = AddressState::Tentative},
                            {.address = "fe80::bc71:942f:7fb6:cbaa", .state = AddressState::Deprecated}},
        },
    };

    const auto samples = calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 2u);
    EXPECT_TRUE(samples[0].ipAddresses.empty());
    EXPECT_TRUE(samples[1].ipAddresses.empty());
    // Configured DNS servers are kept; the Network tab shows them only while the adapter is Up.
    EXPECT_EQ(samples[0].dnsServers, std::vector<std::string>{"192.168.1.1"});
}

TEST(NetworkCollector, CalculateNetworkSamples_TemporaryAndLinkLocalMarks_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 1,
        .operationalStatus = OperationalStatus::Up,
        .ipAddresses =
            {
                {.address = "fd44:555c:2315:4176:c6d0:77e0:5d11:3727"},
                {.address = "fd44:555c:2315:4176:483d:5f94:693b:1ea5", .isTemporary = true},
                {.address = "fe80::d347:c091:2487:3b32%16", .isLinkLocal = true},
            },
    }};

    const auto samples = calculateNetworkSamples(adapters, baselines, std::chrono::steady_clock::now());

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].ipAddresses, (std::vector<IpAddress>{
                                          {.text = "fd44:555c:2315:4176:c6d0:77e0:5d11:3727"},
                                          {.text = "fd44:555c:2315:4176:483d:5f94:693b:1ea5", .isTemporary = true},
                                          {.text = "fe80::d347:c091:2487:3b32%16", .isLinkLocal = true},
                                      }));
}

TEST(NetworkCollector, CalculateNetworkSamples_AdapterComesUp_DeprecatedAddressKept)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};
    std::vector<RawNetworkAdapter> adapters = {RawNetworkAdapter{
        .luid = 101,
        .operationalStatus = OperationalStatus::Down,
        .ipAddresses = {{.address = "192.168.1.16", .state = AddressState::Deprecated}},
    }};

    const auto down = calculateNetworkSamples(adapters, baselines, t0);
    adapters[0].operationalStatus = OperationalStatus::Up;
    const auto up = calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(down.size(), 1u);
    ASSERT_EQ(up.size(), 1u);
    EXPECT_TRUE(down[0].ipAddresses.empty());
    EXPECT_EQ(texts(up[0].ipAddresses), (std::vector<std::string>{"192.168.1.16"}));
}

TEST(NetworkCollector, Collect_InjectedReader_InvokedAndReturnsSamples)
{
    const auto now = std::chrono::steady_clock::now();

    auto mockReader = []() -> std::optional<std::vector<RawNetworkAdapter>> {
        return std::vector<RawNetworkAdapter>{RawNetworkAdapter{
            .luid = 200,
            .adapterName = "Wi-Fi",
            .inBytesTotal = 1234,
            .outBytesTotal = 5678,
            .isLoopback = false,
        }};
    };

    NetworkCollector collector(mockReader, [now]() { return now; });
    const auto samples = collector.collect();

    ASSERT_TRUE(samples.has_value());
    ASSERT_EQ(samples->size(), 1u);
    EXPECT_EQ((*samples)[0].adapterName, "Wi-Fi");
    EXPECT_EQ((*samples)[0].inBytesTotal, 1234u);
    EXPECT_EQ((*samples)[0].outBytesTotal, 5678u);
}

TEST(NetworkCollector, ResetBaselines_NextCollectHasNoRatesAndTheOneAfterMeasuresFromIt)
{
    uint64_t inBytes = 100'000;
    uint64_t outBytes = 50'000;
    auto now = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    NetworkCollector collector(
        [&]() -> std::optional<std::vector<RawNetworkAdapter>> {
            return std::vector<RawNetworkAdapter>{RawNetworkAdapter{
                .luid = 200, .adapterName = "Wi-Fi", .inBytesTotal = inBytes, .outBytesTotal = outBytes}};
        },
        [&now]() { return now; });
    const auto first = collector.collect();

    // Traffic before the reset, as before a sleep; no rate may include it.
    now += std::chrono::hours{8};
    inBytes += 1'000'000;
    outBytes += 1'000'000;
    collector.resetBaselines();
    const auto afterReset = collector.collect();

    now += std::chrono::seconds{2};
    inBytes += 20'000;
    outBytes += 10'000;
    const auto next = collector.collect();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(afterReset.has_value());
    ASSERT_TRUE(next.has_value());
    EXPECT_FALSE((*afterReset)[0].inBytesPerSec.has_value());
    EXPECT_FALSE((*afterReset)[0].outBytesPerSec.has_value());
    EXPECT_EQ((*next)[0].inBytesPerSec, std::optional<uint64_t>{10'000});
    EXPECT_EQ((*next)[0].outBytesPerSec, std::optional<uint64_t>{5'000});
}

TEST(NetworkCollector, Collect_ReaderReturnsNullopt_ReturnsNullopt)
{
    auto failingReader = []() -> std::optional<std::vector<RawNetworkAdapter>> { return std::nullopt; };

    NetworkCollector collector(failingReader, []() { return std::chrono::steady_clock::now(); });
    const auto samples = collector.collect();

    EXPECT_FALSE(samples.has_value());
}
