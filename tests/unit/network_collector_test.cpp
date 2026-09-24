#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "platform/windows/network_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(NetworkCollector, CalculateNetworkSamples_FirstTick_RatesAreZeroAndBaselineRecorded)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 101,
            .ifIndex = 1,
            .adapterName = "Ethernet",
            .friendlyName = "Ethernet",
            .description = "Intel Gigabit",
            .inBytesTotal = 50'000,
            .outBytesTotal = 25'000,
            .linkSpeedBps = 1'000'000'000,
            .operationalStatus = OperationalStatus::Up,
            .ipAddresses = {"192.168.1.10"},
            .dnsServers = {"1.1.1.1"},
            .isLoopback = false,
        }
    };

    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, t0);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Ethernet");
    EXPECT_EQ(samples[0].inBytesTotal, 50'000u);
    EXPECT_EQ(samples[0].outBytesTotal, 25'000u);
    EXPECT_EQ(samples[0].inBytesPerSec, 0u);
    EXPECT_EQ(samples[0].outBytesPerSec, 0u);
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

    std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
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
        }
    };

    // First tick to set baseline
    NetworkCollector::calculateNetworkSamples(adapters, baselines, t0);

    // Second tick: +20,000 inBytes, +10,000 outBytes over 2 seconds
    adapters[0].inBytesTotal = 120'000;
    adapters[0].outBytesTotal = 60'000;

    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].inBytesTotal, 120'000u);
    EXPECT_EQ(samples[0].outBytesTotal, 60'000u);
    EXPECT_EQ(samples[0].inBytesPerSec, 10'000u); // 20,000 / 2s
    EXPECT_EQ(samples[0].outBytesPerSec, 5'000u); // 10,000 / 2s
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

    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, now);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Wi-Fi");
    EXPECT_FALSE(baselines.contains(1));
    EXPECT_TRUE(baselines.contains(2));
}

TEST(NetworkCollector, CalculateNetworkSamples_CounterResetOrUnderflow_RatesZeroWithoutCrash)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};
    const auto t1 = std::chrono::steady_clock::time_point{std::chrono::seconds{101}};

    std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 101,
            .adapterName = "Ethernet",
            .inBytesTotal = 50'000,
            .outBytesTotal = 25'000,
            .isLoopback = false,
        }
    };

    NetworkCollector::calculateNetworkSamples(adapters, baselines, t0);

    // Counter resets to a lower value (e.g. adapter reset)
    adapters[0].inBytesTotal = 1'000;
    adapters[0].outBytesTotal = 500;

    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, t1);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].inBytesPerSec, 0u);
    EXPECT_EQ(samples[0].outBytesPerSec, 0u);
    EXPECT_EQ(baselines[101].inBytes, 1'000u);
    EXPECT_EQ(baselines[101].outBytes, 500u);
}

TEST(NetworkCollector, CalculateNetworkSamples_ZeroElapsedDuration_RatesZero)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto t0 = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

    std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 101,
            .adapterName = "Ethernet",
            .inBytesTotal = 50'000,
            .outBytesTotal = 25'000,
            .isLoopback = false,
        }
    };

    NetworkCollector::calculateNetworkSamples(adapters, baselines, t0);

    // Same timestamp
    adapters[0].inBytesTotal = 60'000;
    adapters[0].outBytesTotal = 30'000;
    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, t0);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].inBytesPerSec, 0u);
    EXPECT_EQ(samples[0].outBytesPerSec, 0u);
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

    NetworkCollector::calculateNetworkSamples(adaptersTick0, baselines, t0);
    EXPECT_EQ(baselines.size(), 2u);

    // USB-NIC disconnected
    const std::vector<RawNetworkAdapter> adaptersTick1 = {
        RawNetworkAdapter{.luid = 101, .adapterName = "Ethernet", .isLoopback = false},
    };

    const auto samples = NetworkCollector::calculateNetworkSamples(adaptersTick1, baselines, t1);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(baselines.size(), 1u);
    EXPECT_TRUE(baselines.contains(101));
    EXPECT_FALSE(baselines.contains(102));
}

TEST(NetworkCollector, CalculateNetworkSamples_IpAndDnsAddresses_PreservedInSample)
{
    std::unordered_map<uint64_t, NetworkBaseline> baselines;
    const auto now = std::chrono::steady_clock::now();

    const std::vector<RawNetworkAdapter> adapters = {
        RawNetworkAdapter{
            .luid = 101,
            .adapterName = "Ethernet",
            .friendlyName = "Ethernet 1",
            .description = "Realtek Controller",
            .ipAddresses = {"192.168.1.50", "2001:db8::1"},
            .dnsServers = {"1.1.1.1", "1.0.0.1"},
            .isLoopback = false,
        }
    };

    const auto samples = NetworkCollector::calculateNetworkSamples(adapters, baselines, now);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].friendlyName, "Ethernet 1");
    EXPECT_EQ(samples[0].description, "Realtek Controller");
    ASSERT_EQ(samples[0].ipAddresses.size(), 2u);
    EXPECT_EQ(samples[0].ipAddresses[0], "192.168.1.50");
    EXPECT_EQ(samples[0].ipAddresses[1], "2001:db8::1");
    ASSERT_EQ(samples[0].dnsServers.size(), 2u);
    EXPECT_EQ(samples[0].dnsServers[0], "1.1.1.1");
    EXPECT_EQ(samples[0].dnsServers[1], "1.0.0.1");
}

TEST(NetworkCollector, Collect_InjectedReader_InvokedAndReturnsSamples)
{
    const auto now = std::chrono::steady_clock::now();

    auto mockReader = []() -> std::optional<std::vector<RawNetworkAdapter>> {
        return std::vector<RawNetworkAdapter>{
            RawNetworkAdapter{
                .luid = 200,
                .adapterName = "Wi-Fi",
                .inBytesTotal = 1234,
                .outBytesTotal = 5678,
                .isLoopback = false,
            }
        };
    };

    NetworkCollector collector(mockReader, [now]() { return now; });
    const auto samples = collector.collect();

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].adapterName, "Wi-Fi");
    EXPECT_EQ(samples[0].inBytesTotal, 1234u);
    EXPECT_EQ(samples[0].outBytesTotal, 5678u);
}

TEST(NetworkCollector, Collect_ReaderReturnsNullopt_ReturnsEmptyVector)
{
    auto failingReader = []() -> std::optional<std::vector<RawNetworkAdapter>> {
        return std::nullopt;
    };

    NetworkCollector collector(failingReader, []() { return std::chrono::steady_clock::now(); });
    const auto samples = collector.collect();

    EXPECT_TRUE(samples.empty());
}

