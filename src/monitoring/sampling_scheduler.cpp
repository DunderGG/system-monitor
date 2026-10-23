#include "monitoring/sampling_scheduler.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <utility>

#include <QMetaType>
#include <spdlog/spdlog.h>

namespace verdandi::monitoring
{

namespace
{

// Waits on condition, with lock held, until interval has passed since start,
// a stop is requested, or isWoken() returns true. interval is the
// scheduler's member, which its setter changes under the lock and then
// notifies, so it is read again whenever the thread wakes: a new interval
// applies to the wait in progress, still measured from start, and ends it at
// once if it has already passed (code review F-46).
template <typename Predicate>
void waitForInterval(std::condition_variable_any& condition, std::unique_lock<std::mutex>& lock,
                     const std::stop_token& stopToken, std::chrono::steady_clock::time_point start,
                     const std::chrono::milliseconds& interval, Predicate isWoken)
{
    while (!stopToken.stop_requested() && !isWoken()) {
        const auto deadline = start + interval;
        if (std::chrono::steady_clock::now() >= deadline) {
            return;
        }
        condition.wait_until(lock, stopToken, deadline, [&] { return isWoken() || start + interval != deadline; });
    }
}

} // namespace

SamplingScheduler::SamplingScheduler(std::chrono::milliseconds interval, QObject* parent)
    : QObject(parent), m_interval(interval)
{
    qRegisterMetaType<verdandi::domain::SystemSnapshot>();
}

SamplingScheduler::~SamplingScheduler()
{
    stop();

    // The listeners call back into this scheduler. Clearing them waits for a
    // call in progress, so none runs once the members start being destroyed.
    std::lock_guard lock(m_collectorMutex);
    if (m_connectivityCollector) {
        m_connectivityCollector->setChangeListener({});
    }
    if (m_networkProfileCollector) {
        m_networkProfileCollector->setChangeListener({});
    }
}

void SamplingScheduler::start()
{
    if (m_isRunning.load()) {
        return;
    }

    m_isRunning = true;
    m_slowThread = std::jthread([this](std::stop_token stopToken) { runSlowCollectors(stopToken); });
    m_thread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void SamplingScheduler::stop()
{
    if (!m_isRunning.load()) {
        return;
    }

    m_thread.request_stop();
    m_slowThread.request_stop();
    {
        std::lock_guard lock(m_sleepMutex);
        m_sleepCv.notify_all();
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
    if (m_slowThread.joinable()) {
        m_slowThread.join();
    }

    m_isRunning = false;
}

bool SamplingScheduler::isRunning() const
{
    return m_isRunning.load();
}

void SamplingScheduler::setInterval(std::chrono::milliseconds interval)
{
    std::lock_guard lock(m_sleepMutex);
    m_interval = interval;
    m_sleepCv.notify_all();
}

std::chrono::milliseconds SamplingScheduler::interval() const
{
    std::lock_guard lock(m_sleepMutex);
    return m_interval;
}

void SamplingScheduler::setSlowInterval(std::chrono::milliseconds interval)
{
    std::lock_guard lock(m_sleepMutex);
    m_slowInterval = interval;
    m_sleepCv.notify_all();
}

std::chrono::milliseconds SamplingScheduler::slowInterval() const
{
    std::lock_guard lock(m_sleepMutex);
    return m_slowInterval;
}

void SamplingScheduler::onSystemSuspending()
{
    {
        // The threads see it before their next collection; neither needs waking.
        std::lock_guard lock(m_sleepMutex);
        m_isSystemSuspended = true;
    }
    spdlog::info("System is suspending: sampling paused until it resumes");
}

void SamplingScheduler::onSystemResumed()
{
    {
        std::lock_guard lock(m_sleepMutex);
        m_isSystemSuspended = false;
        m_isFastBaselineResetPending = true;
        m_isSlowBaselineResetPending = true;
        m_sleepCv.notify_all();
    }
    spdlog::info("System resumed: sampling resumes with new rate baselines");
}

bool SamplingScheduler::isSystemSuspended() const
{
    std::lock_guard lock(m_sleepMutex);
    return m_isSystemSuspended;
}

void SamplingScheduler::setCpuCollector(std::unique_ptr<ICpuCollector> collector)
{
    assert(!m_isRunning.load() && "setCpuCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_cpuCollector = std::move(collector);
}

void SamplingScheduler::setMemoryCollector(std::unique_ptr<IMemoryCollector> collector)
{
    assert(!m_isRunning.load() && "setMemoryCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_memoryCollector = std::move(collector);
}

void SamplingScheduler::setDiskCollector(std::unique_ptr<IDiskCollector> collector)
{
    assert(!m_isRunning.load() && "setDiskCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_diskCollector = std::move(collector);
}

void SamplingScheduler::setNetworkCollector(std::unique_ptr<INetworkCollector> collector)
{
    assert(!m_isRunning.load() && "setNetworkCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_networkCollector = std::move(collector);
}

void SamplingScheduler::setConnectivityCollector(std::unique_ptr<IConnectivityCollector> collector)
{
    assert(!m_isRunning.load() && "setConnectivityCollector must only be called when scheduler is stopped");
    if (collector) {
        collector->setChangeListener([this] { requestEventRefresh(); });
    }
    std::lock_guard lock(m_collectorMutex);
    m_connectivityCollector = std::move(collector);
}

void SamplingScheduler::setNetworkProfileCollector(std::unique_ptr<INetworkProfileCollector> collector)
{
    assert(!m_isRunning.load() && "setNetworkProfileCollector must only be called when scheduler is stopped");
    if (collector) {
        collector->setChangeListener([this] { requestEventRefresh(); });
    }
    std::lock_guard lock(m_collectorMutex);
    m_networkProfileCollector = std::move(collector);
}

void SamplingScheduler::setUptimeCollector(std::unique_ptr<IUptimeCollector> collector)
{
    assert(!m_isRunning.load() && "setUptimeCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_uptimeCollector = std::move(collector);
}

void SamplingScheduler::setSystemActivityCollector(std::unique_ptr<ISystemActivityCollector> collector)
{
    assert(!m_isRunning.load() && "setSystemActivityCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_systemActivityCollector = std::move(collector);
}

void SamplingScheduler::setHealthThresholds(const HealthThresholds& thresholds)
{
    assert(!m_isRunning.load() && "setHealthThresholds must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_healthThresholds = thresholds;
}

void SamplingScheduler::setProcessCollector(std::unique_ptr<IProcessCollector> collector)
{
    assert(!m_isRunning.load() && "setProcessCollector must only be called when scheduler is stopped");
    std::lock_guard lock(m_collectorMutex);
    m_processCollector = std::move(collector);
}

domain::SystemSnapshot SamplingScheduler::sampleOnce()
{
    domain::SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();

    ICpuCollector* cpu = nullptr;
    IMemoryCollector* memory = nullptr;
    IDiskCollector* disk = nullptr;
    INetworkCollector* network = nullptr;
    IUptimeCollector* uptime = nullptr;
    ISystemActivityCollector* systemActivity = nullptr;
    HealthThresholds healthThresholds;

    {
        std::lock_guard lock(m_collectorMutex);
        cpu = m_cpuCollector.get();
        memory = m_memoryCollector.get();
        disk = m_diskCollector.get();
        network = m_networkCollector.get();
        uptime = m_uptimeCollector.get();
        systemActivity = m_systemActivityCollector.get();
        healthThresholds = m_healthThresholds;
    }

    if (cpu) {
        snapshot.cpu = cpu->collect();
    }
    if (memory) {
        snapshot.memory = memory->collect();
    }
    if (disk) {
        snapshot.disks = disk->collect();
    }
    if (network) {
        snapshot.networks = network->collect();
    }
    if (uptime) {
        snapshot.uptime = uptime->collect();
    }
    if (systemActivity) {
        snapshot.activity = systemActivity->collect();
    }

    {
        // Copied under the lock; the slow-collector thread only moves a finished
        // result in, so it never waits long.
        std::lock_guard lock(m_slowResultsMutex);
        snapshot.connectivity = m_slowResults.connectivity;
        snapshot.networkProfiles = m_slowResults.networkProfiles;
        snapshot.processes = m_slowResults.processes;
    }

    snapshot.health = evaluateHealth(snapshot, healthThresholds);

    return snapshot;
}

void SamplingScheduler::refreshSlowCollectors()
{
    assert(!m_isRunning.load() && "refreshSlowCollectors must only be called when scheduler is stopped");
    collectSlowResults();
}

void SamplingScheduler::refreshEventDrivenCollectors()
{
    assert(!m_isRunning.load() && "refreshEventDrivenCollectors must only be called when scheduler is stopped");
    collectEventDrivenResults();
}

void SamplingScheduler::requestEventRefresh()
{
    std::lock_guard lock(m_sleepMutex);
    m_isEventRefreshPending = true;
    m_sleepCv.notify_all();
}

void SamplingScheduler::collectEventDrivenResults()
{
    IConnectivityCollector* connectivity = nullptr;
    INetworkProfileCollector* networkProfile = nullptr;
    {
        std::lock_guard lock(m_collectorMutex);
        connectivity = m_connectivityCollector.get();
        networkProfile = m_networkProfileCollector.get();
    }

    domain::ConnectivityStatus connectivityStatus;
    std::optional<std::vector<domain::NetworkProfile>> networkProfiles;
    if (connectivity) {
        connectivityStatus = connectivity->collect();
    }
    if (networkProfile) {
        networkProfiles = networkProfile->collect();
    }

    // The process list stays as the last full refresh left it.
    std::lock_guard lock(m_slowResultsMutex);
    m_slowResults.connectivity = connectivityStatus;
    m_slowResults.networkProfiles = std::move(networkProfiles);
}

void SamplingScheduler::collectSlowResults()
{
    IConnectivityCollector* connectivity = nullptr;
    INetworkProfileCollector* networkProfile = nullptr;
    IProcessCollector* process = nullptr;
    {
        std::lock_guard lock(m_collectorMutex);
        connectivity = m_connectivityCollector.get();
        networkProfile = m_networkProfileCollector.get();
        process = m_processCollector.get();
    }

    // Collected outside both locks: process enumeration takes milliseconds.
    SlowResults results;
    if (connectivity) {
        results.connectivity = connectivity->collect();
    }
    if (networkProfile) {
        results.networkProfiles = networkProfile->collect();
    }
    if (process) {
        results.processes = process->collect();
    }

    std::lock_guard lock(m_slowResultsMutex);
    m_slowResults = std::move(results);
}

void SamplingScheduler::resetFastCollectorBaselines()
{
    ICpuCollector* cpu = nullptr;
    INetworkCollector* network = nullptr;
    {
        std::lock_guard lock(m_collectorMutex);
        cpu = m_cpuCollector.get();
        network = m_networkCollector.get();
    }

    if (cpu) {
        cpu->resetBaselines();
    }
    if (network) {
        network->resetBaselines();
    }
}

void SamplingScheduler::resetSlowCollectorBaselines()
{
    IProcessCollector* process = nullptr;
    {
        std::lock_guard lock(m_collectorMutex);
        process = m_processCollector.get();
    }

    if (process) {
        process->resetBaselines();
    }
}

void SamplingScheduler::run(std::stop_token stopToken)
{
    // Logged after the lock is released: a log line writes a file, and change
    // listeners take the sleep mutex on OS notification threads.
    std::chrono::milliseconds interval{};
    {
        std::lock_guard lock(m_sleepMutex);
        interval = m_interval;
    }
    spdlog::info("SamplingScheduler background thread started (interval: {}ms)", interval.count());

    while (!stopToken.stop_requested()) {
        bool isFirstTickAfterResume = false;
        {
            // Nothing is collected while the system suspends, so that no
            // sample taken before the sleep is the baseline of one after it.
            std::unique_lock lock(m_sleepMutex);
            m_sleepCv.wait(lock, stopToken, [this] { return !m_isSystemSuspended; });
            if (stopToken.stop_requested()) {
                break;
            }
            isFirstTickAfterResume = std::exchange(m_isFastBaselineResetPending, false);
        }

        const auto tickStart = std::chrono::steady_clock::now();
        if (isFirstTickAfterResume) {
            resetFastCollectorBaselines();
        }

        const domain::SystemSnapshot snapshot = sampleOnce();
        // After a resume, the rate collectors' first sample only records their
        // new baselines, so the snapshot is discarded rather than shown without rates.
        if (!isFirstTickAfterResume) {
            emit snapshotReady(snapshot);
        }

        std::unique_lock lock(m_sleepMutex);
        waitForInterval(m_sleepCv, lock, stopToken, tickStart, m_interval, [] { return false; });
    }

    spdlog::info("SamplingScheduler background thread stopped");
}

void SamplingScheduler::runSlowCollectors(std::stop_token stopToken)
{
    // Logged after the lock is released, as in run().
    std::chrono::milliseconds slowInterval{};
    {
        std::lock_guard lock(m_sleepMutex);
        slowInterval = m_slowInterval;
    }
    spdlog::info("SamplingScheduler slow-collector thread started (interval: {}ms)", slowInterval.count());

    // Waits for the next full refresh, or for an event-driven collector to
    // report a change, which collects only the event-driven collectors so the
    // process list keeps its own cadence. The first wait ends at once, since
    // no refresh has started, and so does the first after the system
    // resumes, which starts a full refresh.
    auto refreshStart = std::chrono::steady_clock::time_point::min();
    while (!stopToken.stop_requested()) {
        bool isEventRefresh = false;
        bool isFirstRefreshAfterResume = false;
        {
            std::unique_lock lock(m_sleepMutex);
            waitForInterval(m_sleepCv, lock, stopToken, refreshStart, m_slowInterval,
                            [this] { return m_isEventRefreshPending || m_isSlowBaselineResetPending; });
            // Nothing is collected while the system suspends, as on the tick.
            m_sleepCv.wait(lock, stopToken, [this] { return !m_isSystemSuspended; });
            if (stopToken.stop_requested()) {
                break;
            }
            isFirstRefreshAfterResume = std::exchange(m_isSlowBaselineResetPending, false);
            // A full refresh collects the event-driven collectors too, so it
            // also answers a change that is pending when it starts.
            isEventRefresh = !isFirstRefreshAfterResume && m_isEventRefreshPending &&
                             std::chrono::steady_clock::now() < refreshStart + m_slowInterval;
            m_isEventRefreshPending = false;
        }

        if (isEventRefresh) {
            collectEventDrivenResults();
            continue;
        }

        // Unlike the tick's first sample after a resume, this refresh is kept:
        // the list is current, with no rates until the next refresh.
        if (isFirstRefreshAfterResume) {
            resetSlowCollectorBaselines();
        }
        refreshStart = std::chrono::steady_clock::now();
        collectSlowResults();
    }

    spdlog::info("SamplingScheduler slow-collector thread stopped");
}

} // namespace verdandi::monitoring
