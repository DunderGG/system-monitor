#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <QObject>
#include <thread>

#include <gtest/gtest.h>

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

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady, [&](const SystemSnapshot &snapshot) {
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
    explicit MutexReentryProbeCollector(SamplingScheduler &scheduler) : m_scheduler(scheduler) {}

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
    SamplingScheduler &m_scheduler;
    bool m_called{false};
};

} // namespace

TEST(SamplingScheduler, SampleOnce_ReleasesCollectorMutexBeforeCollection)
{
    SamplingScheduler scheduler;
    auto probe = std::make_unique<MutexReentryProbeCollector>(scheduler);
    const auto *probePtr = probe.get();
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
    explicit FakeUptimeCollector(std::chrono::milliseconds uptime)
        : m_uptime(uptime)
    {
    }

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
    explicit FixedCpuCollector(float usagePercent)
        : m_usagePercent(usagePercent)
    {
    }

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
