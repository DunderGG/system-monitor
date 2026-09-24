#include "platform/windows/memory_collector.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include <Windows.h>

namespace sysmon::platform
{

domain::MemorySample calculateMemorySample(const MemoryStatusData &data)
{
    const uint64_t total = data.totalPhys;
    const uint64_t available = std::min(data.availPhys, total);
    const uint64_t used = total - available;
    const float usagePercent = total > 0
        ? std::clamp(static_cast<float>(used) * 100.0f / static_cast<float>(total), 0.0f, 100.0f)
        : 0.0f;

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
    : m_reader([](MemoryStatusData &data) {
          MEMORYSTATUSEX memStatus{};
          memStatus.dwLength = sizeof(MEMORYSTATUSEX);
          if (!::GlobalMemoryStatusEx(&memStatus)) {
              const DWORD error = ::GetLastError();
              spdlog::error("GlobalMemoryStatusEx failed with error code: {}", error);
              return false;
          }
          data.totalPhys = memStatus.ullTotalPhys;
          data.availPhys = memStatus.ullAvailPhys;
          data.totalPageFile = memStatus.ullTotalPageFile;
          data.availPageFile = memStatus.ullAvailPageFile;
          return true;
      })
{}

MemoryCollector::MemoryCollector(MemoryStatusReader reader)
    : m_reader(std::move(reader))
{}

domain::MemorySample MemoryCollector::collect()
{
    MemoryStatusData data{};
    if (!m_reader || !m_reader(data)) {
        return domain::MemorySample{};
    }
    return calculateMemorySample(data);
}

} // namespace sysmon::platform

