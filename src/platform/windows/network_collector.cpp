#include "platform/windows/network_collector.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <spdlog/spdlog.h>

#include "platform/windows/adapter_details_cache.h"
#include "platform/windows/repeated_failure_log.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sysmon::platform
{

namespace
{

// Starting GetAdaptersAddresses buffer size recommended by its documentation.
constexpr ULONG kInitialAdapterBufferSize = 15 * 1024;
// The required size can grow between calls, so the documented pattern retries a few times.
constexpr int kMaxAdapterQueryAttempts = 3;

std::string wideToUtf8(const WCHAR* wideStr)
{
    if (!wideStr || wideStr[0] == L'\0') {
        return {};
    }
    const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(sizeNeeded - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, result.data(), sizeNeeded, nullptr, nullptr);
    return result;
}

struct MibTableDeleter
{
    void operator()(MIB_IF_TABLE2* p) const noexcept
    {
        if (p) {
            FreeMibTable(p);
        }
    }
};

using ScopedMibIfTable2 = std::unique_ptr<MIB_IF_TABLE2, MibTableDeleter>;

// Cancels a NotifyUnicastIpAddressChange / NotifyIpInterfaceChange registration.
// CancelMibChangeNotify2 waits for in-flight callbacks to return.
struct MibNotificationDeleter
{
    void operator()(HANDLE handle) const noexcept
    {
        if (handle != nullptr) {
            CancelMibChangeNotify2(handle);
        }
    }
};

using ScopedMibNotification = std::unique_ptr<std::remove_pointer_t<HANDLE>, MibNotificationDeleter>;

// Returns bytes/sec between two cumulative counter readings, or std::nullopt when
// no meaningful rate exists: non-positive elapsed time, or a counter that went
// backwards (adapter reset), where the delta across the reset is unknown.
std::optional<uint64_t> calculateByteRate(uint64_t previousBytes, uint64_t currentBytes, double elapsedSec)
{
    if (elapsedSec <= 0.0 || currentBytes < previousBytes) {
        return std::nullopt;
    }
    const double deltaBytes = static_cast<double>(currentBytes - previousBytes);
    return static_cast<uint64_t>(std::round(deltaBytes / elapsedSec));
}

domain::OperationalStatus toOperationalStatus(IF_OPER_STATUS status)
{
    switch (status) {
        case IfOperStatusUp:
            return domain::OperationalStatus::Up;
        case IfOperStatusDown:
            return domain::OperationalStatus::Down;
        case IfOperStatusTesting:
            return domain::OperationalStatus::Testing;
        case IfOperStatusDormant:
            return domain::OperationalStatus::Dormant;
        case IfOperStatusNotPresent:
            return domain::OperationalStatus::NotPresent;
        case IfOperStatusLowerLayerDown:
            return domain::OperationalStatus::LowerLayerDown;
        case IfOperStatusUnknown:
        default:
            return domain::OperationalStatus::Unknown;
    }
}

// Reads names, addresses, and DNS servers for all adapters with GetAdaptersAddresses.
std::optional<AdapterDetailsTable> queryAdapterDetails(RepeatedFailureLog& failureLog)
{
    const ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG bufferSize = kInitialAdapterBufferSize;
    std::vector<BYTE> buffer;
    ULONG gaaResult = ERROR_BUFFER_OVERFLOW;

    for (int attempt = 0; attempt < kMaxAdapterQueryAttempts && gaaResult == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(bufferSize);
        gaaResult = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                         reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &bufferSize);
    }

    if (gaaResult == ERROR_NO_DATA) {
        failureLog.success();
        return AdapterDetailsTable{};
    }
    if (gaaResult != NO_ERROR) {
        failureLog.failure(spdlog::level::warn, "GetAdaptersAddresses failed with code {}; using GetIfTable2 data only",
                           gaaResult);
        return std::nullopt;
    }

    failureLog.success();
    AdapterDetailsTable table;
    const auto* addresses = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buffer.data());
    for (const IP_ADAPTER_ADDRESSES* curr = addresses; curr != nullptr; curr = curr->Next) {
        AdapterDetails details;
        if (curr->AdapterName) {
            details.adapterName = curr->AdapterName;
        }
        if (curr->FriendlyName) {
            details.friendlyName = wideToUtf8(curr->FriendlyName);
        }
        if (curr->Description) {
            details.description = wideToUtf8(curr->Description);
        }

        for (PIP_ADAPTER_UNICAST_ADDRESS uni = curr->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
            if (!uni->Address.lpSockaddr) {
                continue;
            }
            char ipBuffer[INET6_ADDRSTRLEN] = {0};
            if (uni->Address.lpSockaddr->sa_family == AF_INET) {
                const auto* sin = reinterpret_cast<const sockaddr_in*>(uni->Address.lpSockaddr);
                if (inet_ntop(AF_INET, &(sin->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                    details.ipAddresses.emplace_back(ipBuffer);
                }
            } else if (uni->Address.lpSockaddr->sa_family == AF_INET6) {
                const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(uni->Address.lpSockaddr);
                if (inet_ntop(AF_INET6, &(sin6->sin6_addr), ipBuffer, sizeof(ipBuffer))) {
                    details.ipAddresses.emplace_back(ipBuffer);
                }
            }
        }

        for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = curr->FirstDnsServerAddress; dns != nullptr; dns = dns->Next) {
            if (!dns->Address.lpSockaddr) {
                continue;
            }
            char ipBuffer[INET6_ADDRSTRLEN] = {0};
            if (dns->Address.lpSockaddr->sa_family == AF_INET) {
                const auto* sin = reinterpret_cast<const sockaddr_in*>(dns->Address.lpSockaddr);
                if (inet_ntop(AF_INET, &(sin->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                    details.dnsServers.emplace_back(ipBuffer);
                }
            } else if (dns->Address.lpSockaddr->sa_family == AF_INET6) {
                const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(dns->Address.lpSockaddr);
                if (inet_ntop(AF_INET6, &(sin6->sin6_addr), ipBuffer, sizeof(ipBuffer))) {
                    details.dnsServers.emplace_back(ipBuffer);
                }
            }
        }

        const uint64_t luidKey = curr->Luid.Value;
        if (luidKey != 0) {
            table.byLuid[luidKey] = std::move(details);
            table.luidByIfIndex[curr->IfIndex] = luidKey;
        } else if (curr->IfIndex != 0) {
            table.luidByIfIndex[curr->IfIndex] = static_cast<uint64_t>(curr->IfIndex);
            table.byLuid[static_cast<uint64_t>(curr->IfIndex)] = std::move(details);
        }
    }
    return table;
}

} // namespace

/**
 * Production adapter reader. Reads GetIfTable2 on every call and takes names,
 * addresses, and DNS servers from an AdapterDetailsCache, re-reading them with
 * GetAdaptersAddresses only when the cache says they are due.
 *
 * Address and interface change notifications mark the cache stale. They are an
 * event-driven source (docs/architecture.md, "Event-driven collectors"): the
 * callbacks only store the stale mark, the registrations are RAII members
 * declared after the cache so they are cancelled first, the cache starts stale
 * so the first read is complete, and if registration fails the cache's maximum
 * age still refreshes the details.
 *
 * Registers callbacks with this as their context, so it is neither copyable nor movable.
 */
class WindowsAdapterReader
{
public:
    WindowsAdapterReader()
    {
        HANDLE addressHandle = nullptr;
        const DWORD addressResult =
            NotifyUnicastIpAddressChange(AF_UNSPEC, &onUnicastAddressChange, this, FALSE, &addressHandle);
        if (addressResult == NO_ERROR) {
            m_addressNotification.reset(addressHandle);
        } else {
            spdlog::warn("NotifyUnicastIpAddressChange failed with error {}; adapter addresses refresh every {}s",
                         addressResult, AdapterDetailsCache::kMaxAge.count());
        }

        HANDLE interfaceHandle = nullptr;
        const DWORD interfaceResult =
            NotifyIpInterfaceChange(AF_UNSPEC, &onInterfaceChange, this, FALSE, &interfaceHandle);
        if (interfaceResult == NO_ERROR) {
            m_interfaceNotification.reset(interfaceHandle);
        } else {
            spdlog::warn("NotifyIpInterfaceChange failed with error {}; adapter details refresh every {}s",
                         interfaceResult, AdapterDetailsCache::kMaxAge.count());
        }
    }

    WindowsAdapterReader(const WindowsAdapterReader&) = delete;
    WindowsAdapterReader& operator=(const WindowsAdapterReader&) = delete;
    WindowsAdapterReader(WindowsAdapterReader&&) = delete;
    WindowsAdapterReader& operator=(WindowsAdapterReader&&) = delete;
    ~WindowsAdapterReader() = default;

    std::optional<std::vector<RawNetworkAdapter>> read()
    {
        MIB_IF_TABLE2* rawTable = nullptr;
        const DWORD mibResult = GetIfTable2(&rawTable);
        if (mibResult != NO_ERROR || !rawTable) {
            m_ifTableFailureLog.failure(spdlog::level::err, "GetIfTable2 failed with error code {}", mibResult);
            return std::nullopt;
        }
        ScopedMibIfTable2 table(rawTable);
        m_ifTableFailureLog.success();

        std::vector<uint64_t> luids;
        luids.reserve(table->NumEntries);
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            luids.push_back(table->Table[i].InterfaceLuid.Value);
        }

        const auto now = std::chrono::steady_clock::now();
        if (m_cache.beginRefreshIfDue(luids, now)) {
            m_cache.completeRefresh(queryAdapterDetails(m_adapterDetailsFailureLog), luids, now);
        }
        const AdapterDetailsTable& detailsTable = m_cache.table();

        std::vector<RawNetworkAdapter> rawAdapters;
        rawAdapters.reserve(table->NumEntries);

        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const MIB_IF_ROW2& row = table->Table[i];
            RawNetworkAdapter raw;
            raw.luid = row.InterfaceLuid.Value;
            raw.ifIndex = row.InterfaceIndex;
            raw.isLoopback = (row.Type == IF_TYPE_SOFTWARE_LOOPBACK);
            raw.isFilterInterface = row.InterfaceAndOperStatusFlags.FilterInterface != FALSE;
            raw.isHardwareInterface = row.InterfaceAndOperStatusFlags.HardwareInterface != FALSE;
            raw.inBytesTotal = row.InOctets;
            raw.outBytesTotal = row.OutOctets;
            raw.linkSpeedBps =
                (row.ReceiveLinkSpeed > row.TransmitLinkSpeed) ? row.ReceiveLinkSpeed : row.TransmitLinkSpeed;
            raw.operationalStatus = toOperationalStatus(row.OperStatus);

            const std::string alias = wideToUtf8(row.Alias);
            const std::string desc = wideToUtf8(row.Description);

            if (const AdapterDetails* details = detailsTable.find(raw.luid, raw.ifIndex)) {
                raw.friendlyName = !details->friendlyName.empty() ? details->friendlyName : alias;
                raw.description = !details->description.empty() ? details->description : desc;
                raw.adapterName = !raw.friendlyName.empty()
                                      ? raw.friendlyName
                                      : (!details->adapterName.empty() ? details->adapterName : raw.description);
                raw.ipAddresses = details->ipAddresses;
                raw.dnsServers = details->dnsServers;
            } else {
                raw.friendlyName = alias;
                raw.description = desc;
                raw.adapterName = !alias.empty() ? alias : desc;
            }

            rawAdapters.push_back(std::move(raw));
        }

        return rawAdapters;
    }

private:
    static void WINAPI onUnicastAddressChange(PVOID callerContext, PMIB_UNICASTIPADDRESS_ROW /*row*/,
                                              MIB_NOTIFICATION_TYPE /*notificationType*/) noexcept
    {
        static_cast<WindowsAdapterReader*>(callerContext)->m_cache.markStale();
    }

    static void WINAPI onInterfaceChange(PVOID callerContext, PMIB_IPINTERFACE_ROW /*row*/,
                                         MIB_NOTIFICATION_TYPE /*notificationType*/) noexcept
    {
        static_cast<WindowsAdapterReader*>(callerContext)->m_cache.markStale();
    }

    RepeatedFailureLog m_ifTableFailureLog{"GetIfTable2"};
    RepeatedFailureLog m_adapterDetailsFailureLog{"GetAdaptersAddresses"};
    AdapterDetailsCache m_cache;

    // Declared after m_cache so they are cancelled (waiting for in-flight
    // callbacks) before the cache the callbacks write to is destroyed.
    ScopedMibNotification m_addressNotification;
    ScopedMibNotification m_interfaceNotification;
};

NetworkCollector::NetworkCollector()
    : m_windowsReader(std::make_unique<WindowsAdapterReader>()),
      m_adaptersReader([reader = m_windowsReader.get()] { return reader->read(); }),
      m_clockReader([]() { return std::chrono::steady_clock::now(); })
{}

NetworkCollector::NetworkCollector(NetworkAdaptersReader adaptersReader, SteadyClockReader clockReader)
    : m_adaptersReader(std::move(adaptersReader)), m_clockReader(std::move(clockReader))
{}

NetworkCollector::~NetworkCollector() = default;

std::optional<std::vector<domain::NetworkSample>> NetworkCollector::collect()
{
    if (!m_adaptersReader) {
        return std::nullopt;
    }

    const auto rawAdapters = m_adaptersReader();
    if (!rawAdapters.has_value()) {
        return std::nullopt;
    }

    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();
    return calculateNetworkSamples(*rawAdapters, m_baselines, now);
}

std::vector<domain::NetworkSample>
NetworkCollector::calculateNetworkSamples(const std::vector<RawNetworkAdapter>& adapters,
                                          std::unordered_map<uint64_t, NetworkBaseline>& baselines,
                                          std::chrono::steady_clock::time_point currentTime)
{
    std::vector<domain::NetworkSample> samples;
    std::unordered_set<uint64_t> activeKeys;

    for (const auto& adapter : adapters) {
        // Filter interfaces (WFP, QoS Packet Scheduler, ...) mirror the counters of the
        // adapter they are bound to; reporting them would count its traffic again.
        if (adapter.isLoopback || adapter.isFilterInterface) {
            continue;
        }

        const uint64_t key = adapter.luid != 0 ? adapter.luid
                                               : (adapter.ifIndex != 0 ? static_cast<uint64_t>(adapter.ifIndex)
                                                                       : std::hash<std::string>{}(adapter.adapterName));
        activeKeys.insert(key);

        // Rates stay std::nullopt when they cannot be computed (no baseline yet,
        // zero elapsed time, or a counter reset) rather than reporting 0 B/s.
        std::optional<uint64_t> inBytesPerSec;
        std::optional<uint64_t> outBytesPerSec;

        const auto it = baselines.find(key);
        if (it != baselines.end()) {
            const double elapsedSec = std::chrono::duration<double>(currentTime - it->second.timestamp).count();
            inBytesPerSec = calculateByteRate(it->second.inBytes, adapter.inBytesTotal, elapsedSec);
            outBytesPerSec = calculateByteRate(it->second.outBytes, adapter.outBytesTotal, elapsedSec);
        }

        baselines[key] = NetworkBaseline{
            .inBytes = adapter.inBytesTotal,
            .outBytes = adapter.outBytesTotal,
            .timestamp = currentTime,
        };

        domain::NetworkSample sample{
            .adapterName = adapter.adapterName,
            .friendlyName = adapter.friendlyName,
            .description = adapter.description,
            .inBytesTotal = adapter.inBytesTotal,
            .outBytesTotal = adapter.outBytesTotal,
            .inBytesPerSec = inBytesPerSec,
            .outBytesPerSec = outBytesPerSec,
            .linkSpeedBps = adapter.linkSpeedBps,
            .operationalStatus = adapter.operationalStatus,
            .ipAddresses = adapter.ipAddresses,
            .dnsServers = adapter.dnsServers,
            .isHardwareInterface = adapter.isHardwareInterface,
        };

        samples.push_back(std::move(sample));
    }

    std::erase_if(baselines, [&activeKeys](const auto& pair) { return !activeKeys.contains(pair.first); });

    return samples;
}

} // namespace sysmon::platform
