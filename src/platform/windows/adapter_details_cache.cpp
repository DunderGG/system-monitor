#include "platform/windows/adapter_details_cache.h"

#include <algorithm>
#include <utility>

namespace sysmon::platform
{

const AdapterDetails* AdapterDetailsTable::find(uint64_t luid, uint32_t ifIndex) const
{
    if (const auto it = byLuid.find(luid); it != byLuid.end()) {
        return &it->second;
    }
    if (const auto indexIt = luidByIfIndex.find(ifIndex); indexIt != luidByIfIndex.end()) {
        if (const auto it = byLuid.find(indexIt->second); it != byLuid.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

void AdapterDetailsCache::markStale()
{
    const std::lock_guard lock(m_staleMutex);
    m_isStale = true;
}

bool AdapterDetailsCache::beginRefreshIfDue(std::span<const uint64_t> interfaceLuids,
                                            std::chrono::steady_clock::time_point now)
{
    const bool hasNewInterface =
        std::ranges::any_of(interfaceLuids, [this](uint64_t luid) { return !m_knownLuids.contains(luid); });
    const bool isExpired = !m_lastRefresh || now - *m_lastRefresh >= kMaxAge;

    const std::lock_guard lock(m_staleMutex);
    const bool isDue = m_isStale || hasNewInterface || isExpired;
    m_isStale = false;
    return isDue;
}

void AdapterDetailsCache::completeRefresh(std::optional<AdapterDetailsTable> table,
                                          std::span<const uint64_t> interfaceLuids,
                                          std::chrono::steady_clock::time_point now)
{
    if (!table) {
        m_table = {};
        m_knownLuids.clear();
        m_lastRefresh.reset();
        return;
    }

    m_table = std::move(*table);
    m_knownLuids = std::unordered_set<uint64_t>(interfaceLuids.begin(), interfaceLuids.end());
    m_lastRefresh = now;
}

const AdapterDetailsTable& AdapterDetailsCache::table() const
{
    return m_table;
}

} // namespace sysmon::platform
