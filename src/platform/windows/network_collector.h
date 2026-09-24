#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/network_sample.h"
#include "monitoring/collector.h"

namespace sysmon::platform
{

/**
 * Raw adapter information queried from Windows IP Helper APIs.
 * Used for dependency injection during unit tests.
 */
struct RawNetworkAdapter
{
    uint64_t luid{0};
    uint32_t ifIndex{0};
    std::string adapterName;
    std::string friendlyName;
    std::string description;
    uint64_t inBytesTotal{0};
    uint64_t outBytesTotal{0};
    uint64_t linkSpeedBps{0};
    domain::OperationalStatus operationalStatus{domain::OperationalStatus::Unknown};
    std::vector<std::string> ipAddresses;
    std::vector<std::string> dnsServers;
    bool isLoopback{false};
};

/**
 * Historical counter baseline for a specific adapter to calculate throughput.
 */
struct NetworkBaseline
{
    uint64_t inBytes{0};
    uint64_t outBytes{0};
    std::chrono::steady_clock::time_point timestamp;
};

using NetworkAdaptersReader = std::function<std::optional<std::vector<RawNetworkAdapter>>()>;
using SteadyClockReader = std::function<std::chrono::steady_clock::time_point()>;

/**
 * Collector implementation for network adapter traffic, throughput, and addresses.
 *
 * Windows implementation uses:
 * - GetIfTable2 (releasing table with FreeMibTable) for 64-bit traffic counters and link speed.
 * - GetAdaptersAddresses for friendly names, device descriptions, IP addresses, and DNS servers.
 *
 * Filters out loopback interfaces (IF_TYPE_SOFTWARE_LOOPBACK).
 * Computes instantaneous throughput (bytes/sec) from counter deltas across steady_clock ticks.
 */
class NetworkCollector : public sysmon::monitoring::INetworkCollector
{
public:
    /** Default constructor using live Windows IP Helper APIs and std::chrono::steady_clock. */
    NetworkCollector();

    /** Injected constructor for deterministic unit testing. */
    NetworkCollector(NetworkAdaptersReader adaptersReader, SteadyClockReader clockReader);

    ~NetworkCollector() override = default;

    [[nodiscard]] std::vector<domain::NetworkSample> collect() override;

    /**
     * Pure calculation helper that processes raw adapter records against historical baselines,
     * calculates bytes/sec rates over elapsed monotonic time, and purges retired baselines.
     */
    static std::vector<domain::NetworkSample> calculateNetworkSamples(
        const std::vector<RawNetworkAdapter> &adapters,
        std::unordered_map<uint64_t, NetworkBaseline> &baselines,
        std::chrono::steady_clock::time_point currentTime);

private:
    NetworkAdaptersReader m_adaptersReader;
    SteadyClockReader m_clockReader;
    std::unordered_map<uint64_t, NetworkBaseline> m_baselines;
};

} // namespace sysmon::platform

