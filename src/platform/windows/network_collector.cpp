#include "platform/windows/network_collector.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <spdlog/spdlog.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sysmon::platform
{

namespace
{

std::string wideToUtf8(const WCHAR *wideStr)
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
    void operator()(MIB_IF_TABLE2 *p) const noexcept
    {
        if (p) {
            FreeMibTable(p);
        }
    }
};

using ScopedMibIfTable2 = std::unique_ptr<MIB_IF_TABLE2, MibTableDeleter>;

struct AdapterDetails
{
    std::string adapterName;
    std::string friendlyName;
    std::string description;
    std::vector<std::string> ipAddresses;
    std::vector<std::string> dnsServers;
};

std::optional<std::vector<RawNetworkAdapter>> queryWindowsAdapters()
{
    MIB_IF_TABLE2 *rawTable = nullptr;
    const DWORD mibResult = GetIfTable2(&rawTable);
    if (mibResult != NO_ERROR || !rawTable) {
        spdlog::error("GetIfTable2 failed with error code {}", mibResult);
        return std::nullopt;
    }
    ScopedMibIfTable2 table(rawTable);

    std::unordered_map<uint64_t, AdapterDetails> detailsByLuid;
    std::unordered_map<uint32_t, uint64_t> ifIndexToLuid;

    ULONG bufferSize = 16384;
    std::vector<BYTE> buffer(bufferSize);
    auto *addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    const ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;

    ULONG gaaResult = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &bufferSize);
    if (gaaResult == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        gaaResult = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &bufferSize);
    }

    if (gaaResult == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES curr = addresses; curr != nullptr; curr = curr->Next) {
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
                    const auto *sin = reinterpret_cast<const sockaddr_in *>(uni->Address.lpSockaddr);
                    if (inet_ntop(AF_INET, &(sin->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                        details.ipAddresses.emplace_back(ipBuffer);
                    }
                } else if (uni->Address.lpSockaddr->sa_family == AF_INET6) {
                    const auto *sin6 = reinterpret_cast<const sockaddr_in6 *>(uni->Address.lpSockaddr);
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
                    const auto *sin = reinterpret_cast<const sockaddr_in *>(dns->Address.lpSockaddr);
                    if (inet_ntop(AF_INET, &(sin->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                        details.dnsServers.emplace_back(ipBuffer);
                    }
                } else if (dns->Address.lpSockaddr->sa_family == AF_INET6) {
                    const auto *sin6 = reinterpret_cast<const sockaddr_in6 *>(dns->Address.lpSockaddr);
                    if (inet_ntop(AF_INET6, &(sin6->sin6_addr), ipBuffer, sizeof(ipBuffer))) {
                        details.dnsServers.emplace_back(ipBuffer);
                    }
                }
            }

            const uint64_t luidKey = curr->Luid.Value;
            if (luidKey != 0) {
                detailsByLuid[luidKey] = details;
                ifIndexToLuid[curr->IfIndex] = luidKey;
            } else if (curr->IfIndex != 0) {
                ifIndexToLuid[curr->IfIndex] = static_cast<uint64_t>(curr->IfIndex);
                detailsByLuid[static_cast<uint64_t>(curr->IfIndex)] = details;
            }
        }
    } else {
        spdlog::warn("GetAdaptersAddresses failed with code {}; using GetIfTable2 data only", gaaResult);
    }

    std::vector<RawNetworkAdapter> rawAdapters;
    rawAdapters.reserve(table->NumEntries);

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2 &row = table->Table[i];
        RawNetworkAdapter raw;
        raw.luid = row.InterfaceLuid.Value;
        raw.ifIndex = row.InterfaceIndex;
        raw.isLoopback = (row.Type == IF_TYPE_SOFTWARE_LOOPBACK);
        raw.inBytesTotal = row.InOctets;
        raw.outBytesTotal = row.OutOctets;
        raw.linkSpeedBps = (row.ReceiveLinkSpeed > row.TransmitLinkSpeed) ? row.ReceiveLinkSpeed : row.TransmitLinkSpeed;

        switch (row.OperStatus) {
            case IfOperStatusUp:
                raw.operationalStatus = domain::OperationalStatus::Up;
                break;
            case IfOperStatusDown:
                raw.operationalStatus = domain::OperationalStatus::Down;
                break;
            case IfOperStatusTesting:
                raw.operationalStatus = domain::OperationalStatus::Testing;
                break;
            case IfOperStatusUnknown:
                raw.operationalStatus = domain::OperationalStatus::Unknown;
                break;
            case IfOperStatusDormant:
                raw.operationalStatus = domain::OperationalStatus::Dormant;
                break;
            case IfOperStatusNotPresent:
                raw.operationalStatus = domain::OperationalStatus::NotPresent;
                break;
            case IfOperStatusLowerLayerDown:
                raw.operationalStatus = domain::OperationalStatus::LowerLayerDown;
                break;
            default:
                raw.operationalStatus = domain::OperationalStatus::Unknown;
                break;
        }

        const std::string alias = wideToUtf8(row.Alias);
        const std::string desc = wideToUtf8(row.Description);

        auto addrIt = detailsByLuid.find(raw.luid);
        if (addrIt == detailsByLuid.end()) {
            const auto ifIt = ifIndexToLuid.find(raw.ifIndex);
            if (ifIt != ifIndexToLuid.end()) {
                addrIt = detailsByLuid.find(ifIt->second);
            }
        }

        if (addrIt != detailsByLuid.end()) {
            const auto &details = addrIt->second;
            raw.friendlyName = !details.friendlyName.empty() ? details.friendlyName : alias;
            raw.description = !details.description.empty() ? details.description : desc;
            raw.adapterName = !raw.friendlyName.empty() ? raw.friendlyName : (!details.adapterName.empty() ? details.adapterName : raw.description);
            raw.ipAddresses = details.ipAddresses;
            raw.dnsServers = details.dnsServers;
        } else {
            raw.friendlyName = alias;
            raw.description = desc;
            raw.adapterName = !alias.empty() ? alias : desc;
        }

        rawAdapters.push_back(std::move(raw));
    }

    return rawAdapters;
}

} // namespace

NetworkCollector::NetworkCollector()
    : m_adaptersReader(queryWindowsAdapters),
      m_clockReader([]() { return std::chrono::steady_clock::now(); })
{
}

NetworkCollector::NetworkCollector(NetworkAdaptersReader adaptersReader, SteadyClockReader clockReader)
    : m_adaptersReader(std::move(adaptersReader)),
      m_clockReader(std::move(clockReader))
{
}

std::vector<domain::NetworkSample> NetworkCollector::collect()
{
    if (!m_adaptersReader) {
        return {};
    }

    const auto rawAdapters = m_adaptersReader();
    if (!rawAdapters.has_value()) {
        return {};
    }

    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();
    return calculateNetworkSamples(*rawAdapters, m_baselines, now);
}

std::vector<domain::NetworkSample> NetworkCollector::calculateNetworkSamples(
    const std::vector<RawNetworkAdapter> &adapters,
    std::unordered_map<uint64_t, NetworkBaseline> &baselines,
    std::chrono::steady_clock::time_point currentTime)
{
    std::vector<domain::NetworkSample> samples;
    std::unordered_set<uint64_t> activeKeys;

    for (const auto &adapter : adapters) {
        if (adapter.isLoopback) {
            continue;
        }

        const uint64_t key = adapter.luid != 0 ? adapter.luid
                             : (adapter.ifIndex != 0 ? static_cast<uint64_t>(adapter.ifIndex)
                                : std::hash<std::string>{}(adapter.adapterName));
        activeKeys.insert(key);

        uint64_t inBytesPerSec = 0;
        uint64_t outBytesPerSec = 0;

        const auto it = baselines.find(key);
        if (it != baselines.end()) {
            const auto elapsed = currentTime - it->second.timestamp;
            const double elapsedSec = std::chrono::duration<double>(elapsed).count();

            if (elapsedSec > 0.0) {
                if (adapter.inBytesTotal >= it->second.inBytes) {
                    const double deltaIn = static_cast<double>(adapter.inBytesTotal - it->second.inBytes);
                    inBytesPerSec = static_cast<uint64_t>(std::round(deltaIn / elapsedSec));
                }
                if (adapter.outBytesTotal >= it->second.outBytes) {
                    const double deltaOut = static_cast<double>(adapter.outBytesTotal - it->second.outBytes);
                    outBytesPerSec = static_cast<uint64_t>(std::round(deltaOut / elapsedSec));
                }
            }
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
        };

        samples.push_back(std::move(sample));
    }

    std::erase_if(baselines, [&activeKeys](const auto &pair) {
        return !activeKeys.contains(pair.first);
    });

    return samples;
}

} // namespace sysmon::platform

