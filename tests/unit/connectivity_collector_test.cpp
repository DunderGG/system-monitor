#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "log_capture.h"
#include "platform/windows/connectivity_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

ConnectivityReader fixedReader(ConnectivityLevel level, std::optional<bool> isMetered)
{
    return [level, isMetered] {
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = level,
            .isMetered = isMetered,
        }};
    };
}

} // namespace

TEST(ConnectivityCollector, Collect_InternetAccess_ReturnsInternetAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(status.isMetered, false);
}

TEST(ConnectivityCollector, Collect_MeteredConnection_SetsIsMetered)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, true));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(status.isMetered, true);
}

TEST(ConnectivityCollector, Collect_LocalAccess_ReturnsLocalAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::LocalAccess);
    EXPECT_EQ(status.isMetered, false);
}

TEST(ConnectivityCollector, Collect_CaptivePortal_ReturnsConstrainedInternetAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::ConstrainedInternetAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::ConstrainedInternetAccess);
}

TEST(ConnectivityCollector, Collect_NoConnectivity_ReturnsNone)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::None, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::None);
}

TEST(ConnectivityCollector, Collect_UnknownCost_ReturnsNulloptIsMetered)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, std::nullopt));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_FALSE(status.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderFails_ReturnsUnknown)
{
    ConnectivityCollector collector([] { return std::optional<ConnectivityStatus>{}; });

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(status.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderFailsAfterSuccess_ReturnsUnknownNotStaleStatus)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        if (callCount > 1) {
            return std::optional<ConnectivityStatus>{};
        }
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = ConnectivityLevel::InternetAccess,
            .isMetered = true,
        }};
    });

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(second.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(second.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderRecoversAfterFailure_ReturnsFreshStatus)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        if (callCount == 1) {
            return std::optional<ConnectivityStatus>{};
        }
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = ConnectivityLevel::LocalAccess,
            .isMetered = false,
        }};
    });

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first.level, ConnectivityLevel::Unknown);
    EXPECT_EQ(second.level, ConnectivityLevel::LocalAccess);
    EXPECT_EQ(second.isMetered, false);
}

TEST(ConnectivityCollector, Collect_StatusChangesBetweenTicks_ReflectsEachChange)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        const bool internet = (callCount % 2 == 1);
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = internet ? ConnectivityLevel::InternetAccess : ConnectivityLevel::None,
            .isMetered = false,
        }};
    });

    const auto s1 = collector.collect();
    const auto s2 = collector.collect();
    const auto s3 = collector.collect();

    EXPECT_EQ(s1.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(s2.level, ConnectivityLevel::None);
    EXPECT_EQ(s3.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(callCount, 3);
}

// ---------------------------------------------------------------------------
// Notification path with a fake subscriber (event-driven collector rules)
// ---------------------------------------------------------------------------

namespace
{

// Records what the collector did with its subscription. Owned by the test and
// outlives the collector, so the fakes can refer to it by reference.
struct FakeSubscriptionState
{
    ConnectivityChangeHandler handler;
    int subscribeCount = 0;
    bool isCancelled = false;
    bool shouldFail = false;
};

class FakeSubscription : public ConnectivitySubscription
{
public:
    explicit FakeSubscription(FakeSubscriptionState &state)
        : m_state(state)
    {
    }

    ~FakeSubscription() override
    {
        m_state.isCancelled = true;
    }

private:
    FakeSubscriptionState &m_state;
};

ConnectivitySubscriber fakeSubscriber(FakeSubscriptionState &state)
{
    return [&state](ConnectivityChangeHandler handler) -> std::unique_ptr<ConnectivitySubscription> {
        ++state.subscribeCount;
        if (state.shouldFail) {
            return nullptr;
        }
        state.handler = std::move(handler);
        return std::make_unique<FakeSubscription>(state);
    };
}

ConnectivityStatus status(ConnectivityLevel level)
{
    return ConnectivityStatus{.level = level, .isMetered = false};
}

} // namespace

TEST(ConnectivityCollector, Construction_SeedsFromReaderBeforeSubscribing)
{
    FakeSubscriptionState state;
    int readsBeforeSubscribe = -1;
    int readCount = 0;
    auto reader = [&readCount] {
        ++readCount;
        return std::optional<ConnectivityStatus>{status(ConnectivityLevel::LocalAccess)};
    };
    auto subscriber = [&](ConnectivityChangeHandler handler) {
        readsBeforeSubscribe = readCount;
        return fakeSubscriber(state)(std::move(handler));
    };

    ConnectivityCollector collector(reader, subscriber);

    EXPECT_EQ(readsBeforeSubscribe, 1);
    EXPECT_EQ(collector.collect().level, ConnectivityLevel::LocalAccess);
}

TEST(ConnectivityCollector, Notification_UpdatesCollectedStatus)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false), fakeSubscriber(state));
    ASSERT_TRUE(state.handler);

    state.handler(ConnectivityStatus{.level = ConnectivityLevel::InternetAccess, .isMetered = true});
    const auto collected = collector.collect();

    EXPECT_EQ(collected.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(collected.isMetered, true);
}

TEST(ConnectivityCollector, Notification_StoresOnlyAndCollectLogsTheChange)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false), fakeSubscriber(state));
    ASSERT_TRUE(state.handler);
    static_cast<void>(collector.collect()); // Logs the seeded status.
    const sysmon::tests::ScopedLogCapture logCapture;

    // The notification runs on an OS thread in production and must not log.
    state.handler(status(ConnectivityLevel::InternetAccess));
    const std::string afterNotification = logCapture.output();
    static_cast<void>(collector.collect());
    static_cast<void>(collector.collect());

    EXPECT_TRUE(afterNotification.empty());
    EXPECT_EQ(logCapture.output(), "info Connectivity changed: level=InternetAccess, metered=no\n");
}

TEST(ConnectivityCollector, NotificationPath_CollectDoesNotPollReader)
{
    FakeSubscriptionState state;
    int readCount = 0;
    auto reader = [&readCount] {
        ++readCount;
        return std::optional<ConnectivityStatus>{status(ConnectivityLevel::InternetAccess)};
    };
    ConnectivityCollector collector(reader, fakeSubscriber(state));

    [[maybe_unused]] const auto first = collector.collect();
    [[maybe_unused]] const auto second = collector.collect();

    EXPECT_EQ(readCount, 1); // Only the construction seed.
}

TEST(ConnectivityCollector, SeedFails_UnknownUntilFirstNotification)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector([] { return std::optional<ConnectivityStatus>{}; }, fakeSubscriber(state));

    const auto beforeNotification = collector.collect();
    state.handler(status(ConnectivityLevel::None));
    const auto afterNotification = collector.collect();

    EXPECT_EQ(beforeNotification.level, ConnectivityLevel::Unknown);
    EXPECT_EQ(afterNotification.level, ConnectivityLevel::None);
}

TEST(ConnectivityCollector, Destruction_CancelsSubscription)
{
    FakeSubscriptionState state;
    {
        ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, false), fakeSubscriber(state));
        EXPECT_FALSE(state.isCancelled);
    }

    EXPECT_TRUE(state.isCancelled);
}

TEST(ConnectivityCollector, SubscribeFails_FallsBackToPollingReader)
{
    FakeSubscriptionState state{.shouldFail = true};
    int readCount = 0;
    auto reader = [&readCount] {
        ++readCount;
        return std::optional<ConnectivityStatus>{status(readCount == 1 ? ConnectivityLevel::LocalAccess
                                                                        : ConnectivityLevel::InternetAccess)};
    };

    ConnectivityCollector collector(reader, fakeSubscriber(state));
    const auto collected = collector.collect();

    EXPECT_EQ(state.subscribeCount, 1);
    EXPECT_EQ(readCount, 2); // Seed plus one poll.
    EXPECT_EQ(collected.level, ConnectivityLevel::InternetAccess);
}

TEST(ConnectivityCollector, NotificationsFromAnotherThread_CollectSeesLatestAfterJoin)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::None, false), fakeSubscriber(state));

    {
        std::jthread notifier([&state] {
            for (int i = 0; i < 1000; ++i) {
                state.handler(status(i % 2 == 0 ? ConnectivityLevel::LocalAccess : ConnectivityLevel::None));
            }
            state.handler(status(ConnectivityLevel::InternetAccess));
        });
        for (int i = 0; i < 1000; ++i) {
            [[maybe_unused]] const auto concurrent = collector.collect();
        }
    }

    EXPECT_EQ(collector.collect().level, ConnectivityLevel::InternetAccess);
}
