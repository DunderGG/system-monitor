#include "monitoring/sampling_scheduler.h"

#include <cassert>
#include <chrono>
#include <mutex>
#include <QMetaType>
#include <utility>

#include <spdlog/spdlog.h>

namespace sysmon::monitoring
{

SamplingScheduler::SamplingScheduler(std::chrono::milliseconds interval, QObject *parent)
    : QObject(parent), m_interval(interval)
{
    qRegisterMetaType<sysmon::domain::SystemSnapshot>();
}

SamplingScheduler::~SamplingScheduler()
{
    stop();
}

void SamplingScheduler::start()
{
    if (m_isRunning.load()) {
        return;
    }

    m_isRunning = true;
    m_thread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
}

void SamplingScheduler::stop()
{
    if (!m_isRunning.load()) {
        return;
    }

    m_thread.request_stop();
    {
        std::lock_guard lock(m_sleepMutex);
        m_sleepCv.notify_all();
    }

    if (m_thread.joinable()) {
        m_thread.join();
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
    std::lock_guard lock(m_collectorMutex);
    m_connectivityCollector = std::move(collector);
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

    ICpuCollector *cpu = nullptr;
    IMemoryCollector *memory = nullptr;
    IDiskCollector *disk = nullptr;
    INetworkCollector *network = nullptr;
    IConnectivityCollector *connectivity = nullptr;
    IProcessCollector *process = nullptr;

    {
        std::lock_guard lock(m_collectorMutex);
        cpu = m_cpuCollector.get();
        memory = m_memoryCollector.get();
        disk = m_diskCollector.get();
        network = m_networkCollector.get();
        connectivity = m_connectivityCollector.get();
        process = m_processCollector.get();
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
    if (connectivity) {
        snapshot.connectivity = connectivity->collect();
    }
    if (process) {
        snapshot.processes = process->collect();
    }

    return snapshot;
}

void SamplingScheduler::run(std::stop_token stopToken)
{
    {
        std::lock_guard lock(m_sleepMutex);
        spdlog::info("SamplingScheduler background thread started (interval: {}ms)", m_interval.count());
    }

    while (!stopToken.stop_requested()) {
        const auto tickStart = std::chrono::steady_clock::now();

        const domain::SystemSnapshot snapshot = sampleOnce();
        emit snapshotReady(snapshot);

        std::unique_lock lock(m_sleepMutex);
        const auto nextTick = tickStart + m_interval;
        m_sleepCv.wait_until(lock, stopToken, nextTick, [&stopToken] { return stopToken.stop_requested(); });
    }

    spdlog::info("SamplingScheduler background thread stopped");
}

} // namespace sysmon::monitoring
