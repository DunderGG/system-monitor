#pragma once

#include <concepts>
#include <vector>

#include "domain/connectivity_status.h"
#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "domain/process_info.h"

namespace sysmon::monitoring
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
 * Concept constraining types that can act as a typed sample collector.
 */
template <typename C, typename T>
concept Collector = requires(C collector) {
    { collector.collect() } -> std::convertible_to<T>;
};

using ICpuCollector = ICollector<domain::CpuSample>;
using IMemoryCollector = ICollector<domain::MemorySample>;
using IDiskCollector = ICollector<std::vector<domain::DiskSample>>;
using INetworkCollector = ICollector<std::vector<domain::NetworkSample>>;
using IConnectivityCollector = ICollector<domain::ConnectivityStatus>;
using IProcessCollector = ICollector<std::vector<domain::ProcessInfo>>;

} // namespace sysmon::monitoring
