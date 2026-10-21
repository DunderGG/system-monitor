#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "platform/windows/connectivity_collector.h"
#include "support/fake_subscription.h"
#include "support/log_capture.h"

using namespace verdandi::domain;
using namespace verdandi::platform;
using verdandi::tests::FakeSubscription;

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

TEST(ConnectivityCollector, Collect_ReturnsTheReadersStatusUnchanged)
{
    // The collector never inspects the status: ConnectivityHintMapping turns
    // the hint into one, field by field, and is tested there.
    const ConnectivityStatus read{
        .level = ConnectivityLevel::ConstrainedInternetAccess, .isMetered = std::nullopt, .isRoaming = true};
    ConnectivityCollector collector([read] { return std::optional<ConnectivityStatus>{read}; });

    EXPECT_EQ(collector.collect(), read);
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
    ConnectivityChangeHandler handler{};
    int subscribeCount = 0;
    bool isCancelled = false;
    bool shouldFail = false;
};

ConnectivitySubscriber fakeSubscriber(FakeSubscriptionState& state)
{
    return [&state](ConnectivityChangeHandler handler) -> std::unique_ptr<ConnectivitySubscription> {
        ++state.subscribeCount;
        if (state.shouldFail) {
            return nullptr;
        }
        state.handler = std::move(handler);
        return std::make_unique<FakeSubscription>([&state] { state.isCancelled = true; });
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
    const verdandi::tests::ScopedLogCapture logCapture;

    // The notification runs on an OS thread in production and must not log.
    state.handler(status(ConnectivityLevel::InternetAccess));
    const std::string afterNotification = logCapture.output();
    static_cast<void>(collector.collect());
    static_cast<void>(collector.collect());

    EXPECT_TRUE(afterNotification.empty());
    EXPECT_EQ(logCapture.output(), "info Connectivity changed: level=InternetAccess, metered=no, roaming=unknown\n");
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
        return std::optional<ConnectivityStatus>{
            status(readCount == 1 ? ConnectivityLevel::LocalAccess : ConnectivityLevel::InternetAccess)};
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

TEST(ConnectivityCollector, NotifiedChange_CallsChangeListener)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false), fakeSubscriber(state));
    int calls = 0;
    collector.setChangeListener([&calls] { ++calls; });

    state.handler(status(ConnectivityLevel::InternetAccess));

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(collector.collect().level, ConnectivityLevel::InternetAccess);
}

TEST(ConnectivityCollector, NotifiedSameStatus_DoesNotCallChangeListener)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false), fakeSubscriber(state));
    int calls = 0;
    collector.setChangeListener([&calls] { ++calls; });

    // The initial notification Windows sends usually repeats the seeded status.
    state.handler(status(ConnectivityLevel::LocalAccess));

    EXPECT_EQ(calls, 0);
}

TEST(ConnectivityCollector, ChangeListenerCleared_NoLongerCalled)
{
    FakeSubscriptionState state;
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false), fakeSubscriber(state));
    int calls = 0;
    collector.setChangeListener([&calls] { ++calls; });

    collector.setChangeListener({});
    state.handler(status(ConnectivityLevel::InternetAccess));

    EXPECT_EQ(calls, 0);
}

TEST(ConnectivityCollector, PollingMode_PolledChangeDoesNotCallChangeListener)
{
    ConnectivityLevel level = ConnectivityLevel::LocalAccess;
    ConnectivityCollector collector([&level] { return std::optional<ConnectivityStatus>{status(level)}; });
    int calls = 0;
    collector.setChangeListener([&calls] { ++calls; });
    static_cast<void>(collector.collect());

    level = ConnectivityLevel::InternetAccess;
    const auto polled = collector.collect();

    EXPECT_EQ(polled.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(calls, 0);
}
