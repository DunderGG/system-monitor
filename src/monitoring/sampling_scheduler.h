#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include <QObject>

#include "domain/system_snapshot.h"
#include "monitoring/collector.h"
#include "monitoring/health_evaluator.h"

Q_DECLARE_METATYPE(sysmon::domain::SystemSnapshot)

namespace sysmon::monitoring
{

/**
 * Periodically samples registered metrics collectors and emits immutable
 * SystemSnapshot values via Qt signals across thread boundaries.
 *
 * Threading model:
 * - The tick thread (a std::jthread) runs the fast collectors (CPU, memory,
 *   disk, network, uptime, system activity) every interval(), assembles the
 *   snapshot, and emits it via the snapshotReady signal.
 * - The slow-collector thread (a second std::jthread) runs the slow collectors
 *   (processes, connectivity, and network profiles) every slowInterval() and
 *   stores their results under a mutex. Each tick merges the latest stored
 *   results, so a snapshot's processes can be up to slowInterval() old. All
 *   three are empty (std::nullopt, ConnectivityLevel::Unknown) until the first
 *   slow refresh.
 * - Connectivity and network profiles are event-driven collectors. When one
 *   reports a change through its change listener, the slow-collector thread
 *   wakes and collects those two again, without waiting for its next refresh
 *   and without running the process collector. The change reaches the next
 *   tick's snapshot; no extra snapshot is emitted.
 * - While the system suspends, neither thread collects (DC-129). After it
 *   resumes, each thread resets the baselines of its rate collectors before
 *   it collects again, so no rate spans the sleep. The tick discards its first
 *   snapshot, which only records the new baselines.
 * - Clean shutdown is coordinated using std::stop_token and std::condition_variable_any.
 */
class SamplingScheduler : public QObject
{
    Q_OBJECT

public:
    static constexpr std::chrono::milliseconds kDefaultSlowInterval{2000};

    explicit SamplingScheduler(std::chrono::milliseconds interval = std::chrono::milliseconds{1000},
                               QObject* parent = nullptr);
    ~SamplingScheduler() override;

    SamplingScheduler(const SamplingScheduler&) = delete;
    SamplingScheduler& operator=(const SamplingScheduler&) = delete;
    SamplingScheduler(SamplingScheduler&&) = delete;
    SamplingScheduler& operator=(SamplingScheduler&&) = delete;

    /** Starts the tick thread and the slow-collector thread. */
    void start();

    /** Stops both background threads cooperatively and waits for them to exit. */
    void stop();

    /** Returns true if the background threads are currently running. */
    [[nodiscard]] bool isRunning() const;

    /** Sets the sampling interval of the fast collectors. Thread-safe. */
    void setInterval(std::chrono::milliseconds interval);

    /** Returns the current sampling interval of the fast collectors. */
    [[nodiscard]] std::chrono::milliseconds interval() const;

    /** Sets the interval of the slow collectors (processes, connectivity, network profiles). Thread-safe. */
    void setSlowInterval(std::chrono::milliseconds interval);

    /** Returns the current interval of the slow collectors. */
    [[nodiscard]] std::chrono::milliseconds slowInterval() const;

    /**
     * Pauses both threads before their next collection, because the system
     * is about to sleep or hibernate, until onSystemResumed(). Thread-safe and
     * non-blocking; may be called whether or not the scheduler is running.
     */
    void onSystemSuspending();

    /**
     * Ends the pause of onSystemSuspending(), if any. Each thread then resets
     * the baselines of its rate collectors before it collects again: the tick
     * at its next tick, whose snapshot it discards, and the slow-collector
     * thread at once, with a full refresh. A resume without a suspend resets
     * them too. Thread-safe and non-blocking.
     */
    void onSystemResumed();

    /** Returns true between onSystemSuspending() and onSystemResumed(). */
    [[nodiscard]] bool isSystemSuspended() const;

    /**
     * Collector registration.
     * Invariant: These methods must only be called while the scheduler is stopped
     * (before start() or after stop()). Hot-swapping collectors while the background
     * sampling thread is running is not supported. The scheduler sets the change
     * listener of each event-driven collector, and clears it on destruction.
     */
    void setCpuCollector(std::unique_ptr<ICpuCollector> collector);
    void setMemoryCollector(std::unique_ptr<IMemoryCollector> collector);
    void setDiskCollector(std::unique_ptr<IDiskCollector> collector);
    void setNetworkCollector(std::unique_ptr<INetworkCollector> collector);
    void setConnectivityCollector(std::unique_ptr<IConnectivityCollector> collector);
    void setNetworkProfileCollector(std::unique_ptr<INetworkProfileCollector> collector);
    void setUptimeCollector(std::unique_ptr<IUptimeCollector> collector);
    void setSystemActivityCollector(std::unique_ptr<ISystemActivityCollector> collector);
    void setProcessCollector(std::unique_ptr<IProcessCollector> collector);

    /**
     * Sets the thresholds used to evaluate SystemSnapshot::health on each tick.
     * Same invariant as collector registration: only call while stopped.
     */
    void setHealthThresholds(const HealthThresholds& thresholds);

    /**
     * Synchronously collects a sample from the registered fast collectors,
     * merges the latest slow-collector results, and returns the assembled
     * SystemSnapshot. Runs on the tick thread; call it directly (for an
     * immediate read or a unit test) only while the scheduler is stopped.
     */
    [[nodiscard]] domain::SystemSnapshot sampleOnce();

    /**
     * Synchronously runs the registered slow collectors and stores their
     * results for the next sampleOnce(). The slow-collector thread does this
     * every slowInterval(); call it directly only while the scheduler is
     * stopped, e.g. in unit tests.
     */
    void refreshSlowCollectors();

    /**
     * Synchronously runs only the event-driven collectors (connectivity and
     * network profiles) and stores their results, keeping the process list.
     * The slow-collector thread does this when one of them reports a change;
     * call it directly only while the scheduler is stopped.
     */
    void refreshEventDrivenCollectors();

signals:
    /** Emitted on each sampling tick with the newly assembled snapshot. */
    void snapshotReady(const sysmon::domain::SystemSnapshot& snapshot);

private:
    // The latest results of the slow collectors. Defaults mean "no data yet".
    struct SlowResults
    {
        domain::ConnectivityStatus connectivity;
        std::optional<std::vector<domain::NetworkProfile>> networkProfiles;
        std::optional<std::vector<domain::ProcessInfo>> processes;
    };

    void run(std::stop_token stopToken);
    void runSlowCollectors(std::stop_token stopToken);
    void collectSlowResults();
    void collectEventDrivenResults();
    void resetFastCollectorBaselines();
    void resetSlowCollectorBaselines();

    // The change listener of the event-driven collectors: marks an event
    // refresh as due and wakes the slow-collector thread. Thread-safe and
    // non-blocking; called from OS notification threads.
    void requestEventRefresh();

    std::chrono::milliseconds m_interval{1000};
    std::chrono::milliseconds m_slowInterval{kDefaultSlowInterval};
    std::atomic<bool> m_isRunning{false};

    mutable std::mutex m_sleepMutex;
    std::condition_variable_any m_sleepCv;
    // Set by requestEventRefresh(), cleared by the slow-collector thread. Guarded by m_sleepMutex.
    bool m_isEventRefreshPending{false};
    // Set by onSystemSuspending(), cleared by onSystemResumed(). Guarded by m_sleepMutex.
    bool m_isSystemSuspended{false};
    // Set by onSystemResumed(), each cleared by its thread when it resets its
    // rate collectors' baselines. Guarded by m_sleepMutex.
    bool m_isFastBaselineResetPending{false};
    bool m_isSlowBaselineResetPending{false};

    std::jthread m_thread;
    std::jthread m_slowThread;

    // Written by the slow-collector thread, read by the tick thread.
    std::mutex m_slowResultsMutex;
    SlowResults m_slowResults;

    mutable std::mutex m_collectorMutex;
    std::unique_ptr<ICpuCollector> m_cpuCollector;
    std::unique_ptr<IMemoryCollector> m_memoryCollector;
    std::unique_ptr<IDiskCollector> m_diskCollector;
    std::unique_ptr<INetworkCollector> m_networkCollector;
    std::unique_ptr<IConnectivityCollector> m_connectivityCollector;
    std::unique_ptr<INetworkProfileCollector> m_networkProfileCollector;
    std::unique_ptr<IUptimeCollector> m_uptimeCollector;
    std::unique_ptr<ISystemActivityCollector> m_systemActivityCollector;
    HealthThresholds m_healthThresholds;
    std::unique_ptr<IProcessCollector> m_processCollector;
};

} // namespace sysmon::monitoring
