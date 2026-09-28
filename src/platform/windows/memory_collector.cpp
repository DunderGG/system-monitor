#include "platform/windows/memory_collector.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <windows.h>

#include <spdlog/spdlog.h>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

std::optional<domain::MemorySample> calculateMemorySample(const MemoryStatusData& data)
{
    if (data.totalPhys == 0) {
        return std::nullopt;
    }

    const uint64_t total = data.totalPhys;
    const uint64_t available = std::min(data.availPhys, total);
    const uint64_t used = total - available;
    const float usagePercent = std::clamp(static_cast<float>(used) * 100.0f / static_cast<float>(total), 0.0f, 100.0f);

    const uint64_t commitLimit = data.totalPageFile;
    const uint64_t commitAvail = std::min(data.availPageFile, commitLimit);
    const uint64_t commitCurrent = commitLimit - commitAvail;

    return domain::MemorySample{
        .totalBytes = total,
        .availableBytes = available,
        .usagePercent = usagePercent,
        .commitLimit = commitLimit,
        .commitCurrent = commitCurrent,
    };
}

MemoryCollector::MemoryCollector()
    : m_reader([failureLog = RepeatedFailureLog{"GlobalMemoryStatusEx"}](MemoryStatusData& data) mutable {
          MEMORYSTATUSEX memStatus{};
          memStatus.dwLength = sizeof(MEMORYSTATUSEX);
          if (!::GlobalMemoryStatusEx(&memStatus)) {
              const DWORD error = ::GetLastError();
              failureLog.failure(spdlog::level::err, "GlobalMemoryStatusEx failed with error code: {}", error);
              return false;
          }
          failureLog.success();
          data.totalPhys = memStatus.ullTotalPhys;
          data.availPhys = memStatus.ullAvailPhys;
          data.totalPageFile = memStatus.ullTotalPageFile;
          data.availPageFile = memStatus.ullAvailPageFile;
          return true;
      })
{}

MemoryCollector::MemoryCollector(MemoryStatusReader reader) : m_reader(std::move(reader)) {}

std::optional<domain::MemorySample> MemoryCollector::collect()
{
    MemoryStatusData data{};
    if (!m_reader || !m_reader(data)) {
        return std::nullopt;
    }
    return calculateMemorySample(data);
}

} // namespace sysmon::platform
