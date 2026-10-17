#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "platform/windows/connectivity_collector.h"
#include "platform/windows/network_profile_collector.h"
#include "support/fake_subscription.h"

using namespace sysmon::domain;
using namespace sysmon::platform;
using sysmon::tests::FakeSubscription;
using namespace std::chrono_literals;

namespace
{

// Long enough for the worker to act on a busy machine, and only reached when a test fails.
constexpr auto kTimeout = 5s;
// Intervals that leave every read after the first to notifications unless a test shortens them.
constexpr NetworkProfileCollector::ReadIntervals kOnlyOnNotification{.minimum = 0ms, .maximumAge = 1h};

// What the collector did with its reader and subscription, and what the
// reader returns. Owned by the test and outlives the collector. The worker
// reads on its own thread, so every field is guarded by mutex.
struct FakeState
{
    std::mutex mutex;
    std::condition_variable changed;
    ConnectivityChangeHandler handler;
    bool shouldFailSubscribe = false;
    bool isCancelled = false;
    int reads = 0;
    // Reads with a higher number wait until the test allows them.
    int readsAllowed = std::numeric_limits<int>::max();
    std::optional<std::vector<NetworkProfile>> nextResult = std::vector<NetworkProfile>{};
    // Called during each read, without the mutex held, with the read's number.
    std::function<void(int)> duringRead;
    std::thread::id readerCreatedOn;
    std::thread::id readOn;
    std::thread::id readerDestroyedOn;
    int listenerCalls = 0;

    // Waits until predicate, called with the mutex held, is true. Returns false on a timeout.
    template <typename Predicate> bool waitUntil(Predicate predicate)
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, kTimeout, predicate);
    }

    void allowReads(int count)
    {
        const std::lock_guard lock(mutex);
        readsAllowed = count;
        changed.notify_all();
    }

    // Delivers a connectivity change, as the OS does on a thread of its own.
    void notify()
    {
        ConnectivityChangeHandler copy;
        {
            const std::lock_guard lock(mutex);
            copy = handler;
        }
        copy(ConnectivityStatus{.level = ConnectivityLevel::InternetAccess});
    }
};

// Allows every read when it goes out of scope. Declared after the collector,
// so a test that holds reads back releases them before the collector's
// destructor waits for the read in progress, even when an assertion fails.
class ReleaseReadsOnExit
{
public:
    explicit ReleaseReadsOnExit(FakeState& state) : m_state(state) {}

    ~ReleaseReadsOnExit()
    {
        m_state.allowReads(std::numeric_limits<int>::max());
    }

    ReleaseReadsOnExit(const ReleaseReadsOnExit&) = delete;
    ReleaseReadsOnExit& operator=(const ReleaseReadsOnExit&) = delete;

private:
    FakeState& m_state;
};

// Records the thread it is destroyed on.
class DestructionWitness
{
public:
    explicit DestructionWitness(FakeState& state) : m_state(state) {}

    ~DestructionWitness()
    {
        const std::lock_guard lock(m_state.mutex);
        m_state.readerDestroyedOn = std::this_thread::get_id();
        m_state.changed.notify_all();
    }

    DestructionWitness(const DestructionWitness&) = delete;
    DestructionWitness& operator=(const DestructionWitness&) = delete;

private:
    FakeState& m_state;
};

NetworkProfileReader makeReader(FakeState& state)
{
    const auto witness = std::make_shared<DestructionWitness>(state);
    return [&state, witness] {
        int read = 0;
        std::function<void(int)> duringRead;
        {
            std::unique_lock lock(state.mutex);
            state.readOn = std::this_thread::get_id();
            read = ++state.reads;
            state.changed.notify_all();
            state.changed.wait(lock, [&state, read] { return read <= state.readsAllowed; });
            duringRead = state.duringRead;
        }
        if (duringRead) {
            duringRead(read);
        }
        const std::lock_guard lock(state.mutex);
        return state.nextResult;
    };
}

std::unique_ptr<NetworkProfileCollector>
makeCollector(FakeState& state, NetworkProfileCollector::ReadIntervals intervals = kOnlyOnNotification)
{
    auto collector = std::make_unique<NetworkProfileCollector>(
        [&state] {
            {
                const std::lock_guard lock(state.mutex);
                state.readerCreatedOn = std::this_thread::get_id();
            }
            return makeReader(state);
        },
        [&state](ConnectivityChangeHandler handler) -> std::unique_ptr<ConnectivitySubscription> {
            const std::lock_guard lock(state.mutex);
            if (state.shouldFailSubscribe) {
                return nullptr;
            }
            state.handler = std::move(handler);
            return std::make_unique<FakeSubscription>([&state] {
                const std::lock_guard cancelLock(state.mutex);
                state.isCancelled = true;
            });
        },
        intervals);
    return collector;
}

void countListenerCalls(NetworkProfileCollector& collector, FakeState& state)
{
    collector.setChangeListener([&state] {
        const std::lock_guard lock(state.mutex);
        ++state.listenerCalls;
        state.changed.notify_all();
    });
}

// Waits until the collector has finished count reads.
bool waitForReads(const NetworkProfileCollector& collector, std::uint64_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + kTimeout;
    while (collector.readCount() < count && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    return collector.readCount() >= count;
}

std::vector<NetworkProfile> home()
{
    return {{.name = "Home", .category = NetworkCategory::Private, .interfaceIds = {7}}};
}

} // namespace

TEST(NetworkProfileCollector, Construction_SubscribesAndReadsAtOnceOnItsOwnThread)
{
    FakeState state;

    const auto collector = makeCollector(state);

    ASSERT_TRUE(waitForReads(*collector, 1));
    const std::lock_guard lock(state.mutex);
    EXPECT_TRUE(state.handler);
    EXPECT_EQ(state.reads, 1);
    EXPECT_NE(state.readOn, std::this_thread::get_id());
    EXPECT_EQ(state.readerCreatedOn, state.readOn);
}

TEST(NetworkProfileCollector, BeforeTheFirstReadFinishes_CollectReturnsNullopt)
{
    FakeState state;
    state.readsAllowed = 0;
    state.nextResult = home();
    const auto collector = makeCollector(state);
    const ReleaseReadsOnExit release(state);
    ASSERT_TRUE(state.waitUntil([&state] { return state.reads == 1; }));

    EXPECT_FALSE(collector->collect().has_value());
    EXPECT_EQ(collector->readCount(), 0u);
}

TEST(NetworkProfileCollector, FirstRead_StoresProfilesAndCallsTheListener)
{
    FakeState state;
    state.readsAllowed = 0;
    state.nextResult = home();
    const auto collector = makeCollector(state);
    const ReleaseReadsOnExit release(state);
    countListenerCalls(*collector, state);

    state.allowReads(1);

    ASSERT_TRUE(state.waitUntil([&state] { return state.listenerCalls == 1; }));
    EXPECT_EQ(collector->collect(), std::optional<std::vector<NetworkProfile>>{home()});
}

TEST(NetworkProfileCollector, NoChange_NoFurtherRead)
{
    FakeState state;
    const auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(collector->readCount(), 1u);
}

TEST(NetworkProfileCollector, ConnectivityChange_ReadsAgainAndCallsTheListenerWithNewProfiles)
{
    FakeState state;
    const auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));
    countListenerCalls(*collector, state);
    {
        const std::lock_guard lock(state.mutex);
        state.nextResult = home();
    }

    state.notify();

    ASSERT_TRUE(state.waitUntil([&state] { return state.listenerCalls == 1; }));
    EXPECT_EQ(collector->collect(), std::optional<std::vector<NetworkProfile>>{home()});
    EXPECT_EQ(collector->readCount(), 2u);
}

TEST(NetworkProfileCollector, ConnectivityChange_SameProfiles_ListenerNotCalled)
{
    FakeState state;
    const auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));
    countListenerCalls(*collector, state);

    state.notify();

    ASSERT_TRUE(waitForReads(*collector, 2));
    const std::lock_guard lock(state.mutex);
    EXPECT_EQ(state.listenerCalls, 0);
}

TEST(NetworkProfileCollector, ChangeReportedDuringARead_ReadsAgain)
{
    FakeState state;
    // The first read is in progress when the change arrives. The stale mark is
    // cleared before a read starts, so this change must cause a second read.
    state.duringRead = [&state](int read) {
        if (read == 1) {
            state.notify();
        }
    };

    const auto collector = makeCollector(state);

    EXPECT_TRUE(waitForReads(*collector, 2));
}

TEST(NetworkProfileCollector, NotificationBurst_ReadsAtMostOncePerMinimumInterval)
{
    FakeState state;
    const auto collector = makeCollector(state, {.minimum = 200ms, .maximumAge = 1h});
    ASSERT_TRUE(waitForReads(*collector, 1));

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < 500ms) {
        state.notify();
        std::this_thread::sleep_for(1ms);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // Hundreds of notifications; at most one read per started 200 ms after the
    // first, and at least the one the first notification made due.
    const auto allowed = 1 + elapsed / 200ms + 1;
    EXPECT_LE(collector->readCount(), static_cast<std::uint64_t>(allowed));
    EXPECT_TRUE(waitForReads(*collector, 2));
}

TEST(NetworkProfileCollector, MaximumAgePassed_ReadsAgain)
{
    FakeState state;

    const auto collector = makeCollector(state, {.minimum = 0ms, .maximumAge = 20ms});

    EXPECT_TRUE(waitForReads(*collector, 3));
}

TEST(NetworkProfileCollector, ReadFails_NulloptUntilTheNextChange)
{
    FakeState state;
    state.nextResult = std::nullopt;
    const auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));
    EXPECT_FALSE(collector->collect().has_value());
    countListenerCalls(*collector, state);
    {
        const std::lock_guard lock(state.mutex);
        state.nextResult = home();
    }

    state.notify();

    ASSERT_TRUE(state.waitUntil([&state] { return state.listenerCalls == 1; }));
    EXPECT_EQ(collector->collect(), std::optional<std::vector<NetworkProfile>>{home()});
}

TEST(NetworkProfileCollector, SubscribeFails_MaximumAgeStillRefreshes)
{
    FakeState state;
    state.shouldFailSubscribe = true;

    const auto collector = makeCollector(state, {.minimum = 0ms, .maximumAge = 20ms});

    EXPECT_TRUE(waitForReads(*collector, 2));
}

TEST(NetworkProfileCollector, Destruction_CancelsSubscriptionAndDestroysTheReaderOnItsThread)
{
    FakeState state;
    std::thread::id readThread;
    {
        const auto collector = makeCollector(state);
        ASSERT_TRUE(waitForReads(*collector, 1));
        const std::lock_guard lock(state.mutex);
        EXPECT_FALSE(state.isCancelled);
        readThread = state.readOn;
    }

    const std::lock_guard lock(state.mutex);
    EXPECT_TRUE(state.isCancelled);
    EXPECT_EQ(state.readerDestroyedOn, readThread);
}

TEST(NetworkProfileCollector, Destruction_WaitingForTheNextRead_ReturnsAtOnce)
{
    FakeState state;
    auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));

    const auto start = std::chrono::steady_clock::now();
    collector.reset();

    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(NetworkProfileCollector, ChangeListenerCleared_NoLongerCalled)
{
    FakeState state;
    const auto collector = makeCollector(state);
    ASSERT_TRUE(waitForReads(*collector, 1));
    countListenerCalls(*collector, state);
    collector->setChangeListener({});
    {
        const std::lock_guard lock(state.mutex);
        state.nextResult = home();
    }

    state.notify();

    ASSERT_TRUE(waitForReads(*collector, 2));
    const std::lock_guard lock(state.mutex);
    EXPECT_EQ(state.listenerCalls, 0);
}
