#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "domain/system_snapshot.h"
#include "support/network_samples.h"

namespace verdandi::tests
{

inline constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

/** A snapshot with only the given adapters, or with a failed adapter read for std::nullopt. */
[[nodiscard]] inline domain::SystemSnapshot
snapshotWithNetworks(std::optional<std::vector<domain::NetworkSample>> networks)
{
    domain::SystemSnapshot snapshot;
    snapshot.networks = std::move(networks);
    return snapshot;
}

/** A snapshot with only the given processes, taken atSecond seconds into the monotonic clock. */
[[nodiscard]] inline domain::SystemSnapshot snapshotWithProcesses(std::vector<domain::ProcessInfo> processes,
                                                                  int atSecond = 0)
{
    domain::SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{atSecond}};
    snapshot.processes = std::move(processes);
    return snapshot;
}

/**
 * A snapshot with a value in every field the dashboard and the Performance
 * tab show: 4 cores at 42.5%, half of 16 GiB in use, D:\ 10% and C:\ 75%
 * full, an Ethernet adapter that is Up (ID 1) and a Wi-Fi adapter that is
 * Down (ID 2), internet access, 4 hours 12 minutes of uptime, and memory at
 * Warning health.
 */
[[nodiscard]] inline domain::SystemSnapshot populatedSnapshot()
{
    domain::SystemSnapshot snapshot;
    snapshot.cpu = domain::CpuSample{
        .totalUsagePercent = 42.5f, .coreUsagePercents = {10.0f, 20.0f, 30.0f, 40.0f}, .coreCount = 4};
    snapshot.memory = domain::MemorySample{
        .totalBytes = 16 * kGiB,
        .availableBytes = 8 * kGiB,
        .usagePercent = 50.0f,
        .commitLimit = 32 * kGiB,
        .commitCurrent = 8 * kGiB,
    };
    snapshot.disks = std::vector<domain::DiskSample>{
        domain::DiskSample{.volumeName = "D:\\", .totalBytes = 1000, .freeBytes = 900, .usagePercent = 10.0f},
        domain::DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .freeBytes = 250, .usagePercent = 75.0f},
    };
    snapshot.networks = std::vector<domain::NetworkSample>{
        adapter(1, "Ethernet", domain::OperationalStatus::Up),
        adapter(2, "Wi-Fi", domain::OperationalStatus::Down),
    };
    snapshot.connectivity = {.level = domain::ConnectivityLevel::InternetAccess, .isMetered = false};
    snapshot.uptime = std::chrono::hours{4} + std::chrono::minutes{12};
    snapshot.health = domain::SystemHealth{
        .cpu = domain::HealthLevel::Healthy,
        .memory = domain::HealthLevel::Warning,
        .disk = domain::HealthLevel::Healthy,
        .network = domain::HealthLevel::Healthy,
        .overall = domain::HealthLevel::Warning,
    };
    return snapshot;
}

/**
 * populatedSnapshot() with the longest values the tabs show: 256 logical
 * processors; three adapters that are Up (Ethernet, Wi-Fi and a VPN tunnel,
 * IDs 1 to 3) with long descriptions, IPv6 addresses and DNS server lists;
 * every connectivity flag set; and two networks, one with a 32-byte name,
 * the longest SSID.
 */
[[nodiscard]] inline domain::SystemSnapshot demandingSnapshot()
{
    domain::SystemSnapshot snapshot = populatedSnapshot();
    snapshot.cpu->coreUsagePercents.assign(256, 50.0f);
    snapshot.cpu->coreCount = 256;

    domain::NetworkSample ethernet = adapter(1, "Ethernet");
    ethernet.description = "Realtek Gaming 2.5GbE Family Controller #2 (Kernel Debugger Compatible)";
    ethernet.linkSpeedBps = 2'500'000'000;
    ethernet.ipAddresses = {
        {.text = "192.168.100.119"},
        {.text = "2a02:aa1:1234:5678:9abc:def0:1234:5678"},
        {.text = "2a02:aa1:1234:5678:1111:2222:3333:4444", .isTemporary = true},
        {.text = "fe80::cd5b:b3cd:b185:d66b%16", .isLinkLocal = true},
    };
    ethernet.dnsServers = {"fd44:555c:2315:4176:c6d0:77e0:5d11:1", "2001:4860:4860::8888", "192.168.100.1"};
    ethernet.inBytesTotal = 987'654'321'098;
    ethernet.outBytesTotal = 123'456'789'012;
    domain::NetworkSample wifi = ethernet;
    wifi.interfaceId = 2;
    wifi.adapterName = "{Wi-Fi}";
    wifi.friendlyName = "Wi-Fi";
    wifi.description = "Intel(R) Wi-Fi 7 BE200 320MHz Wireless Network Adapter";
    wifi.type = domain::AdapterType::WiFi;
    domain::NetworkSample tunnel = adapter(3, "Mullvad", domain::OperationalStatus::Up, kVirtualAdapter);
    tunnel.description = "Mullvad Tunnel (WireGuard) Virtual Network Adapter";
    tunnel.type = domain::AdapterType::Other;
    tunnel.ipAddresses = {{.text = "10.64.12.34"}, {.text = "fc00:bbbb:bbbb:bb01:d:0:1c:2f45"}};
    tunnel.dnsServers = {"10.64.0.1"};
    snapshot.networks = std::vector<domain::NetworkSample>{ethernet, wifi, tunnel};

    snapshot.connectivity = {
        .level = domain::ConnectivityLevel::ConstrainedInternetAccess, .isMetered = true, .isRoaming = true};
    snapshot.networkProfiles = std::vector<domain::NetworkProfile>{
        {.name = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",
         .category = domain::NetworkCategory::DomainAuthenticated,
         .interfaceIds = {1}},
        {.name = "Guest network of the coffee shop", .category = domain::NetworkCategory::Public, .interfaceIds = {2}},
    };
    return snapshot;
}

} // namespace verdandi::tests
