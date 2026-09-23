#include "platform/windows/cpu_collector.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

#include <Windows.h>

#include <spdlog/spdlog.h>

namespace sysmon::platform
{

namespace
{

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

using pfnNtQuerySystemInformation = NTSTATUS(NTAPI *)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

using pfnNtQuerySystemInformationEx = NTSTATUS(NTAPI *)(
    ULONG SystemInformationClass,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

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

    funcs.ntQuerySystemInformationEx = reinterpret_cast<pfnNtQuerySystemInformationEx>(
        ::GetProcAddress(ntdll, "NtQuerySystemInformationEx"));
    if (funcs.ntQuerySystemInformationEx == nullptr) {
        spdlog::warn("GetProcAddress failed for NtQuerySystemInformationEx in ntdll.dll, using NtQuerySystemInformation fallback");
    }

    funcs.ntQuerySystemInformation = reinterpret_cast<pfnNtQuerySystemInformation>(
        ::GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (funcs.ntQuerySystemInformation == nullptr) {
        spdlog::error("GetProcAddress failed for NtQuerySystemInformation in ntdll.dll");
    }

    return funcs;
}

bool queryGroupProcessorPerformance(
    pfnNtQuerySystemInformationEx ntQuerySystemInfoEx,
    USHORT processorGroup,
    std::vector<SystemTimesData> &outGroupCores,
    int estimatedCoresInGroup)
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
        const NTSTATUS status = ntQuerySystemInfoEx(
            kSystemProcessorPerformanceInformation,
            &group,
            sizeof(USHORT),
            buffer.data(),
            bufferSizeBytes,
            &returnLength
        );

        if (status == kStatusSuccess) {
            const std::size_t count = (returnLength > 0)
                ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION))
                : buffer.size();

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

        spdlog::warn("NtQuerySystemInformationEx(group {}) failed with status 0x{:08X}",
                     processorGroup, static_cast<uint32_t>(status));
        return false;
    }

    spdlog::warn("NtQuerySystemInformationEx buffer resizing loop exceeded max attempts for group {}", processorGroup);
    return false;
}

bool queryLegacyProcessorPerformance(
    pfnNtQuerySystemInformation ntQuerySystemInfo,
    std::vector<SystemTimesData> &outCoreTimes,
    int estimatedCoreCount)
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
        const NTSTATUS status = ntQuerySystemInfo(
            kSystemProcessorPerformanceInformation,
            buffer.data(),
            bufferSizeBytes,
            &returnLength
        );

        if (status == kStatusSuccess) {
            const std::size_t count = (returnLength > 0)
                ? (returnLength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION))
                : buffer.size();

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

        spdlog::error("NtQuerySystemInformation(SystemProcessorPerformanceInformation) failed with status 0x{:08X}",
                      static_cast<uint32_t>(status));
        return false;
    }

    spdlog::error("NtQuerySystemInformation buffer resizing loop exceeded max attempts");
    return false;
}

bool queryAllProcessorPerformance(
    const NtdllProcessorFunctions &funcs,
    std::vector<SystemTimesData> &outCoreTimes,
    int estimatedCoreCount)
{
    const USHORT groupCount = ::GetActiveProcessorGroupCount();

    // Primary path: Use NtQuerySystemInformationEx across all active processor groups
    if (funcs.ntQuerySystemInformationEx != nullptr && groupCount > 0) {
        outCoreTimes.clear();
        bool allGroupsSucceeded = true;

        for (USHORT group = 0; group < groupCount; ++group) {
            const DWORD coresInGroup = ::GetActiveProcessorCount(group);
            if (!queryGroupProcessorPerformance(
                    funcs.ntQuerySystemInformationEx,
                    group,
                    outCoreTimes,
                    static_cast<int>(coresInGroup))) {
                allGroupsSucceeded = false;
                break;
            }
        }

        if (allGroupsSucceeded && !outCoreTimes.empty()) {
            return true;
        }

        spdlog::warn("NtQuerySystemInformationEx failed across groups, falling back to NtQuerySystemInformation");
        outCoreTimes.clear();
    }

    // Fallback path: Query primary group with NtQuerySystemInformation
    if (funcs.ntQuerySystemInformation != nullptr) {
        return queryLegacyProcessorPerformance(funcs.ntQuerySystemInformation, outCoreTimes, estimatedCoreCount);
    }

    return false;
}

constexpr uint64_t fileTimeToUInt64(const FILETIME &fileTime) noexcept
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

SystemTimesData aggregateCoreTimes(const std::vector<SystemTimesData> &coreTimes) noexcept
{
    SystemTimesData sum{};
    for (const auto &core : coreTimes) {
        sum.idleTime += core.idleTime;
        sum.kernelTime += core.kernelTime;
        sum.userTime += core.userTime;
    }
    return sum;
}

} // namespace

float calculateCpuUsage(
    const SystemTimesData &previous,
    const SystemTimesData &current,
    std::chrono::nanoseconds monotonicElapsed)
{
    if (monotonicElapsed <= std::chrono::nanoseconds{0}) {
        return 0.0f;
    }

    const uint64_t deltaIdle = (current.idleTime >= previous.idleTime)
        ? (current.idleTime - previous.idleTime)
        : 0;
    const uint64_t deltaKernel = (current.kernelTime >= previous.kernelTime)
        ? (current.kernelTime - previous.kernelTime)
        : 0;
    const uint64_t deltaUser = (current.userTime >= previous.userTime)
        ? (current.userTime - previous.userTime)
        : 0;

    const uint64_t deltaTotal = deltaKernel + deltaUser;
    if (deltaTotal == 0) {
        return 0.0f;
    }

    const uint64_t deltaBusy = (deltaTotal > deltaIdle) ? (deltaTotal - deltaIdle) : 0;
    const float usage = static_cast<float>(deltaBusy) * 100.0f / static_cast<float>(deltaTotal);
    return std::clamp(usage, 0.0f, 100.0f);
}

CpuCollector::CpuCollector()
    : m_timesReader([](SystemTimesData &data) {
          FILETIME idleTime{};
          FILETIME kernelTime{};
          FILETIME userTime{};

          if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
              const DWORD errorCode = ::GetLastError();
              spdlog::error("GetSystemTimes failed with error code: {}", errorCode);
              return false;
          }

          data.idleTime = fileTimeToUInt64(idleTime);
          data.kernelTime = fileTimeToUInt64(kernelTime);
          data.userTime = fileTimeToUInt64(userTime);
          return true;
      }),
      m_clockReader([] { return std::chrono::steady_clock::now(); }),
      m_coreCount(detectLogicalCoreCount())
{
    const auto funcs = resolveNtdllProcessorFunctions();
    const int initialCores = m_coreCount;
    m_coreReader = [funcs, initialCores](std::vector<SystemTimesData> &coreTimes) {
        return queryAllProcessorPerformance(funcs, coreTimes, initialCores);
    };

    if (m_timesReader(m_previousTimes)) {
        if (m_coreReader) {
            m_coreReader(m_previousCoreTimes);
            if (m_previousCoreTimes.size() > 64) {
                m_previousTimes = aggregateCoreTimes(m_previousCoreTimes);
            }
        }
        m_previousTimestamp = m_clockReader();
        m_hasBaseline = true;
    }
}

CpuCollector::CpuCollector(SystemTimesReader timesReader, SteadyClockReader clockReader, int coreCount)
    : CpuCollector(std::move(timesReader), nullptr, std::move(clockReader), coreCount)
{}

CpuCollector::CpuCollector(
    SystemTimesReader timesReader,
    CorePerformanceReader coreReader,
    SteadyClockReader clockReader,
    int coreCount)
    : m_timesReader(std::move(timesReader)),
      m_coreReader(std::move(coreReader)),
      m_clockReader(std::move(clockReader)),
      m_coreCount(std::max(1, coreCount))
{
    if (m_timesReader && m_clockReader && m_timesReader(m_previousTimes)) {
        if (m_coreReader) {
            m_coreReader(m_previousCoreTimes);
            if (m_previousCoreTimes.size() > 64) {
                m_previousTimes = aggregateCoreTimes(m_previousCoreTimes);
            }
        }
        m_previousTimestamp = m_clockReader();
        m_hasBaseline = true;
    }
}

domain::CpuSample CpuCollector::collect()
{
    SystemTimesData currentTimes{};
    std::vector<SystemTimesData> currentCoreTimes;
    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();

    const bool hasTimes = m_timesReader && m_timesReader(currentTimes);
    const bool hasCoreTimes = m_coreReader && m_coreReader(currentCoreTimes);

    if (!hasTimes && !hasCoreTimes) {
        return domain::CpuSample{
            .totalUsagePercent = 0.0f,
            .coreUsagePercents = {},
            .coreCount = m_coreCount,
        };
    }

    // For multi-group systems (>64 cores) or if GetSystemTimes failed,
    // aggregate all cores to ensure all processor groups are included in total CPU.
    if (hasCoreTimes && (currentCoreTimes.size() > 64 || !hasTimes)) {
        currentTimes = aggregateCoreTimes(currentCoreTimes);
    }

    if (!m_hasBaseline) {
        m_previousTimes = currentTimes;
        m_previousCoreTimes = std::move(currentCoreTimes);
        m_previousTimestamp = now;
        m_hasBaseline = true;

        std::vector<float> initialCoreUsages;
        if (hasCoreTimes) {
            initialCoreUsages.assign(m_previousCoreTimes.size(), 0.0f);
        }

        const int coreCount = !initialCoreUsages.empty() ? static_cast<int>(initialCoreUsages.size()) : m_coreCount;

        return domain::CpuSample{
            .totalUsagePercent = 0.0f,
            .coreUsagePercents = std::move(initialCoreUsages),
            .coreCount = coreCount,
        };
    }

    const auto monotonicElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_previousTimestamp);
    const float totalUsage = calculateCpuUsage(m_previousTimes, currentTimes, monotonicElapsed);

    std::vector<float> coreUsages;
    if (hasCoreTimes && currentCoreTimes.size() == m_previousCoreTimes.size()) {
        coreUsages.reserve(currentCoreTimes.size());
        for (std::size_t i = 0; i < currentCoreTimes.size(); ++i) {
            coreUsages.push_back(calculateCpuUsage(m_previousCoreTimes[i], currentCoreTimes[i], monotonicElapsed));
        }
    } else if (hasCoreTimes) {
        coreUsages.assign(currentCoreTimes.size(), 0.0f);
    }

    const int coreCount = !coreUsages.empty() ? static_cast<int>(coreUsages.size()) : m_coreCount;

    m_previousTimes = currentTimes;
    m_previousCoreTimes = std::move(currentCoreTimes);
    m_previousTimestamp = now;

    return domain::CpuSample{
        .totalUsagePercent = totalUsage,
        .coreUsagePercents = std::move(coreUsages),
        .coreCount = coreCount,
    };
}

int CpuCollector::coreCount() const
{
    return m_coreCount;
}

} // namespace sysmon::platform
