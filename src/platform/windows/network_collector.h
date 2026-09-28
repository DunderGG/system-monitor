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
    // An NDIS filter layer (e.g. WFP or QoS Packet Scheduler) bound to another
    // interface. Its counters repeat that interface's traffic.
    bool isFilterInterface{false};
    // A physical adapter rather than a tunnel, VPN, or other virtual interface.
    bool isHardwareInterface{false};
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

class WindowsAdapterReader;

/**
 * Collector implementation for network adapter traffic, throughput, and addresses.
 *
 * Windows implementation uses:
 * - GetIfTable2 (releasing table with FreeMibTable) on every collect() for 64-bit
 *   traffic counters, link speed, and operational status.
 * - GetAdaptersAddresses for friendly names, device descriptions, IP addresses, and
 *   DNS servers, re-read only when AdapterDetailsCache says they are due. Address
 *   and interface change notifications mark the cache stale.
 *
 * Filters out loopback interfaces (IF_TYPE_SOFTWARE_LOOPBACK) and NDIS filter
 * interfaces, whose counters repeat the traffic of the interface they are bound to.
 * Computes instantaneous throughput (bytes/sec) from counter deltas across steady_clock ticks.
 * collect() returns std::nullopt when the interface table cannot be read.
 * A rate is std::nullopt (never 0) when it cannot be computed: first sample for an
 * adapter, zero elapsed time, or a counter reset.
 */
class NetworkCollector : public sysmon::monitoring::INetworkCollector
{
public:
    /** Default constructor using live Windows IP Helper APIs and std::chrono::steady_clock. */
    NetworkCollector();

    /** Injected constructor for deterministic unit testing. */
    NetworkCollector(NetworkAdaptersReader adaptersReader, SteadyClockReader clockReader);

    ~NetworkCollector() override;

    [[nodiscard]] std::optional<std::vector<domain::NetworkSample>> collect() override;

    /**
     * Pure calculation helper that processes raw adapter records against historical baselines,
     * calculates bytes/sec rates over elapsed monotonic time, and purges retired baselines.
     */
    static std::vector<domain::NetworkSample> calculateNetworkSamples(
        const std::vector<RawNetworkAdapter> &adapters,
        std::unordered_map<uint64_t, NetworkBaseline> &baselines,
        std::chrono::steady_clock::time_point currentTime);

private:
    // Owns the Windows reader (and its change notifications) in production;
    // null when a reader is injected. Declared before m_adaptersReader, which
    // refers to it.
    std::unique_ptr<WindowsAdapterReader> m_windowsReader;
    NetworkAdaptersReader m_adaptersReader;
    SteadyClockReader m_clockReader;
    std::unordered_map<uint64_t, NetworkBaseline> m_baselines;
};

} // namespace sysmon::platform

