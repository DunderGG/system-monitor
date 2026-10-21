#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QObject>

#include "domain/cpu_sample.h"
#include "domain/network_sample.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"

// SamplingScheduler: sampling paused while the system suspends, and the rate
// collectors' baselines reset when it resumes (DC-129).

using namespace std::chrono_literals;
using sysmon::domain::SystemSnapshot;
using sysmon::monitoring::SamplingScheduler;

namespace
{

// The calls the scheduler made to one rate collector, in order: "collect" or
// "reset". It outlives the collector, which the scheduler owns.
class CallLog
{
public:
    // Records a call and returns how many calls of its kind there have been.
    int record(const std::string& call)
    {
        const std::lock_guard lock(m_mutex);
        m_calls.push_back(call);
        return static_cast<int>(std::ranges::count(m_calls, call));
    }

    [[nodiscard]] std::vector<std::string> calls() const
    {
        const std::lock_guard lock(m_mutex);
        return m_calls;
    }

    [[nodiscard]] int count(const std::string& call) const
    {
        const std::lock_guard lock(m_mutex);
        return static_cast<int>(std::ranges::count(m_calls, call));
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_calls;
};

// Reports the number of its collect() call as its CPU usage, so a test can
// tell which collection a snapshot carries.
class RecordingCpuCollector : public sysmon::monitoring::ICpuCollector
{
public:
    explicit RecordingCpuCollector(CallLog& log) : m_log(log) {}

    [[nodiscard]] std::optional<sysmon::domain::CpuSample> collect() override
    {
        const int call = m_log.record("collect");
        return sysmon::domain::CpuSample{.totalUsagePercent = static_cast<float>(call), .coreCount = 1};
    }

    void resetBaselines() override
    {
        m_log.record("reset");
    }

private:
    CallLog& m_log;
};

class RecordingNetworkCollector : public sysmon::monitoring::INetworkCollector
{
public:
    explicit RecordingNetworkCollector(CallLog& log) : m_log(log) {}

    [[nodiscard]] std::optional<std::vector<sysmon::domain::NetworkSample>> collect() override
    {
        m_log.record("collect");
        return std::vector<sysmon::domain::NetworkSample>{};
    }

    void resetBaselines() override
    {
        m_log.record("reset");
    }

private:
    CallLog& m_log;
};

class RecordingProcessCollector : public sysmon::monitoring::IProcessCollector
{
public:
    explicit RecordingProcessCollector(CallLog& log) : m_log(log) {}

    [[nodiscard]] std::optional<std::vector<sysmon::domain::ProcessInfo>> collect() override
    {
        m_log.record("collect");
        return std::vector<sysmon::domain::ProcessInfo>{};
    }

    void resetBaselines() override
    {
        m_log.record("reset");
    }

private:
    CallLog& m_log;
};

// A scheduler that ticks every 10 ms, with recording rate collectors, and the
// CPU usage of each snapshot it emitted.
class RecordingScheduler
{
public:
    explicit RecordingScheduler(std::chrono::milliseconds slowInterval) : m_scheduler(10ms)
    {
        m_scheduler.setSlowInterval(slowInterval);
        m_scheduler.setCpuCollector(std::make_unique<RecordingCpuCollector>(m_cpu));
        m_scheduler.setNetworkCollector(std::make_unique<RecordingNetworkCollector>(m_network));
        m_scheduler.setProcessCollector(std::make_unique<RecordingProcessCollector>(m_process));
        // Without a context object the lambda runs on the tick thread.
        QObject::connect(&m_scheduler, &SamplingScheduler::snapshotReady, [this](const SystemSnapshot& snapshot) {
            const std::lock_guard lock(m_mutex);
            m_emittedCpuUsages.push_back(snapshot.cpu ? std::optional<float>{snapshot.cpu->totalUsagePercent}
                                                      : std::nullopt);
        });
    }

    SamplingScheduler& scheduler()
    {
        return m_scheduler;
    }

    const CallLog& cpu() const
    {
        return m_cpu;
    }

    const CallLog& network() const
    {
        return m_network;
    }

    const CallLog& process() const
    {
        return m_process;
    }

    [[nodiscard]] std::vector<std::optional<float>> emittedCpuUsages() const
    {
        const std::lock_guard lock(m_mutex);
        return m_emittedCpuUsages;
    }

    [[nodiscard]] int snapshotCount() const
    {
        const std::lock_guard lock(m_mutex);
        return static_cast<int>(m_emittedCpuUsages.size());
    }

private:
    // Declared before the scheduler, which owns the collectors that record into them.
    CallLog m_cpu;
    CallLog m_network;
    CallLog m_process;
    mutable std::mutex m_mutex;
    std::vector<std::optional<float>> m_emittedCpuUsages;
    SamplingScheduler m_scheduler;
};

// Polls until isDone returns true, or 5 s pass on a busy machine; returns its last result.
bool waitUntil(const std::function<bool()>& isDone)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!isDone() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }
    return isDone();
}

std::vector<std::string> firstCalls(const CallLog& log, std::size_t count)
{
    auto calls = log.calls();
    calls.resize(std::min(calls.size(), count));
    return calls;
}

} // namespace

TEST(SamplingScheduler, Running_SystemSuspending_NothingCollectedUntilResumed)
{
    RecordingScheduler recording(10ms);
    SamplingScheduler& scheduler = recording.scheduler();
    scheduler.start();
    ASSERT_TRUE(waitUntil([&] { return recording.snapshotCount() >= 2 && recording.process().count("collect") >= 2; }));

    scheduler.onSystemSuspending();
    const int cpuCollectsAtSuspend = recording.cpu().count("collect");
    const int processCollectsAtSuspend = recording.process().count("collect");
    const int snapshotsAtSuspend = recording.snapshotCount();
    const bool isSuspended = scheduler.isSystemSuspended();
    // Twenty ticks' and slow refreshes' time.
    std::this_thread::sleep_for(200ms);
    const int cpuCollectsWhileSuspended = recording.cpu().count("collect") - cpuCollectsAtSuspend;
    const int processCollectsWhileSuspended = recording.process().count("collect") - processCollectsAtSuspend;
    const int snapshotsWhileSuspended = recording.snapshotCount() - snapshotsAtSuspend;

    scheduler.onSystemResumed();
    const bool isEmittingAgain =
        waitUntil([&] { return recording.snapshotCount() > snapshotsAtSuspend + snapshotsWhileSuspended; });
    scheduler.stop();

    EXPECT_TRUE(isSuspended);
    EXPECT_FALSE(scheduler.isSystemSuspended());
    // At most one collection that was already under way on each thread.
    EXPECT_LE(cpuCollectsWhileSuspended, 1);
    EXPECT_LE(processCollectsWhileSuspended, 1);
    EXPECT_LE(snapshotsWhileSuspended, 1);
    EXPECT_TRUE(isEmittingAgain);
}

TEST(SamplingScheduler, Running_SystemResumed_BaselinesResetBeforeCollectingAndFirstTickDiscarded)
{
    RecordingScheduler recording(60s);
    SamplingScheduler& scheduler = recording.scheduler();
    scheduler.onSystemSuspending();
    scheduler.start();

    scheduler.onSystemResumed();
    // The slow-collector thread refreshes at once, not a slow interval later.
    const bool hasSampled =
        waitUntil([&] { return recording.snapshotCount() >= 1 && recording.process().count("collect") >= 1; });
    scheduler.stop();

    ASSERT_TRUE(hasSampled);
    const std::vector<std::string> resetThenCollect{"reset", "collect"};
    EXPECT_EQ(firstCalls(recording.cpu(), 2), resetThenCollect);
    EXPECT_EQ(firstCalls(recording.network(), 2), resetThenCollect);
    EXPECT_EQ(firstCalls(recording.process(), 2), resetThenCollect);
    // The first snapshot carries the second collection.
    EXPECT_EQ(recording.emittedCpuUsages().front(), 2.0f);
    EXPECT_EQ(recording.cpu().count("collect"), recording.snapshotCount() + 1);
}

TEST(SamplingScheduler, Running_SystemResumedWithoutSuspending_BaselinesResetToo)
{
    RecordingScheduler recording(60s);
    SamplingScheduler& scheduler = recording.scheduler();
    scheduler.start();
    ASSERT_TRUE(waitUntil([&] { return recording.snapshotCount() >= 2 && recording.process().count("collect") >= 1; }));

    const int snapshotsAtResume = recording.snapshotCount();
    scheduler.onSystemResumed();
    // The first snapshot after the resume may come from a tick already under
    // way; the second comes after the tick that resets and discards.
    const bool hasSampled = waitUntil([&] {
        return recording.snapshotCount() >= snapshotsAtResume + 2 && recording.process().count("collect") >= 2;
    });
    scheduler.stop();

    ASSERT_TRUE(hasSampled);
    EXPECT_EQ(recording.cpu().count("reset"), 1);
    EXPECT_EQ(recording.network().count("reset"), 1);
    EXPECT_EQ(recording.process().count("reset"), 1);
    EXPECT_EQ(recording.process().calls(), (std::vector<std::string>{"collect", "reset", "collect"}));
    EXPECT_EQ(recording.cpu().count("collect"), recording.snapshotCount() + 1);
}

TEST(SamplingScheduler, Stop_WhileSystemSuspended_RespondsImmediately)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{5000});
    scheduler.setSlowInterval(60s);
    scheduler.onSystemSuspending();
    scheduler.start();
    std::this_thread::sleep_for(30ms);

    const auto stopStart = std::chrono::steady_clock::now();
    scheduler.stop();
    const auto stopDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stopStart);

    EXPECT_FALSE(scheduler.isRunning());
    EXPECT_LT(stopDuration.count(), 1000);
}
