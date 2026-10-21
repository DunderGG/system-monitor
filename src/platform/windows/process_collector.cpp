#include "platform/windows/process_collector.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <windows.h>

#include <spdlog/spdlog.h>

#include "platform/windows/byte_rate.h"
#include "platform/windows/process_access_policy.h"
#include "platform/windows/process_details_reader.h"
#include "platform/windows/repeated_failure_log.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/system_process_information_reader.h"

namespace sysmon::platform
{

namespace
{

// Process CPU times cover every processor group, so the capacity they are
// shared across must too.
int activeLogicalProcessorCount()
{
    const DWORD count = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (count == 0) {
        spdlog::error("GetActiveProcessorCount failed with error code {}; process CPU usage unavailable",
                      ::GetLastError());
    }
    return static_cast<int>(count);
}

std::optional<float> cpuUsagePercent(const ProcessRateBaseline& baseline, uint64_t cpuTimeMs,
                                     std::chrono::duration<double, std::milli> elapsed, int logicalProcessorCount)
{
    if (elapsed.count() <= 0.0 || cpuTimeMs < baseline.cpuTimeMs || logicalProcessorCount <= 0) {
        return std::nullopt;
    }

    const double capacityMs = elapsed.count() * logicalProcessorCount;
    const double usage = static_cast<double>(cpuTimeMs - baseline.cpuTimeMs) * 100.0 / capacityMs;
    return static_cast<float>(std::clamp(usage, 0.0, 100.0));
}

} // namespace

void calculateProcessRates(std::vector<domain::ProcessInfo>& processes, ProcessRateBaselines& baselines,
                           std::chrono::steady_clock::time_point now, int logicalProcessorCount)
{
    ProcessRateBaselines nextBaselines;
    nextBaselines.reserve(processes.size());

    for (auto& process : processes) {
        const ProcessRateBaseline current{
            .cpuTimeMs = process.cpuUserTimeMs + process.cpuKernelTimeMs,
            .ioReadBytes = process.ioReadBytes,
            .ioWriteBytes = process.ioWriteBytes,
            .timestamp = now,
        };
        const auto identity = process.identity();

        process.cpuUsagePercent.reset();
        process.ioReadBytesPerSec.reset();
        process.ioWriteBytesPerSec.reset();
        if (const auto it = baselines.find(identity); it != baselines.end()) {
            const ProcessRateBaseline& previous = it->second;
            const auto elapsed = std::chrono::duration<double, std::milli>(now - previous.timestamp);
            const double elapsedSeconds = elapsed.count() / 1000.0;
            process.cpuUsagePercent = cpuUsagePercent(previous, current.cpuTimeMs, elapsed, logicalProcessorCount);
            process.ioReadBytesPerSec = calculateByteRate(previous.ioReadBytes, current.ioReadBytes, elapsedSeconds);
            process.ioWriteBytesPerSec = calculateByteRate(previous.ioWriteBytes, current.ioWriteBytes, elapsedSeconds);
        }
        nextBaselines.insert_or_assign(identity, current);
    }

    baselines = std::move(nextBaselines);
}

ProcessCollector::ProcessCollector()
    : ProcessCollector(
          [reader = SystemProcessInformationReader{},
           parseFailureLog = RepeatedFailureLog{"Parsing the process list"}]() mutable
              -> std::optional<std::vector<domain::ProcessInfo>> {
              const auto buffer = reader.read();
              if (!buffer) {
                  return std::nullopt;
              }

              auto processes = parseSystemProcessInformation(*buffer);
              if (!processes) {
                  parseFailureLog.failure(spdlog::level::err,
                                          "NtQuerySystemInformation(SystemProcessInformation) returned a malformed "
                                          "process list ({} bytes)",
                                          buffer->size());
                  return std::nullopt;
              }
              parseFailureLog.success();
              return processes;
          },
          makeWindowsProcessDetailsReader(), nullptr, activeLogicalProcessorCount())
{}

ProcessCollector::ProcessCollector(ProcessListReader listReader, ProcessDetailsReader detailsReader,
                                   SteadyClockReader clockReader, int logicalProcessorCount)
    : m_listReader(std::move(listReader)), m_identityCache(skippingNeverOpenedProcesses(std::move(detailsReader))),
      m_clockReader(std::move(clockReader)), m_logicalProcessorCount(logicalProcessorCount)
{}

std::optional<std::vector<domain::ProcessInfo>> ProcessCollector::collect()
{
    if (!m_listReader) {
        return std::nullopt;
    }

    auto processes = m_listReader();
    if (!processes) {
        return std::nullopt;
    }
    // Taken right after the list, so the elapsed time matches the counters in it.
    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();

    calculateProcessRates(*processes, m_rateBaselines, now, m_logicalProcessorCount);
    m_identityCache.apply(*processes);
    return processes;
}

void ProcessCollector::resetBaselines()
{
    m_rateBaselines.clear();
}

} // namespace sysmon::platform
