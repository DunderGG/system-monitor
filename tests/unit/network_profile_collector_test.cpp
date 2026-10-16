#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "platform/windows/connectivity_collector.h"
#include "platform/windows/network_profile_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

// What the collector did with its reader and subscription. Owned by the test
// and outlives the collector, so the fakes can refer to it by reference.
struct FakeState
{
    ConnectivityChangeHandler handler;
    bool shouldFailSubscribe = false;
    bool isCancelled = false;
    int reads = 0;
    std::optional<std::vector<NetworkProfile>> nextResult = std::vector<NetworkProfile>{};
    std::chrono::steady_clock::time_point now{std::chrono::seconds{1000}};
};

class FakeSubscription : public ConnectivitySubscription
{
public:
    explicit FakeSubscription(FakeState& state) : m_state(state) {}

    ~FakeSubscription() override
    {
        m_state.isCancelled = true;
    }

private:
    FakeState& m_state;
};

NetworkProfileCollector makeCollector(FakeState& state)
{
    return NetworkProfileCollector(
        [&state] {
            ++state.reads;
            return state.nextResult;
        },
        [&state](ConnectivityChangeHandler handler) -> std::unique_ptr<ConnectivitySubscription> {
            if (state.shouldFailSubscribe) {
                return nullptr;
            }
            state.handler = std::move(handler);
            return std::make_unique<FakeSubscription>(state);
        },
        [&state] { return state.now; });
}

std::vector<NetworkProfile> home()
{
    return {{.name = "Home", .category = NetworkCategory::Private, .interfaceIds = {7}}};
}

} // namespace

TEST(NetworkProfileCollector, Construction_SubscribesButDoesNotRead)
{
    FakeState state;

    const auto collector = makeCollector(state);

    EXPECT_TRUE(state.handler);
    EXPECT_EQ(state.reads, 0);
}

TEST(NetworkProfileCollector, FirstCollect_ReadsAndReturnsProfiles)
{
    FakeState state;
    state.nextResult = home();
    auto collector = makeCollector(state);

    const auto profiles = collector.collect();

    EXPECT_EQ(state.reads, 1);
    EXPECT_EQ(profiles, std::optional<std::vector<NetworkProfile>>{home()});
}

TEST(NetworkProfileCollector, NoChange_ReturnsCachedWithoutReading)
{
    FakeState state;
    state.nextResult = home();
    auto collector = makeCollector(state);
    static_cast<void>(collector.collect());
    state.nextResult = std::vector<NetworkProfile>{};
    state.now += NetworkProfileCollector::kMaxAge - std::chrono::seconds{1};

    const auto profiles = collector.collect();

    EXPECT_EQ(state.reads, 1);
    EXPECT_EQ(profiles, std::optional<std::vector<NetworkProfile>>{home()});
}

TEST(NetworkProfileCollector, ConnectivityChange_NextCollectReadsAgain)
{
    FakeState state;
    auto collector = makeCollector(state);
    static_cast<void>(collector.collect());
    state.nextResult = home();

    // On an OS thread in production; it only marks the cache stale.
    state.handler(ConnectivityStatus{.level = ConnectivityLevel::InternetAccess});

    EXPECT_EQ(state.reads, 1);
    EXPECT_EQ(collector.collect(), std::optional<std::vector<NetworkProfile>>{home()});
    EXPECT_EQ(state.reads, 2);
    static_cast<void>(collector.collect());
    EXPECT_EQ(state.reads, 2);
}

TEST(NetworkProfileCollector, MaxAgePassed_ReadsAgain)
{
    FakeState state;
    auto collector = makeCollector(state);
    static_cast<void>(collector.collect());
    state.nextResult = home();
    state.now += NetworkProfileCollector::kMaxAge;

    const auto profiles = collector.collect();

    EXPECT_EQ(state.reads, 2);
    EXPECT_EQ(profiles, std::optional<std::vector<NetworkProfile>>{home()});
}

TEST(NetworkProfileCollector, ReadFails_NulloptRetriedAtNextChangeNotEveryCollect)
{
    FakeState state;
    state.nextResult = std::nullopt;
    auto collector = makeCollector(state);

    EXPECT_FALSE(collector.collect().has_value());
    EXPECT_FALSE(collector.collect().has_value());
    EXPECT_EQ(state.reads, 1);

    state.nextResult = home();
    state.handler(ConnectivityStatus{});

    EXPECT_EQ(collector.collect(), std::optional<std::vector<NetworkProfile>>{home()});
    EXPECT_EQ(state.reads, 2);
}

TEST(NetworkProfileCollector, SubscribeFails_MaxAgeStillRefreshes)
{
    FakeState state;
    state.shouldFailSubscribe = true;
    auto collector = makeCollector(state);
    static_cast<void>(collector.collect());
    state.nextResult = home();

    static_cast<void>(collector.collect());
    EXPECT_EQ(state.reads, 1);
    state.now += NetworkProfileCollector::kMaxAge;

    EXPECT_EQ(collector.collect(), std::optional<std::vector<NetworkProfile>>{home()});
    EXPECT_EQ(state.reads, 2);
}

TEST(NetworkProfileCollector, Destruction_CancelsSubscription)
{
    FakeState state;
    {
        const auto collector = makeCollector(state);
        EXPECT_FALSE(state.isCancelled);
    }

    EXPECT_TRUE(state.isCancelled);
}
