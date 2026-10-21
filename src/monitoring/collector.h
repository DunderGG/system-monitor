#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

#include "domain/connectivity_status.h"
#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "domain/process_info.h"
#include "domain/system_activity_sample.h"

namespace verdandi::monitoring
{

/**
 * Interface for metric collectors.
 *
 * Implementations gather domain samples (synchronously on the scheduler
 * tick or asynchronously on dedicated worker threads) and return typed results.
 *
 * @tparam T The sample type produced by this collector.
 */
template <typename T> class ICollector
{
public:
    virtual ~ICollector() = default;

    /**
     * Collects and returns the latest metric sample.
     */
    [[nodiscard]] virtual T collect() = 0;
};

/**
 * Called by an event-driven collector after it stores a change. It may run on
 * any thread, including OS thread-pool threads, so it must return quickly and
 * must not block or call back into the collector.
 */
using ChangeListener = std::function<void()>;

/**
 * Interface for event-driven collectors (docs/architecture.md, "Event-driven
 * collectors"): OS notifications update the collector's cached data, and the
 * collector calls its change listener so that the scheduler can collect the
 * change at once instead of at its next refresh.
 *
 * @tparam T The sample type produced by this collector.
 */
template <typename T> class IEventDrivenCollector : public ICollector<T>
{
public:
    /**
     * Sets the function to call after a change is stored, replacing the
     * previous one; an empty function stops the calls. Thread-safe, and once
     * it returns the previous listener is no longer called.
     */
    virtual void setChangeListener(ChangeListener listener) = 0;
};

/**
 * Interface for collectors that compute rates, such as CPU usage or
 * throughput, from the change in cumulative counters since their previous
 * collect(), their baselines.
 *
 * @tparam T The sample type produced by this collector.
 */
template <typename T> class IRateCollector : public ICollector<T>
{
public:
    /**
     * Forgets the baselines, so that the next collect() only records new ones
     * and reports no rates. The scheduler calls it after the system resumes
     * from sleep, so that no rate spans the sleep (DC-129). Called on the
     * thread that calls collect(), never during one.
     */
    virtual void resetBaselines() = 0;
};

// Collectors return std::nullopt when no valid sample is available
// (query failure, or no baseline yet for rate-based metrics). For collections,
// std::nullopt means the query failed and an empty vector means it succeeded
// and found nothing.
using ICpuCollector = IRateCollector<std::optional<domain::CpuSample>>;
using IMemoryCollector = ICollector<std::optional<domain::MemorySample>>;
using IDiskCollector = ICollector<std::optional<std::vector<domain::DiskSample>>>;
using INetworkCollector = IRateCollector<std::optional<std::vector<domain::NetworkSample>>>;
using IConnectivityCollector = IEventDrivenCollector<domain::ConnectivityStatus>;
using INetworkProfileCollector = IEventDrivenCollector<std::optional<std::vector<domain::NetworkProfile>>>;
using IUptimeCollector = ICollector<std::chrono::milliseconds>;
using IProcessCollector = IRateCollector<std::optional<std::vector<domain::ProcessInfo>>>;
using ISystemActivityCollector = ICollector<std::optional<domain::SystemActivitySample>>;

} // namespace verdandi::monitoring
