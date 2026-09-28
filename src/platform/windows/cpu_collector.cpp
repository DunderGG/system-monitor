#include "platform/windows/cpu_collector.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <Windows.h>

#include <spdlog/spdlog.h>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

namespace
{

// Failure logs for the per-core queries, kept by the core reader across ticks.
struct CoreQueryFailureLogs
{
    RepeatedFailureLog groupQuery{"NtQuerySystemInformationEx(SystemProcessorPerformanceInformation)"};
    RepeatedFailureLog legacyQuery{"NtQuerySystemInformation(SystemProcessorPerformanceInformation)"};
};

using NTSTATUS = LONG;
constexpr NTSTATUS kStatusSuccess = 0x00000000L;
constexpr NTSTATUS kStatusInfoLengthMismatch = static_cast<NTSTATUS>(0xC0000004L);

constexpr ULONG kSystemProcessorPerformanceInformation = 8;

struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
{
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
};

using pfnNtQuerySystemInformation = NTSTATUS(NTAPI*)(ULONG SystemInformationClass, PVOID SystemInformation,
                                                     ULONG SystemInformationLength, PULONG ReturnLength);

using pfnNtQuerySystemInformationEx = NTSTATUS(NTAPI*)(ULONG SystemInformationClass, PVOID InputBuffer,
                                                       ULONG InputBufferLength, PVOID SystemInformation,
                                                       ULONG SystemInformationLength, PULONG ReturnLength);

struct NtdllProcessorFunctions
{
    pfnNtQuerySystemInformationEx ntQuerySystemInformationEx{nullptr};
    pfnNtQuerySystemInformation ntQuerySystemInformation{nullptr};
};

NtdllProcessorFunctions resolveNtdllProcessorFunctions() noexcept
{
    NtdllProcessorFunctions funcs{};
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        spdlog::error("GetModuleHandleW failed for ntdll.dll");
        return funcs;
    }

    funcs.ntQuerySystemInformationEx =
        reinterpret_cast<pfnNtQuerySystemInformationEx>(::GetProcAddress(ntdll, "NtQuerySystemInformationEx"));
    if (funcs.ntQuerySystemInformationEx == nullptr) {
        spdlog::warn("GetProcAddress failed for NtQuerySystemInformationEx in ntdll.dll, using "
                     "NtQuerySystemInformation fallback");
    }

    funcs.ntQuerySystemInformation =
        reinterpret_cast<pfnNtQuerySystemInformation>(::GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (funcs.ntQuerySystemInformation == nullptr) {
        spdlog::error("GetProcAddress failed for NtQuerySystemInformation in ntdll.dll");
    }

    return funcs;
}

bool queryGroupProcessorPerformance(pfnNtQuerySystemInformationEx ntQuerySystemInfoEx, USHORT processorGroup,
                                    std::vector<SystemTimesData>& outGroupCores, int estimatedCoresInGroup,
                                    RepeatedFailureLog& failureLog)
{
    if (ntQuerySystemInfoEx == nullptr) {
        return false;
    }

    std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> buffer;
    buffer.resize(static_cast<std::size_t>(std::max(1, estimatedCoresInGroup)));

    ULONG returnLength = 0;
    constexpr int kMaxAttempts = 4;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const ULONG bufferSizeBytes =
            static_cast<ULONG>(buffer.size() * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));
        USHORT group = processorGroup;
        const NTSTATUS status = ntQuerySystemInfoEx(kSystemProcessorPerformanceInformation, &group, sizeof(USHORT),
                                                    buffer.data(), bufferSizeBytes, &returnLength);

        if (status == kStatusSuccess) {
            const std::size_t count =
                (returnLength > 0) ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)) : buffer.size();

            outGroupCores.reserve(outGroupCores.size() + count);
            for (std::size_t i = 0; i < count; ++i) {
                outGroupCores.push_back(SystemTimesData{
                    .idleTime = static_cast<uint64_t>(buffer[i].IdleTime.QuadPart),
                    .kernelTime = static_cast<uint64_t>(buffer[i].KernelTime.QuadPart),
                    .userTime = static_cast<uint64_t>(buffer[i].UserTime.QuadPart),
                });
            }
            return true;
        }

        if (status == kStatusInfoLengthMismatch) {
            const std::size_t requiredCount = (returnLength > 0)
                                                  ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION))
                                                  : (buffer.size() * 2);
            buffer.resize(std::max(buffer.size() + 1, requiredCount));
            continue;
        }

        failureLog.failure(spdlog::level::warn,
                           "NtQuerySystemInformationEx(group {}) failed with status 0x{:08X}; "
                           "falling back to NtQuerySystemInformation",
                           processorGroup, static_cast<uint32_t>(status));
        return false;
    }

    failureLog.failure(spdlog::level::warn,
                       "NtQuerySystemInformationEx buffer resizing loop exceeded max attempts for group {}; "
                       "falling back to NtQuerySystemInformation",
                       processorGroup);
    return false;
}

bool queryLegacyProcessorPerformance(pfnNtQuerySystemInformation ntQuerySystemInfo,
                                     std::vector<SystemTimesData>& outCoreTimes, int estimatedCoreCount,
                                     RepeatedFailureLog& failureLog)
{
    if (ntQuerySystemInfo == nullptr) {
        return false;
    }

    std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> buffer;
    buffer.resize(static_cast<std::size_t>(std::max(1, estimatedCoreCount)));

    ULONG returnLength = 0;
    constexpr int kMaxAttempts = 4;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const ULONG bufferSizeBytes =
            static_cast<ULONG>(buffer.size() * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));
        const NTSTATUS status =
            ntQuerySystemInfo(kSystemProcessorPerformanceInformation, buffer.data(), bufferSizeBytes, &returnLength);

        if (status == kStatusSuccess) {
            const std::size_t count =
                (returnLength > 0) ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)) : buffer.size();

            outCoreTimes.clear();
            outCoreTimes.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                outCoreTimes.push_back(SystemTimesData{
                    .idleTime = static_cast<uint64_t>(buffer[i].IdleTime.QuadPart),
                    .kernelTime = static_cast<uint64_t>(buffer[i].KernelTime.QuadPart),
                    .userTime = static_cast<uint64_t>(buffer[i].UserTime.QuadPart),
                });
            }
            return true;
        }

        if (status == kStatusInfoLengthMismatch) {
            const std::size_t requiredCount = (returnLength > 0)
                                                  ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION))
                                                  : (buffer.size() * 2);
            buffer.resize(std::max(buffer.size() + 1, requiredCount));
            continue;
        }

        failureLog.failure(
            spdlog::level::err,
            "NtQuerySystemInformation(SystemProcessorPerformanceInformation) failed with status 0x{:08X}",
            static_cast<uint32_t>(status));
        return false;
    }

    failureLog.failure(spdlog::level::err, "NtQuerySystemInformation buffer resizing loop exceeded max attempts");
    return false;
}

bool queryAllProcessorPerformance(const NtdllProcessorFunctions& funcs, std::vector<SystemTimesData>& outCoreTimes,
                                  int estimatedCoreCount, CoreQueryFailureLogs& failureLogs)
{
    const USHORT groupCount = ::GetActiveProcessorGroupCount();

    // Primary path: Use NtQuerySystemInformationEx across all active processor groups
    if (funcs.ntQuerySystemInformationEx != nullptr && groupCount > 0) {
        outCoreTimes.clear();
        bool allGroupsSucceeded = true;

        for (USHORT group = 0; group < groupCount; ++group) {
            const DWORD coresInGroup = ::GetActiveProcessorCount(group);
            if (!queryGroupProcessorPerformance(funcs.ntQuerySystemInformationEx, group, outCoreTimes,
                                                static_cast<int>(coresInGroup), failureLogs.groupQuery)) {
                allGroupsSucceeded = false;
                break;
            }
        }

        if (allGroupsSucceeded && !outCoreTimes.empty()) {
            failureLogs.groupQuery.success();
            return true;
        }

        if (allGroupsSucceeded) {
            failureLogs.groupQuery.failure(spdlog::level::warn, "NtQuerySystemInformationEx returned no processors; "
                                                                "falling back to NtQuerySystemInformation");
        }
        outCoreTimes.clear();
    }

    // Fallback path: Query primary group with NtQuerySystemInformation
    if (funcs.ntQuerySystemInformation != nullptr) {
        const bool hasCoreTimes = queryLegacyProcessorPerformance(funcs.ntQuerySystemInformation, outCoreTimes,
                                                                  estimatedCoreCount, failureLogs.legacyQuery);
        if (hasCoreTimes) {
            failureLogs.legacyQuery.success();
        }
        return hasCoreTimes;
    }

    return false;
}

constexpr uint64_t fileTimeToUInt64(const FILETIME& fileTime) noexcept
{
    return (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | static_cast<uint64_t>(fileTime.dwLowDateTime);
}

int detectLogicalCoreCount() noexcept
{
    const DWORD activeProcessors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (activeProcessors > 0) {
        return static_cast<int>(activeProcessors);
    }

    SYSTEM_INFO systemInfo{};
    ::GetSystemInfo(&systemInfo);
    return std::max(1, static_cast<int>(systemInfo.dwNumberOfProcessors));
}

SystemTimesData aggregateCoreTimes(const std::vector<SystemTimesData>& coreTimes) noexcept
{
    SystemTimesData sum{};
    for (const auto& core : coreTimes) {
        sum.idleTime += core.idleTime;
        sum.kernelTime += core.kernelTime;
        sum.userTime += core.userTime;
    }
    return sum;
}

} // namespace

std::optional<float> calculateCpuUsage(const SystemTimesData& previous, const SystemTimesData& current,
                                       std::chrono::nanoseconds monotonicElapsed)
{
    if (monotonicElapsed <= std::chrono::nanoseconds{0}) {
        return std::nullopt;
    }

    // A counter that went backwards means the counters were reset; the delta
    // across the reset is meaningless, so no rate can be reported.
    if (current.idleTime < previous.idleTime || current.kernelTime < previous.kernelTime ||
        current.userTime < previous.userTime) {
        return std::nullopt;
    }

    const uint64_t deltaIdle = current.idleTime - previous.idleTime;
    const uint64_t deltaKernel = current.kernelTime - previous.kernelTime;
    const uint64_t deltaUser = current.userTime - previous.userTime;

    const uint64_t deltaTotal = deltaKernel + deltaUser;
    if (deltaTotal == 0) {
        return std::nullopt;
    }

    const uint64_t deltaBusy = (deltaTotal > deltaIdle) ? (deltaTotal - deltaIdle) : 0;
    const float usage = static_cast<float>(deltaBusy) * 100.0f / static_cast<float>(deltaTotal);
    return std::clamp(usage, 0.0f, 100.0f);
}

CpuCollector::CpuCollector()
    : m_timesReader([failureLog = RepeatedFailureLog{"GetSystemTimes"}](SystemTimesData& data) mutable {
          FILETIME idleTime{};
          FILETIME kernelTime{};
          FILETIME userTime{};

          if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
              const DWORD errorCode = ::GetLastError();
              failureLog.failure(spdlog::level::err, "GetSystemTimes failed with error code: {}", errorCode);
              return false;
          }
          failureLog.success();

          data.idleTime = fileTimeToUInt64(idleTime);
          data.kernelTime = fileTimeToUInt64(kernelTime);
          data.userTime = fileTimeToUInt64(userTime);
          return true;
      }),
      m_clockReader([] { return std::chrono::steady_clock::now(); }), m_coreCount(detectLogicalCoreCount()),
      m_isMultiGroup(::GetActiveProcessorGroupCount() > 1)
{
    const auto funcs = resolveNtdllProcessorFunctions();
    const int initialCores = m_coreCount;
    m_coreReader = [funcs, initialCores,
                    failureLogs = CoreQueryFailureLogs{}](std::vector<SystemTimesData>& coreTimes) mutable {
        return queryAllProcessorPerformance(funcs, coreTimes, initialCores, failureLogs);
    };

    establishBaseline();
}

CpuCollector::CpuCollector(SystemTimesReader timesReader, SteadyClockReader clockReader, int coreCount)
    : CpuCollector(std::move(timesReader), nullptr, std::move(clockReader), coreCount)
{}

CpuCollector::CpuCollector(SystemTimesReader timesReader, CorePerformanceReader coreReader,
                           SteadyClockReader clockReader, int coreCount, int processorGroupCount)
    : m_timesReader(std::move(timesReader)), m_coreReader(std::move(coreReader)), m_clockReader(std::move(clockReader)),
      m_coreCount(std::max(1, coreCount)), m_isMultiGroup(processorGroupCount > 1)
{
    if (m_clockReader) {
        establishBaseline();
    }
}

void CpuCollector::establishBaseline()
{
    SystemTimesData times{};
    std::vector<SystemTimesData> coreTimes;
    const bool hasTimes = m_timesReader && m_timesReader(times);
    const bool hasCoreTimes = m_coreReader && m_coreReader(coreTimes);

    if (const auto total = totalTimes(hasTimes, times, hasCoreTimes, coreTimes)) {
        m_previousTimes = *total;
        m_previousCoreTimes = std::move(coreTimes);
        m_previousTimestamp = m_clockReader();
        m_hasBaseline = true;
    }
}

std::optional<SystemTimesData> CpuCollector::totalTimes(bool hasTimes, const SystemTimesData& times, bool hasCoreTimes,
                                                        const std::vector<SystemTimesData>& coreTimes) const
{
    // GetSystemTimes covers only the calling thread's processor group, so on a
    // multi-group system the total must come from all cores. With a single
    // group, the per-core sum is also the fallback when GetSystemTimes fails.
    if (hasCoreTimes && (m_isMultiGroup || !hasTimes)) {
        return aggregateCoreTimes(coreTimes);
    }
    // On a multi-group system GetSystemTimes alone would present one group as
    // the whole machine, so no total is reported.
    if (hasTimes && !m_isMultiGroup) {
        return times;
    }
    return std::nullopt;
}

std::optional<domain::CpuSample> CpuCollector::collect()
{
    SystemTimesData times{};
    std::vector<SystemTimesData> currentCoreTimes;
    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();

    const bool hasTimes = m_timesReader && m_timesReader(times);
    const bool hasCoreTimes = m_coreReader && m_coreReader(currentCoreTimes);

    // No usable total this tick: keep the previous baseline, so the next
    // successful tick measures over the longer interval.
    const auto total = totalTimes(hasTimes, times, hasCoreTimes, currentCoreTimes);
    if (!total) {
        return std::nullopt;
    }
    const SystemTimesData& currentTimes = *total;

    if (!m_hasBaseline) {
        // A usage rate needs two samples; this one only establishes the baseline.
        m_previousTimes = currentTimes;
        m_previousCoreTimes = std::move(currentCoreTimes);
        m_previousTimestamp = now;
        m_hasBaseline = true;
        return std::nullopt;
    }

    const auto monotonicElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_previousTimestamp);
    const auto totalUsage = calculateCpuUsage(m_previousTimes, currentTimes, monotonicElapsed);

    // Per-core values are reported all-or-nothing: a partial list would misalign
    // core indices, and zero-filling would misreport missing data as idle cores.
    std::vector<float> coreUsages;
    if (hasCoreTimes && currentCoreTimes.size() == m_previousCoreTimes.size()) {
        coreUsages.reserve(currentCoreTimes.size());
        for (std::size_t i = 0; i < currentCoreTimes.size(); ++i) {
            const auto coreUsage = calculateCpuUsage(m_previousCoreTimes[i], currentCoreTimes[i], monotonicElapsed);
            if (!coreUsage) {
                coreUsages.clear();
                break;
            }
            coreUsages.push_back(*coreUsage);
        }
    }

    const int coreCount = hasCoreTimes ? static_cast<int>(currentCoreTimes.size()) : m_coreCount;

    m_previousTimes = currentTimes;
    m_previousCoreTimes = std::move(currentCoreTimes);
    m_previousTimestamp = now;

    if (!totalUsage) {
        return std::nullopt;
    }

    return domain::CpuSample{
        .totalUsagePercent = *totalUsage,
        .coreUsagePercents = std::move(coreUsages),
        .coreCount = coreCount,
    };
}

int CpuCollector::coreCount() const
{
    return m_coreCount;
}

} // namespace sysmon::platform
