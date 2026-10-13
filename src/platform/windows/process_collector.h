#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "monitoring/collector.h"
#include "platform/windows/process_identity_cache.h"
#include "platform/windows/steady_clock_reader.h"

namespace sysmon::platform
{

/**
 * Reads the running processes with the data the process list itself provides
 * (NtQuerySystemInformation in production). Returns std::nullopt on failure.
 */
using ProcessListReader = std::function<std::optional<std::vector<domain::ProcessInfo>>()>;

/** A process's total CPU time (user + kernel) when it was last seen. */
struct ProcessCpuBaseline
{
    uint64_t cpuTimeMs{0};
    std::chrono::steady_clock::time_point timestamp;
};

using ProcessCpuBaselines =
    std::unordered_map<domain::ProcessIdentity, ProcessCpuBaseline, domain::ProcessIdentityHash>;

/**
 * Sets cpuUsagePercent of every process from the change in its user + kernel
 * time since its baseline, over the monotonic time elapsed, as a share of all
 * logical processors: 100% means every processor was busy with the process.
 * Values are clamped to 0-100, since CPU time is accounted at clock-tick
 * granularity.
 *
 * cpuUsagePercent is std::nullopt for a process without a baseline (first
 * seen, or a reused PID with a new creation time), when no time has elapsed,
 * when the process's CPU time went backwards, or when logicalProcessorCount is
 * not positive. The baselines are then replaced by this list's, so exited
 * processes are dropped. Pure function for deterministic unit testing.
 */
void calculateProcessCpuUsage(std::vector<domain::ProcessInfo>& processes, ProcessCpuBaselines& baselines,
                              std::chrono::steady_clock::time_point now, int logicalProcessorCount);

/**
 * Collector for the running processes.
 *
 * One NtQuerySystemInformation(SystemProcessInformation) call per collect()
 * returns the PID, parent PID, creation time, image name, thread and handle
 * counts, working set, private bytes, I/O byte counters, and CPU times of
 * every process, including protected and elevated ones, without opening any
 * of them. collect() returns std::nullopt when the list cannot be read.
 *
 * The image path and command line come from a ProcessDetailsReader, called
 * once per process identity through a ProcessIdentityCache. Without a details
 * reader they stay std::nullopt with ProcessDetailsStatus::NotQueried. The
 * reader is never called for lsass.exe or csrss.exe (isNeverOpenedProcess()),
 * whose details stay NotQueried.
 *
 * CPU usage is computed against the previous successful collect() with
 * calculateProcessCpuUsage(), so the first collect() reports none. A failed
 * read keeps the baselines, and the next success measures over the longer
 * interval.
 *
 * Not thread-safe; collect() must be called from one thread at a time.
 */
class ProcessCollector : public monitoring::IProcessCollector
{
public:
    /**
     * Reads the process list with NtQuerySystemInformation and each new
     * process's details with makeWindowsProcessDetailsReader(). CPU usage is
     * shared across all active logical processors in every processor group.
     */
    ProcessCollector();

    /**
     * Uses injected readers, for unit testing. A null clock reader uses
     * std::chrono::steady_clock.
     */
    explicit ProcessCollector(ProcessListReader listReader, ProcessDetailsReader detailsReader = nullptr,
                              SteadyClockReader clockReader = nullptr, int logicalProcessorCount = 1);

    ~ProcessCollector() override = default;

    [[nodiscard]] std::optional<std::vector<domain::ProcessInfo>> collect() override;

private:
    ProcessListReader m_listReader;
    ProcessIdentityCache m_identityCache;
    SteadyClockReader m_clockReader;
    int m_logicalProcessorCount{1};
    ProcessCpuBaselines m_cpuBaselines;
};

} // namespace sysmon::platform
