#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QObject>

#include "domain/connectivity_status.h"
#include "domain/network_profile.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "support/fake_collectors.h"

// SamplingScheduler: event-driven collectors, whose change listener wakes the
// slow-collector thread to collect them again without a full slow refresh.

using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::tests::FakeProcessCollector;

namespace
{

// State shared by a test and its FakeEventDrivenConnectivityCollector, which
// the scheduler owns. It outlives the scheduler, so the test can check what the
// scheduler left behind.
struct EventDrivenState
{
    std::mutex mutex;
    sysmon::domain::ConnectivityStatus status{.level = sysmon::domain::ConnectivityLevel::InternetAccess};
    sysmon::monitoring::ChangeListener listener;
    int collectCalls = 0;
};

class FakeEventDrivenConnectivityCollector : public sysmon::monitoring::IConnectivityCollector
{
public:
    explicit FakeEventDrivenConnectivityCollector(EventDrivenState& state) : m_state(state) {}

    [[nodiscard]] sysmon::domain::ConnectivityStatus collect() override
    {
        std::lock_guard lock(m_state.mutex);
        ++m_state.collectCalls;
        return m_state.status;
    }

    void setChangeListener(sysmon::monitoring::ChangeListener listener) override
    {
        std::lock_guard lock(m_state.mutex);
        m_state.listener = std::move(listener);
    }

private:
    EventDrivenState& m_state;
};

// Stores a new status and calls the listener, as an OS notification does.
void notifyChange(EventDrivenState& state, sysmon::domain::ConnectivityLevel level)
{
    std::lock_guard lock(state.mutex);
    state.status = sysmon::domain::ConnectivityStatus{.level = level};
    if (state.listener) {
        state.listener();
    }
}

} // namespace

TEST(SamplingScheduler, EventDrivenCollector_ListenerSetOnRegistrationAndClearedOnDestruction)
{
    EventDrivenState state;
    {
        SamplingScheduler scheduler;
        scheduler.setConnectivityCollector(std::make_unique<FakeEventDrivenConnectivityCollector>(state));
        {
            std::lock_guard lock(state.mutex);
            EXPECT_TRUE(static_cast<bool>(state.listener));
        }
    }

    std::lock_guard lock(state.mutex);
    EXPECT_FALSE(static_cast<bool>(state.listener));
}

TEST(SamplingScheduler, RefreshEventDrivenCollectors_UpdatesConnectivityAndKeepsProcessList)
{
    SamplingScheduler scheduler;
    EventDrivenState state;
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* processPtr = process.get();
    scheduler.setProcessCollector(std::move(process));
    scheduler.setConnectivityCollector(std::make_unique<FakeEventDrivenConnectivityCollector>(state));
    scheduler.refreshSlowCollectors();
    notifyChange(state, sysmon::domain::ConnectivityLevel::LocalAccess);

    scheduler.refreshEventDrivenCollectors();
    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_EQ(snapshot.connectivity.level, sysmon::domain::ConnectivityLevel::LocalAccess);
    ASSERT_TRUE(snapshot.processes.has_value());
    EXPECT_EQ((*snapshot.processes)[0].pid, 1u);
    EXPECT_EQ(processPtr->calls(), 1);
}

TEST(SamplingScheduler, Running_ChangeNotification_ReachesNextSnapshotWithoutAFullSlowRefresh)
{
    using sysmon::domain::ConnectivityLevel;
    SamplingScheduler scheduler(std::chrono::milliseconds{10});
    scheduler.setSlowInterval(std::chrono::seconds{60});
    EventDrivenState state;
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* processPtr = process.get();
    scheduler.setProcessCollector(std::move(process));
    scheduler.setConnectivityCollector(std::make_unique<FakeEventDrivenConnectivityCollector>(state));
    std::atomic<ConnectivityLevel> lastLevel{ConnectivityLevel::Unknown};
    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady,
                     [&lastLevel](const SystemSnapshot& snapshot) { lastLevel = snapshot.connectivity.level; });
    const auto waitFor = [&lastLevel](ConnectivityLevel level) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (lastLevel.load() != level && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    };
    scheduler.start();
    waitFor(ConnectivityLevel::InternetAccess); // The first full slow refresh.

    const auto notified = std::chrono::steady_clock::now();
    notifyChange(state, ConnectivityLevel::LocalAccess);
    waitFor(ConnectivityLevel::LocalAccess);
    const auto latency = std::chrono::steady_clock::now() - notified;
    scheduler.stop();

    EXPECT_EQ(lastLevel.load(), ConnectivityLevel::LocalAccess);
    // Within a few ticks, not the 60-second slow interval.
    EXPECT_LT(latency, std::chrono::seconds{2});
    EXPECT_EQ(processPtr->calls(), 1);
}

namespace
{

// State shared by a test and its FakeEventDrivenNetworkProfileCollector, as
// EventDrivenState is for connectivity.
struct EventDrivenProfileState
{
    std::mutex mutex;
    std::optional<std::vector<sysmon::domain::NetworkProfile>> profiles = std::vector<sysmon::domain::NetworkProfile>{};
    sysmon::monitoring::ChangeListener listener;
};

class FakeEventDrivenNetworkProfileCollector : public sysmon::monitoring::INetworkProfileCollector
{
public:
    explicit FakeEventDrivenNetworkProfileCollector(EventDrivenProfileState& state) : m_state(state) {}

    [[nodiscard]] std::optional<std::vector<sysmon::domain::NetworkProfile>> collect() override
    {
        std::lock_guard lock(m_state.mutex);
        return m_state.profiles;
    }

    void setChangeListener(sysmon::monitoring::ChangeListener listener) override
    {
        std::lock_guard lock(m_state.mutex);
        m_state.listener = std::move(listener);
    }

private:
    EventDrivenProfileState& m_state;
};

// Stores new names and calls the listener, as the collector's worker does after a read.
void notifyProfiles(EventDrivenProfileState& state, std::string name)
{
    std::lock_guard lock(state.mutex);
    state.profiles = std::vector<sysmon::domain::NetworkProfile>{{.name = std::move(name), .interfaceIds = {1}}};
    if (state.listener) {
        state.listener();
    }
}

std::optional<std::string> firstProfileName(const SystemSnapshot& snapshot)
{
    if (!snapshot.networkProfiles || snapshot.networkProfiles->empty()) {
        return std::nullopt;
    }
    return snapshot.networkProfiles->front().name;
}

// Records, for each call, how many times the connectivity collector had been
// collected when it began. Its first call waits until the test releases it.
class GatedProcessCollector : public sysmon::monitoring::IProcessCollector
{
public:
    explicit GatedProcessCollector(EventDrivenState& connectivity) : m_connectivity(connectivity) {}

    [[nodiscard]] std::optional<std::vector<sysmon::domain::ProcessInfo>> collect() override
    {
        int connectivityCalls = 0;
        {
            std::lock_guard lock(m_connectivity.mutex);
            connectivityCalls = m_connectivity.collectCalls;
        }
        std::unique_lock lock(m_mutex);
        m_connectivityCallsAtEachCall.push_back(connectivityCalls);
        m_changed.notify_all();
        if (m_connectivityCallsAtEachCall.size() == 1) {
            m_changed.wait(lock, [this] { return m_isReleased; });
        }
        return std::vector<sysmon::domain::ProcessInfo>{};
    }

    // Waits until the collector has been called count times. Returns false on a timeout.
    bool waitForCalls(std::size_t count)
    {
        std::unique_lock lock(m_mutex);
        return m_changed.wait_for(lock, std::chrono::seconds{5},
                                  [this, count] { return m_connectivityCallsAtEachCall.size() >= count; });
    }

    void release()
    {
        std::lock_guard lock(m_mutex);
        m_isReleased = true;
        m_changed.notify_all();
    }

    [[nodiscard]] std::vector<int> connectivityCallsAtEachCall() const
    {
        std::lock_guard lock(m_mutex);
        return m_connectivityCallsAtEachCall;
    }

private:
    EventDrivenState& m_connectivity;
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<int> m_connectivityCallsAtEachCall;
    bool m_isReleased{false};
};

} // namespace

TEST(SamplingScheduler, Running_NetworkNamesChange_ReachNextSnapshotWithoutAFullSlowRefresh)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{10});
    scheduler.setSlowInterval(std::chrono::seconds{60});
    EventDrivenProfileState state;
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* processPtr = process.get();
    scheduler.setProcessCollector(std::move(process));
    scheduler.setNetworkProfileCollector(std::make_unique<FakeEventDrivenNetworkProfileCollector>(state));
    std::mutex nameMutex;
    std::optional<std::string> lastName;
    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, [&](const SystemSnapshot& snapshot) {
        std::lock_guard lock(nameMutex);
        lastName = firstProfileName(snapshot);
    });
    const auto waitForName = [&](const std::string& name) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard lock(nameMutex);
                if (lastName == name) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return false;
    };
    state.profiles = std::vector<sysmon::domain::NetworkProfile>{{.name = "Home", .interfaceIds = {1}}};
    scheduler.start();
    const bool firstRefresh = waitForName("Home");

    notifyProfiles(state, "Normandy");
    const bool reached = waitForName("Normandy");
    scheduler.stop();

    EXPECT_TRUE(firstRefresh);
    EXPECT_TRUE(reached);
    EXPECT_EQ(processPtr->calls(), 1);
}

TEST(SamplingScheduler, Running_ChangeWhileAFullRefreshIsDue_AnsweredByTheFullRefreshAlone)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{1000});
    scheduler.setSlowInterval(std::chrono::milliseconds{100});
    EventDrivenState state;
    auto process = std::make_unique<GatedProcessCollector>(state);
    auto* processPtr = process.get();
    scheduler.setConnectivityCollector(std::make_unique<FakeEventDrivenConnectivityCollector>(state));
    scheduler.setProcessCollector(std::move(process));
    scheduler.start();

    // The first full refresh is held in the process collector while a change
    // arrives, until the next full refresh is due.
    const bool firstRefresh = processPtr->waitForCalls(1);
    notifyChange(state, sysmon::domain::ConnectivityLevel::LocalAccess);
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    processPtr->release();
    const bool secondRefresh = processPtr->waitForCalls(2);
    scheduler.stop();

    // The next full refresh answers the pending change: had an event refresh
    // run before it, the connectivity collector would have been collected
    // three times by the time the process collector was called again.
    ASSERT_TRUE(firstRefresh && secondRefresh);
    const auto calls = processPtr->connectivityCallsAtEachCall();
    EXPECT_EQ(calls[0], 1);
    EXPECT_EQ(calls[1], 2);
}

TEST(SamplingScheduler, Teardown_WhileNotificationsArrive_NoDeadlock)
{
    // Aborts the test run if the rounds deadlock, rather than hanging it.
    std::mutex watchdogMutex;
    std::condition_variable_any watchdogCv;
    bool isDone = false;
    std::jthread watchdog([&](std::stop_token stopToken) {
        std::unique_lock lock(watchdogMutex);
        if (!watchdogCv.wait_for(lock, stopToken, std::chrono::seconds{60}, [&isDone] { return isDone; }) &&
            !stopToken.stop_requested()) {
            std::fputs("SamplingScheduler teardown deadlocked\n", stderr);
            std::abort();
        }
    });

    // Delivers notifications from another thread for as long as the rounds
    // run, as the OS does while an adapter reconnects.
    EventDrivenState connectivity;
    EventDrivenProfileState profiles;
    std::jthread notifier([&](std::stop_token stopToken) {
        int count = 0;
        while (!stopToken.stop_requested()) {
            ++count;
            notifyChange(connectivity, count % 2 == 0 ? sysmon::domain::ConnectivityLevel::LocalAccess
                                                      : sysmon::domain::ConnectivityLevel::InternetAccess);
            notifyProfiles(profiles, count % 2 == 0 ? "Home" : "Office");
        }
    });

    // Creates, starts, stops and destroys schedulers at random points.
    std::mt19937 random(42);
    std::uniform_int_distribution<int> delayMs(0, 4);
    for (int round = 0; round < 40; ++round) {
        SamplingScheduler scheduler(std::chrono::milliseconds{5});
        scheduler.setSlowInterval(std::chrono::milliseconds{10});
        scheduler.setConnectivityCollector(std::make_unique<FakeEventDrivenConnectivityCollector>(connectivity));
        scheduler.setNetworkProfileCollector(std::make_unique<FakeEventDrivenNetworkProfileCollector>(profiles));
        scheduler.setProcessCollector(std::make_unique<FakeProcessCollector>());
        std::this_thread::sleep_for(std::chrono::milliseconds{delayMs(random)});
        scheduler.start();
        std::this_thread::sleep_for(std::chrono::milliseconds{delayMs(random)});
        if (round % 2 == 0) {
            scheduler.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds{delayMs(random)});
        }
    }
    notifier.request_stop();
    notifier.join();

    {
        std::lock_guard lock(watchdogMutex);
        isDone = true;
    }
    watchdogCv.notify_all();
    std::lock_guard lock(connectivity.mutex);
    EXPECT_FALSE(static_cast<bool>(connectivity.listener));
}
