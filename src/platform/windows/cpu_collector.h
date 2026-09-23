#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "domain/cpu_sample.h"
#include "monitoring/collector.h"

namespace sysmon::platform
{

/**
 * Raw 64-bit tick counts (in 100 ns units) from system times query.
 */
struct SystemTimesData
{
    uint64_t idleTime{0};
    uint64_t kernelTime{0};
    uint64_t userTime{0};
};

/**
 * Calculates CPU utilization percentage from previous and current system time snapshots.
 *
 * Total system capacity is (deltaKernel + deltaUser) because Windows kernel time includes idle time.
 * Pure function with no OS dependencies for deterministic testing.
 */
[[nodiscard]] float calculateCpuUsage(
    const SystemTimesData &previous,
    const SystemTimesData &current,
    std::chrono::nanoseconds monotonicElapsed);

/**
 * Total CPU usage collector for Windows using GetSystemTimes.
 *
 * Samples idle, kernel, and user times across all logical processors
 * and computes utilization percentage over monotonic elapsed time.
 */
class CpuCollector : public monitoring::ICpuCollector
{
public:
    using SystemTimesReader = std::function<bool(SystemTimesData &)>;
    using SteadyClockReader = std::function<std::chrono::steady_clock::time_point()>;

    explicit CpuCollector();
    CpuCollector(SystemTimesReader timesReader, SteadyClockReader clockReader, int coreCount);
    ~CpuCollector() override = default;

    [[nodiscard]] domain::CpuSample collect() override;

    [[nodiscard]] int coreCount() const;

private:
    SystemTimesReader m_timesReader;
    SteadyClockReader m_clockReader;
    int m_coreCount{1};

    SystemTimesData m_previousTimes{};
    std::chrono::steady_clock::time_point m_previousTimestamp{};
    bool m_hasBaseline{false};
};

} // namespace sysmon::platform

