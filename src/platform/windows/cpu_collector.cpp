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

pfnNtQuerySystemInformation resolveNtQuerySystemInformation() noexcept
{
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        spdlog::error("GetModuleHandleW failed for ntdll.dll");
        return nullptr;
    }

    auto func = reinterpret_cast<pfnNtQuerySystemInformation>(
        ::GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (func == nullptr) {
        spdlog::error("GetProcAddress failed for NtQuerySystemInformation in ntdll.dll");
    }
    return func;
}

bool queryProcessorPerformance(
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
    auto ntQuery = resolveNtQuerySystemInformation();
    const int initialCores = m_coreCount;
    m_coreReader = [ntQuery, initialCores](std::vector<SystemTimesData> &coreTimes) {
        return queryProcessorPerformance(ntQuery, coreTimes, initialCores);
    };

    if (m_timesReader(m_previousTimes)) {
        if (m_coreReader) {
            m_coreReader(m_previousCoreTimes);
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

    if (!hasTimes) {
        return domain::CpuSample{
            .totalUsagePercent = 0.0f,
            .coreUsagePercents = {},
            .coreCount = m_coreCount,
        };
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
