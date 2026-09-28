#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sysmon::platform
{

/** Names, addresses, and DNS servers of one adapter, as reported by GetAdaptersAddresses. */
struct AdapterDetails
{
    std::string adapterName;
    std::string friendlyName;
    std::string description;
    std::vector<std::string> ipAddresses;
    std::vector<std::string> dnsServers;
};

/**
 * Adapter details keyed by interface LUID. luidByIfIndex maps interface indexes
 * to keys so an interface can still be matched when its LUID is not found.
 */
struct AdapterDetailsTable
{
    std::unordered_map<uint64_t, AdapterDetails> byLuid;
    std::unordered_map<uint32_t, uint64_t> luidByIfIndex;

    /** Returns the details for an interface, matching by LUID first and interface index second. */
    [[nodiscard]] const AdapterDetails* find(uint64_t luid, uint32_t ifIndex) const;
};

/**
 * Holds adapter details between refreshes and decides when to re-read them.
 *
 * GetAdaptersAddresses takes milliseconds and its data changes rarely, so the
 * details are re-read only when:
 * - markStale() was called (an address or interface change notification),
 * - an interface is present that was not present at the last refresh, or
 * - kMaxAge has passed, which catches changes no notification reports (such as
 *   DNS servers) and keeps details current if notifications are unavailable.
 *
 * markStale() may be called from any thread (OS notification callbacks). All
 * other members must be called from the collecting thread only.
 */
class AdapterDetailsCache
{
public:
    static constexpr std::chrono::seconds kMaxAge{30};

    /** Marks the details out of date. Thread-safe. */
    void markStale();

    /**
     * Returns true if the details must be re-read now, given the LUIDs of all
     * current interfaces. Also clears the stale mark, so a change reported while
     * the caller re-reads triggers another refresh on the next call.
     */
    [[nodiscard]] bool beginRefreshIfDue(std::span<const uint64_t> interfaceLuids,
                                         std::chrono::steady_clock::time_point now);

    /**
     * Stores the result of a re-read. std::nullopt (the query failed) clears the
     * details, so none are presented as current, and makes the next
     * beginRefreshIfDue() return true so the query is retried.
     */
    void completeRefresh(std::optional<AdapterDetailsTable> table, std::span<const uint64_t> interfaceLuids,
                         std::chrono::steady_clock::time_point now);

    [[nodiscard]] const AdapterDetailsTable& table() const;

private:
    std::mutex m_staleMutex;
    bool m_isStale{true};

    AdapterDetailsTable m_table;
    std::unordered_set<uint64_t> m_knownLuids;
    std::optional<std::chrono::steady_clock::time_point> m_lastRefresh;
};

} // namespace sysmon::platform
