#include "platform/windows/cpu_collector.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>

#include <Windows.h>

#include <spdlog/spdlog.h>

namespace sysmon::platform
{

namespace
{

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
    if (m_timesReader(m_previousTimes)) {
        m_previousTimestamp = m_clockReader();
        m_hasBaseline = true;
    }
}

CpuCollector::CpuCollector(SystemTimesReader timesReader, SteadyClockReader clockReader, int coreCount)
    : m_timesReader(std::move(timesReader)),
      m_clockReader(std::move(clockReader)),
      m_coreCount(std::max(1, coreCount))
{
    if (m_timesReader && m_clockReader && m_timesReader(m_previousTimes)) {
        m_previousTimestamp = m_clockReader();
        m_hasBaseline = true;
    }
}

domain::CpuSample CpuCollector::collect()
{
    SystemTimesData currentTimes{};
    const auto now = m_clockReader ? m_clockReader() : std::chrono::steady_clock::now();

    if (!m_timesReader || !m_timesReader(currentTimes)) {
        return domain::CpuSample{
            .totalUsagePercent = 0.0f,
            .coreUsagePercents = {},
            .coreCount = m_coreCount,
        };
    }

    if (!m_hasBaseline) {
        m_previousTimes = currentTimes;
        m_previousTimestamp = now;
        m_hasBaseline = true;
        return domain::CpuSample{
            .totalUsagePercent = 0.0f,
            .coreUsagePercents = {},
            .coreCount = m_coreCount,
        };
    }

    const auto monotonicElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_previousTimestamp);
    const float usage = calculateCpuUsage(m_previousTimes, currentTimes, monotonicElapsed);

    m_previousTimes = currentTimes;
    m_previousTimestamp = now;

    return domain::CpuSample{
        .totalUsagePercent = usage,
        .coreUsagePercents = {},
        .coreCount = m_coreCount,
    };
}

int CpuCollector::coreCount() const
{
    return m_coreCount;
}

} // namespace sysmon::platform

