#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QObject>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "monitoring/synthetic_cpu_collector.h"
#include "monitoring/synthetic_memory_collector.h"

using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;
using sysmon::monitoring::SyntheticCpuCollector;
using sysmon::monitoring::SyntheticMemoryCollector;

TEST(SamplingScheduler, SampleOnce_AssemblesRegisteredCollectors)
{
    SamplingScheduler scheduler;

    scheduler.setCpuCollector(std::make_unique<SyntheticCpuCollector>(6, 45.0f));
    scheduler.setMemoryCollector(std::make_unique<SyntheticMemoryCollector>());

    const auto before = std::chrono::steady_clock::now();
    const SystemSnapshot snapshot = scheduler.sampleOnce();
    const auto after = std::chrono::steady_clock::now();

    EXPECT_GE(snapshot.timestamp, before);
    EXPECT_LE(snapshot.timestamp, after);
    ASSERT_TRUE(snapshot.cpu.has_value());
    EXPECT_EQ(snapshot.cpu->coreCount, 6);
    ASSERT_EQ(snapshot.cpu->coreUsagePercents.size(), 6u);
    ASSERT_TRUE(snapshot.memory.has_value());
    EXPECT_GT(snapshot.memory->totalBytes, 0ULL);
    EXPECT_FALSE(snapshot.disks.has_value());
    EXPECT_FALSE(snapshot.networks.has_value());
    EXPECT_FALSE(snapshot.processes.has_value());
}

TEST(SamplingScheduler, SampleOnce_MissingCollectors_ProduceNoData)
{
    SamplingScheduler scheduler;

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_FALSE(snapshot.cpu.has_value());
    EXPECT_FALSE(snapshot.memory.has_value());
    EXPECT_FALSE(snapshot.disks.has_value());
    EXPECT_FALSE(snapshot.networks.has_value());
    EXPECT_FALSE(snapshot.processes.has_value());
}

TEST(SamplingScheduler, StartAndStop_TransitionsRunningStateCleanly)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{50});

    EXPECT_FALSE(scheduler.isRunning());

    scheduler.start();
    EXPECT_TRUE(scheduler.isRunning());

    scheduler.stop();
    EXPECT_FALSE(scheduler.isRunning());
}

TEST(SamplingScheduler, EmitsSnapshotReadySignal_ReceivedViaQtConnection)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{20});

    scheduler.setCpuCollector(std::make_unique<SyntheticCpuCollector>(4));
    scheduler.setMemoryCollector(std::make_unique<SyntheticMemoryCollector>());

    std::atomic<int> snapshotCount{0};
    SystemSnapshot lastSnapshot;
    std::mutex snapMutex;

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, [&](const SystemSnapshot& snapshot) {
        std::lock_guard lock(snapMutex);
        lastSnapshot = snapshot;
        ++snapshotCount;
    });

    scheduler.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{1000};
    while (snapshotCount.load() < 2 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    EXPECT_GE(snapshotCount.load(), 2);
    {
        std::lock_guard lock(snapMutex);
        ASSERT_TRUE(lastSnapshot.cpu.has_value());
        EXPECT_EQ(lastSnapshot.cpu->coreCount, 4);
        ASSERT_TRUE(lastSnapshot.memory.has_value());
        EXPECT_GT(lastSnapshot.memory->totalBytes, 0ULL);
    }
}

TEST(SamplingScheduler, FastShutdown_RespondsImmediatelyToStop)
{
    // Configure a long 5-second interval
    SamplingScheduler scheduler(std::chrono::milliseconds{5000});

    scheduler.start();
    std::this_thread::sleep_for(std::chrono::milliseconds{30});

    const auto stopStart = std::chrono::steady_clock::now();
    scheduler.stop();
    const auto stopDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stopStart);

    EXPECT_FALSE(scheduler.isRunning());
    // Stop should complete rapidly, not wait out the 5000ms tick interval
    EXPECT_LT(stopDuration.count(), 1000);
}

namespace
{

class MutexReentryProbeCollector : public sysmon::monitoring::ICpuCollector
{
public:
    explicit MutexReentryProbeCollector(SamplingScheduler& scheduler) : m_scheduler(scheduler) {}

    [[nodiscard]] std::optional<sysmon::domain::CpuSample> collect() override
    {
        // Calling setDiskCollector acquires m_collectorMutex.
        // If sampleOnce() held m_collectorMutex during collect(), this non-recursive mutex would deadlock.
        m_scheduler.setDiskCollector(nullptr);
        m_called = true;

        sysmon::domain::CpuSample sample;
        sample.coreCount = 8;
        return sample;
    }

    [[nodiscard]] bool wasCalled() const
    {
        return m_called;
    }

private:
    SamplingScheduler& m_scheduler;
    bool m_called{false};
};

} // namespace

TEST(SamplingScheduler, SampleOnce_ReleasesCollectorMutexBeforeCollection)
{
    SamplingScheduler scheduler;
    auto probe = std::make_unique<MutexReentryProbeCollector>(scheduler);
    const auto* probePtr = probe.get();
    scheduler.setCpuCollector(std::move(probe));

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_TRUE(probePtr->wasCalled());
    ASSERT_TRUE(snapshot.cpu.has_value());
    EXPECT_EQ(snapshot.cpu->coreCount, 8);
}

namespace
{

class FakeUptimeCollector : public sysmon::monitoring::IUptimeCollector
{
public:
    explicit FakeUptimeCollector(std::chrono::milliseconds uptime) : m_uptime(uptime) {}

    [[nodiscard]] std::chrono::milliseconds collect() override
    {
        return m_uptime;
    }

private:
    std::chrono::milliseconds m_uptime;
};

} // namespace

TEST(SamplingScheduler, SampleOnce_UptimeCollectorRegistered_SetsUptime)
{
    SamplingScheduler scheduler;
    scheduler.setUptimeCollector(std::make_unique<FakeUptimeCollector>(std::chrono::milliseconds{90'000}));

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_EQ(snapshot.uptime, std::chrono::milliseconds{90'000});
}

TEST(SamplingScheduler, SampleOnce_NoUptimeCollector_UptimeIsNullopt)
{
    SamplingScheduler scheduler;

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_FALSE(snapshot.uptime.has_value());
}

namespace
{

class FixedCpuCollector : public sysmon::monitoring::ICpuCollector
{
public:
    explicit FixedCpuCollector(float usagePercent) : m_usagePercent(usagePercent) {}

    [[nodiscard]] std::optional<sysmon::domain::CpuSample> collect() override
    {
        return sysmon::domain::CpuSample{.totalUsagePercent = m_usagePercent, .coreCount = 1};
    }

private:
    float m_usagePercent;
};

} // namespace

TEST(SamplingScheduler, SampleOnce_NoCollectors_HealthAllUnknown)
{
    SamplingScheduler scheduler;

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_EQ(snapshot.health, sysmon::domain::SystemHealth{});
}

TEST(SamplingScheduler, SampleOnce_EvaluatesHealthWithConfiguredThresholds)
{
    SamplingScheduler scheduler;
    scheduler.setCpuCollector(std::make_unique<FixedCpuCollector>(50.0f));
    scheduler.setHealthThresholds(sysmon::monitoring::HealthThresholds{
        .cpu = sysmon::monitoring::UsageThresholds{.warningPercent = 40.0f, .criticalPercent = 60.0f},
    });

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_EQ(snapshot.health.cpu, sysmon::domain::HealthLevel::Warning);
}

namespace
{

class FakeSystemActivityCollector : public sysmon::monitoring::ISystemActivityCollector
{
public:
    [[nodiscard]] std::optional<sysmon::domain::SystemActivitySample> collect() override
    {
        return sysmon::domain::SystemActivitySample{.processCount = 3, .threadCount = 30, .handleCount = 300};
    }
};

} // namespace

TEST(SamplingScheduler, SampleOnce_SystemActivityCollectorRegistered_SetsActivity)
{
    SamplingScheduler scheduler;
    scheduler.setSystemActivityCollector(std::make_unique<FakeSystemActivityCollector>());

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    ASSERT_TRUE(snapshot.activity.has_value());
    EXPECT_EQ(snapshot.activity->handleCount, 300u);
}

TEST(SamplingScheduler, SampleOnce_NoSystemActivityCollector_ActivityIsNullopt)
{
    SamplingScheduler scheduler;

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    EXPECT_FALSE(snapshot.activity.has_value());
}

namespace
{

// Counts its calls and records the thread of the last one.
class FakeProcessCollector : public sysmon::monitoring::IProcessCollector
{
public:
    [[nodiscard]] std::optional<std::vector<sysmon::domain::ProcessInfo>> collect() override
    {
        {
            std::lock_guard lock(m_mutex);
            m_lastThread = std::this_thread::get_id();
        }
        const int call = ++m_calls;
        return std::vector<sysmon::domain::ProcessInfo>{
            sysmon::domain::ProcessInfo{.pid = static_cast<uint32_t>(call), .imageName = "app.exe"}};
    }

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

    [[nodiscard]] std::thread::id lastThread() const
    {
        std::lock_guard lock(m_mutex);
        return m_lastThread;
    }

private:
    std::atomic<int> m_calls{0};
    mutable std::mutex m_mutex;
    std::thread::id m_lastThread;
};

class FakeConnectivityCollector : public sysmon::monitoring::IConnectivityCollector
{
public:
    [[nodiscard]] sysmon::domain::ConnectivityStatus collect() override
    {
        ++m_calls;
        return sysmon::domain::ConnectivityStatus{.level = sysmon::domain::ConnectivityLevel::InternetAccess,
                                                  .isMetered = false};
    }

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

private:
    std::atomic<int> m_calls{0};
};

class FakeNetworkProfileCollector : public sysmon::monitoring::INetworkProfileCollector
{
public:
    [[nodiscard]] std::optional<std::vector<sysmon::domain::NetworkProfile>> collect() override
    {
        ++m_calls;
        return std::vector<sysmon::domain::NetworkProfile>{
            {.name = "Home", .category = sysmon::domain::NetworkCategory::Private, .interfaceIds = {7}},
        };
    }

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

private:
    std::atomic<int> m_calls{0};
};

// Records the thread each tick runs on.
class ThreadRecordingCpuCollector : public sysmon::monitoring::ICpuCollector
{
public:
    [[nodiscard]] std::optional<sysmon::domain::CpuSample> collect() override
    {
        std::lock_guard lock(m_mutex);
        m_lastThread = std::this_thread::get_id();
        return sysmon::domain::CpuSample{.totalUsagePercent = 10.0f, .coreCount = 1};
    }

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
    EXPECT_EQ(snapshot.connectivity.level, sysmon::domain::ConnectivityLevel::Unknown);
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
    EXPECT_EQ(first.connectivity.level, sysmon::domain::ConnectivityLevel::InternetAccess);
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
    EXPECT_EQ(snapshot.connectivity, sysmon::domain::ConnectivityStatus{});
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

namespace
{

class SlowProcessCollector : public sysmon::monitoring::IProcessCollector
{
public:
    [[nodiscard]] std::optional<std::vector<sysmon::domain::ProcessInfo>> collect() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        return std::vector<sysmon::domain::ProcessInfo>{};
    }
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
