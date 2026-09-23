#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

#include <QObject>

#include "domain/system_snapshot.h"
#include "monitoring/collector.h"

Q_DECLARE_METATYPE(sysmon::domain::SystemSnapshot)

namespace sysmon::monitoring
{

/**
 * Periodically samples registered metrics collectors and emits immutable
 * SystemSnapshot values via Qt signals across thread boundaries.
 *
 * Threading model:
 * - Owns a background std::jthread running the periodic sampling tick.
 * - Collectors are called synchronously on the background thread.
 * - The assembled snapshot is emitted via snapshotReady signal.
 * - Clean shutdown is coordinated using std::stop_token and std::condition_variable_any.
 */
class SamplingScheduler : public QObject
{
    Q_OBJECT

public:
    explicit SamplingScheduler(
        std::chrono::milliseconds interval = std::chrono::milliseconds{1000},
        QObject*                   parent = nullptr);
    ~SamplingScheduler() override;

    SamplingScheduler(const SamplingScheduler&) = delete;
    SamplingScheduler& operator=(const SamplingScheduler&) = delete;
    SamplingScheduler(SamplingScheduler&&) = delete;
    SamplingScheduler& operator=(SamplingScheduler&&) = delete;

    /** Starts the background sampling thread. */
    void start();

    /** Stops the background sampling thread cooperatively and waits for it to exit. */
    void stop();

    /** Returns true if the background thread is currently running. */
    [[nodiscard]] bool isRunning() const;

    /** Sets the sampling interval. Thread-safe. */
    void setInterval(std::chrono::milliseconds interval);

    /** Returns the current sampling interval. */
    [[nodiscard]] std::chrono::milliseconds interval() const;

    /**
     * Collector registration.
     * Invariant: These methods must only be called while the scheduler is stopped
     * (before start() or after stop()). Hot-swapping collectors while the background
     * sampling thread is running is not supported.
     */
    void setCpuCollector(std::unique_ptr<ICpuCollector> collector);
    void setMemoryCollector(std::unique_ptr<IMemoryCollector> collector);
    void setDiskCollector(std::unique_ptr<IDiskCollector> collector);
    void setNetworkCollector(std::unique_ptr<INetworkCollector> collector);
    void setConnectivityCollector(std::unique_ptr<IConnectivityCollector> collector);
    void setProcessCollector(std::unique_ptr<IProcessCollector> collector);

    /**
     * Synchronously collects a sample from all currently registered collectors
     * and returns the assembled SystemSnapshot. Useful for immediate reads and unit testing.
     */
    [[nodiscard]] domain::SystemSnapshot sampleOnce();

signals:
    /** Emitted on each sampling tick with the newly assembled snapshot. */
    void snapshotReady(const sysmon::domain::SystemSnapshot& snapshot);

private:
    void run(std::stop_token stopToken);

    std::chrono::milliseconds m_interval{1000};
    std::atomic<bool>          m_isRunning{false};

    mutable std::mutex         m_sleepMutex;
    std::condition_variable_any m_sleepCv;

    std::jthread               m_thread;

    mutable std::mutex         m_collectorMutex;
    std::unique_ptr<ICpuCollector>          m_cpuCollector;
    std::unique_ptr<IMemoryCollector>       m_memoryCollector;
    std::unique_ptr<IDiskCollector>         m_diskCollector;
    std::unique_ptr<INetworkCollector>      m_networkCollector;
    std::unique_ptr<IConnectivityCollector> m_connectivityCollector;
    std::unique_ptr<IProcessCollector>      m_processCollector;
};

} // namespace sysmon::monitoring

