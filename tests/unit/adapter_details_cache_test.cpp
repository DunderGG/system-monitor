#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "platform/windows/adapter_details_cache.h"

using namespace sysmon::platform;

namespace
{

const auto kStart = std::chrono::steady_clock::time_point{} + std::chrono::hours{1};
const std::vector<uint64_t> kLuids = {10, 20};

AdapterDetailsTable tableWithEthernet()
{
    AdapterDetailsTable table;
    table.byLuid[10] = AdapterDetails{.friendlyName = "Ethernet", .ipAddresses = {"192.168.1.2"}};
    table.luidByIfIndex[7] = 10;
    return table;
}

// Completes a successful first refresh at kStart.
void refreshAtStart(AdapterDetailsCache& cache)
{
    static_cast<void>(cache.beginRefreshIfDue(kLuids, kStart));
    cache.completeRefresh(tableWithEthernet(), kLuids, kStart);
}

} // namespace

TEST(AdapterDetailsCache, NewCache_RefreshIsDue)
{
    AdapterDetailsCache cache;

    EXPECT_TRUE(cache.beginRefreshIfDue(kLuids, kStart));
}

TEST(AdapterDetailsCache, AfterRefresh_SameInterfacesWithinMaxAge_NotDue)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);

    EXPECT_FALSE(cache.beginRefreshIfDue(kLuids, kStart + std::chrono::seconds{1}));
    EXPECT_FALSE(cache.beginRefreshIfDue(kLuids, kStart + AdapterDetailsCache::kMaxAge - std::chrono::seconds{1}));
}

TEST(AdapterDetailsCache, AfterRefresh_ServesStoredDetails)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);

    const auto* details = cache.table().find(10, 0);

    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->friendlyName, "Ethernet");
    EXPECT_EQ(details->ipAddresses, std::vector<std::string>{"192.168.1.2"});
}

TEST(AdapterDetailsCache, MaxAgeReached_RefreshIsDue)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);

    EXPECT_TRUE(cache.beginRefreshIfDue(kLuids, kStart + AdapterDetailsCache::kMaxAge));
}

TEST(AdapterDetailsCache, MarkStale_RefreshIsDueOnce)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);

    cache.markStale();

    EXPECT_TRUE(cache.beginRefreshIfDue(kLuids, kStart + std::chrono::seconds{1}));
    cache.completeRefresh(tableWithEthernet(), kLuids, kStart + std::chrono::seconds{1});
    EXPECT_FALSE(cache.beginRefreshIfDue(kLuids, kStart + std::chrono::seconds{2}));
}

TEST(AdapterDetailsCache, NewInterfaceAppears_RefreshIsDue)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);
    const std::vector<uint64_t> withNewInterface = {10, 20, 30};

    EXPECT_TRUE(cache.beginRefreshIfDue(withNewInterface, kStart + std::chrono::seconds{1}));
}

TEST(AdapterDetailsCache, InterfaceRemoved_NotDue)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);
    const std::vector<uint64_t> withoutInterface = {10};

    EXPECT_FALSE(cache.beginRefreshIfDue(withoutInterface, kStart + std::chrono::seconds{1}));
}

TEST(AdapterDetailsCache, StaleMarkedDuringRefresh_NextCallStillDue)
{
    AdapterDetailsCache cache;
    ASSERT_TRUE(cache.beginRefreshIfDue(kLuids, kStart));

    // A change notification arrives while the caller is re-reading the details.
    cache.markStale();
    cache.completeRefresh(tableWithEthernet(), kLuids, kStart);

    EXPECT_TRUE(cache.beginRefreshIfDue(kLuids, kStart + std::chrono::seconds{1}));
}

TEST(AdapterDetailsCache, FailedRefresh_ClearsDetailsAndRetries)
{
    AdapterDetailsCache cache;
    refreshAtStart(cache);
    ASSERT_TRUE(cache.beginRefreshIfDue(kLuids, kStart + AdapterDetailsCache::kMaxAge));

    cache.completeRefresh(std::nullopt, kLuids, kStart + AdapterDetailsCache::kMaxAge);

    EXPECT_EQ(cache.table().find(10, 0), nullptr);
    EXPECT_TRUE(cache.beginRefreshIfDue(kLuids, kStart + AdapterDetailsCache::kMaxAge + std::chrono::seconds{1}));
}

TEST(AdapterDetailsTable, Find_UnknownLuid_FallsBackToInterfaceIndex)
{
    const auto table = tableWithEthernet();

    const auto* details = table.find(999, 7);

    ASSERT_NE(details, nullptr);
    EXPECT_EQ(details->friendlyName, "Ethernet");
}

TEST(AdapterDetailsTable, Find_NoMatch_ReturnsNullptr)
{
    const auto table = tableWithEthernet();

    EXPECT_EQ(table.find(999, 999), nullptr);
}
