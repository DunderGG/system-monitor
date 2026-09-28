#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

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
 * Returns std::nullopt when no rate can be computed: non-positive elapsed time, a counter that
 * went backwards (reset), or no CPU time elapsed at all.
 * Pure function with no OS dependencies for deterministic testing.
 */
[[nodiscard]] std::optional<float> calculateCpuUsage(const SystemTimesData& previous, const SystemTimesData& current,
                                                     std::chrono::nanoseconds monotonicElapsed);

/**
 * Total and per-core CPU usage collector for Windows using GetSystemTimes
 * and NtQuerySystemInformation(SystemProcessorPerformanceInformation).
 *
 * Samples idle, kernel, and user times across all logical processors
 * and computes utilization percentages over monotonic elapsed time.
 *
 * On a system with more than one processor group, GetSystemTimes reports only
 * the calling thread's group, so the total is computed by summing every core's
 * times instead. The same happens whenever GetSystemTimes fails.
 *
 * collect() returns std::nullopt when both queries fail or when no rate can be
 * computed yet (the first sample only establishes a baseline). Per-core values
 * are omitted (empty) rather than zero-filled when they cannot be computed.
 */
class CpuCollector : public monitoring::ICpuCollector
{
public:
    using SystemTimesReader = std::function<bool(SystemTimesData&)>;
    using CorePerformanceReader = std::function<bool(std::vector<SystemTimesData>&)>;
    using SteadyClockReader = std::function<std::chrono::steady_clock::time_point()>;

    explicit CpuCollector();
    CpuCollector(SystemTimesReader timesReader, SteadyClockReader clockReader, int coreCount);
    CpuCollector(SystemTimesReader timesReader, CorePerformanceReader coreReader, SteadyClockReader clockReader,
                 int coreCount, int processorGroupCount = 1);
    ~CpuCollector() override = default;

    [[nodiscard]] std::optional<domain::CpuSample> collect() override;

    [[nodiscard]] int coreCount() const;

private:
    // Reads both sources once and stores them as the baseline for the first rate.
    void establishBaseline();

    // Returns the times the total is computed from: GetSystemTimes on a
    // single-group system, otherwise the sum of all cores. std::nullopt when
    // neither source covers the whole machine this tick.
    [[nodiscard]] std::optional<SystemTimesData> totalTimes(bool hasTimes, const SystemTimesData& times,
                                                            bool hasCoreTimes,
                                                            const std::vector<SystemTimesData>& coreTimes) const;

    SystemTimesReader m_timesReader;
    CorePerformanceReader m_coreReader;
    SteadyClockReader m_clockReader;
    int m_coreCount{1};
    bool m_isMultiGroup{false};

    SystemTimesData m_previousTimes{};
    std::vector<SystemTimesData> m_previousCoreTimes{};
    std::chrono::steady_clock::time_point m_previousTimestamp{};
    bool m_hasBaseline{false};
};

} // namespace sysmon::platform
