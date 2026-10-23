#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QObject>

#include "domain/connectivity_status.h"
#include "domain/cpu_sample.h"
#include "domain/network_profile.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "support/fake_collectors.h"

// SamplingScheduler: the slow-collector thread, which runs the process,
// connectivity and network-name collectors at their own interval, and how
// their latest results reach each tick's snapshot.

using verdandi::domain::SystemSnapshot;
using verdandi::monitoring::SamplingScheduler;
using verdandi::tests::FakeProcessCollector;

namespace
{

class FakeConnectivityCollector : public verdandi::monitoring::IConnectivityCollector
{
public:
    [[nodiscard]] verdandi::domain::ConnectivityStatus collect() override
    {
        ++m_calls;
        return verdandi::domain::ConnectivityStatus{.level = verdandi::domain::ConnectivityLevel::InternetAccess,
                                                    .isMetered = false};
    }

    void setChangeListener(verdandi::monitoring::ChangeListener /*listener*/) override {}

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

private:
    std::atomic<int> m_calls{0};
};

class FakeNetworkProfileCollector : public verdandi::monitoring::INetworkProfileCollector
{
public:
    [[nodiscard]] std::optional<std::vector<verdandi::domain::NetworkProfile>> collect() override
    {
        ++m_calls;
        return std::vector<verdandi::domain::NetworkProfile>{
            {.name = "Home", .category = verdandi::domain::NetworkCategory::Private, .interfaceIds = {7}},
        };
    }

    void setChangeListener(verdandi::monitoring::ChangeListener /*listener*/) override {}

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

private:
    std::atomic<int> m_calls{0};
};

// Records the thread each tick runs on.
class ThreadRecordingCpuCollector : public verdandi::monitoring::ICpuCollector
{
public:
    [[nodiscard]] std::optional<verdandi::domain::CpuSample> collect() override
    {
        std::lock_guard lock(m_mutex);
        m_lastThread = std::this_thread::get_id();
        return verdandi::domain::CpuSample{.totalUsagePercent = 10.0f, .coreCount = 1};
    }

    void resetBaselines() override {}

    [[nodiscard]] std::thread::id lastThread() const
    {
        std::lock_guard lock(m_mutex);
        return m_lastThread;
    }

private:
    mutable std::mutex m_mutex;
    std::thread::id m_lastThread;
};

} // namespace

TEST(SamplingScheduler, SampleOnce_DoesNotRunSlowCollectors)
{
    SamplingScheduler scheduler;
    auto process = std::make_unique<FakeProcessCollector>();
    auto connectivity = std::make_unique<FakeConnectivityCollector>();
    const auto* processPtr = process.get();
    const auto* connectivityPtr = connectivity.get();
    scheduler.setProcessCollector(std::move(process));
    scheduler.setConnectivityCollector(std::move(connectivity));

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_EQ(processPtr->calls(), 0);
    EXPECT_EQ(connectivityPtr->calls(), 0);
    EXPECT_FALSE(snapshot.processes.has_value());
    EXPECT_EQ(snapshot.connectivity.level, verdandi::domain::ConnectivityLevel::Unknown);
}

TEST(SamplingScheduler, RefreshSlowCollectors_NetworkProfilesCarriedIntoSnapshots)
{
    SamplingScheduler scheduler;
    auto profiles = std::make_unique<FakeNetworkProfileCollector>();
    const auto* profilesPtr = profiles.get();
    scheduler.setNetworkProfileCollector(std::move(profiles));

    const SystemSnapshot before = scheduler.sampleOnce();
    scheduler.refreshSlowCollectors();
    const SystemSnapshot after = scheduler.sampleOnce();

    EXPECT_FALSE(before.networkProfiles.has_value());
    EXPECT_EQ(profilesPtr->calls(), 1);
    ASSERT_TRUE(after.networkProfiles.has_value());
    ASSERT_EQ(after.networkProfiles->size(), 1u);
    EXPECT_EQ((*after.networkProfiles)[0].name, "Home");
    EXPECT_EQ((*after.networkProfiles)[0].interfaceIds, (std::vector<uint64_t>{7}));
}

TEST(SamplingScheduler, RefreshSlowCollectors_NextSnapshotsCarryLatestResults)
{
    SamplingScheduler scheduler;
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* processPtr = process.get();
    scheduler.setProcessCollector(std::move(process));
    scheduler.setConnectivityCollector(std::make_unique<FakeConnectivityCollector>());

    scheduler.refreshSlowCollectors();
    const SystemSnapshot first = scheduler.sampleOnce();
    const SystemSnapshot second = scheduler.sampleOnce();

    EXPECT_EQ(processPtr->calls(), 1);
    ASSERT_TRUE(first.processes.has_value());
    ASSERT_EQ(first.processes->size(), 1u);
    EXPECT_EQ((*first.processes)[0].pid, 1u);
    EXPECT_EQ(first.connectivity.level, verdandi::domain::ConnectivityLevel::InternetAccess);
    ASSERT_TRUE(second.processes.has_value());
    EXPECT_EQ((*second.processes)[0].pid, 1u);
}

TEST(SamplingScheduler, RefreshSlowCollectors_AgainReplacesResults)
{
    SamplingScheduler scheduler;
    scheduler.setProcessCollector(std::make_unique<FakeProcessCollector>());

    scheduler.refreshSlowCollectors();
    scheduler.refreshSlowCollectors();
    const SystemSnapshot snapshot = scheduler.sampleOnce();

    ASSERT_TRUE(snapshot.processes.has_value());
    EXPECT_EQ((*snapshot.processes)[0].pid, 2u);
}

TEST(SamplingScheduler, RefreshSlowCollectors_NoSlowCollectors_NoDataAndUnknownConnectivity)
{
    SamplingScheduler scheduler;

    scheduler.refreshSlowCollectors();
    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_FALSE(snapshot.processes.has_value());
    EXPECT_FALSE(snapshot.networkProfiles.has_value());
    EXPECT_EQ(snapshot.connectivity, verdandi::domain::ConnectivityStatus{});
}

TEST(SamplingScheduler, SlowInterval_DefaultAndSetter)
{
    SamplingScheduler scheduler;
    EXPECT_EQ(scheduler.slowInterval(), SamplingScheduler::kDefaultSlowInterval);

    scheduler.setSlowInterval(std::chrono::milliseconds{3000});

    EXPECT_EQ(scheduler.slowInterval(), std::chrono::milliseconds{3000});
}

TEST(SamplingScheduler, Running_SlowCollectorsRunOnTheirOwnThreadAtTheirOwnCadence)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{10});
    scheduler.setSlowInterval(std::chrono::milliseconds{200});
    auto cpu = std::make_unique<ThreadRecordingCpuCollector>();
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* cpuPtr = cpu.get();
    const auto* processPtr = process.get();
    scheduler.setCpuCollector(std::move(cpu));
    scheduler.setProcessCollector(std::move(process));

    std::atomic<int> snapshotCount{0};
    std::atomic<bool> sawProcesses{false};
    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, [&](const SystemSnapshot& snapshot) {
        ++snapshotCount;
        if (snapshot.processes.has_value()) {
            sawProcesses = true;
        }
    });

    scheduler.start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while ((snapshotCount.load() < 20 || !sawProcesses.load()) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    scheduler.stop();

    EXPECT_TRUE(sawProcesses.load());
    EXPECT_GE(snapshotCount.load(), 20);
    EXPECT_GE(processPtr->calls(), 1);
    // Ticks run every 10 ms and slow refreshes every 200 ms, so far fewer refreshes than ticks.
    EXPECT_LT(processPtr->calls(), snapshotCount.load() / 2);
    EXPECT_NE(processPtr->lastThread(), cpuPtr->lastThread());
    EXPECT_NE(processPtr->lastThread(), std::this_thread::get_id());
}

TEST(SamplingScheduler, Running_SlowIntervalShortened_AppliesToTheWaitInProgress)
{
    SamplingScheduler scheduler(std::chrono::seconds{10});
    scheduler.setSlowInterval(std::chrono::seconds{10});
    auto process = std::make_unique<FakeProcessCollector>();
    const auto* processPtr = process.get();
    scheduler.setProcessCollector(std::move(process));
    const auto waitForRefreshes = [processPtr](int count, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (processPtr->calls() < count && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        return processPtr->calls() >= count;
    };

    scheduler.start();
    const bool isFirstRefreshed = waitForRefreshes(1, std::chrono::seconds{5});
    scheduler.setSlowInterval(std::chrono::milliseconds{50});
    const bool isNextRefreshed = waitForRefreshes(2, std::chrono::seconds{1});
    scheduler.stop();

    EXPECT_TRUE(isFirstRefreshed);
    // Not when the 10 seconds that the slow-collector thread began to wait for end (code review F-46).
    EXPECT_TRUE(isNextRefreshed);
}

namespace
{

class SlowProcessCollector : public verdandi::monitoring::IProcessCollector
{
public:
    [[nodiscard]] std::optional<std::vector<verdandi::domain::ProcessInfo>> collect() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        return std::vector<verdandi::domain::ProcessInfo>{};
    }

    void resetBaselines() override {}
};

} // namespace

TEST(SamplingScheduler, Stop_LongSlowInterval_RespondsImmediately)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{5000});
    scheduler.setSlowInterval(std::chrono::milliseconds{60'000});
    scheduler.setProcessCollector(std::make_unique<SlowProcessCollector>());

    scheduler.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    const auto stopStart = std::chrono::steady_clock::now();
    scheduler.stop();
    const auto stopDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stopStart);

    EXPECT_FALSE(scheduler.isRunning());
    EXPECT_LT(stopDuration.count(), 1000);
}
