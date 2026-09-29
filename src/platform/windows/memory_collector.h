#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "domain/memory_sample.h"
#include "monitoring/collector.h"
#include "platform/windows/performance_info_reader.h"

namespace sysmon::platform
{

/**
 * Raw memory metrics query output in bytes.
 */
struct MemoryStatusData
{
    uint64_t totalPhys{0};
    uint64_t availPhys{0};
    uint64_t totalPageFile{0};
    uint64_t availPageFile{0};
};

/**
 * Calculates a MemorySample from raw memory status data, adding the cache and
 * pool sizes from performance when it is given and has a non-zero page size.
 * Returns std::nullopt when total physical memory is zero, which is never a valid reading.
 * Pure function with no OS dependencies for deterministic unit testing.
 */
[[nodiscard]] std::optional<domain::MemorySample>
calculateMemorySample(const MemoryStatusData& data,
                      const std::optional<PerformanceInfoData>& performance = std::nullopt);

/**
 * Physical and virtual memory metrics collector for Windows using
 * GlobalMemoryStatusEx, with the cache and kernel pool sizes from
 * GetPerformanceInfo. collect() returns std::nullopt when GlobalMemoryStatusEx
 * fails or returns an invalid reading. When only GetPerformanceInfo fails, the
 * sample is still returned with the cache and pool sizes left empty.
 */
class MemoryCollector : public monitoring::IMemoryCollector
{
public:
    using MemoryStatusReader = std::function<bool(MemoryStatusData&)>;

    MemoryCollector();
    /** performanceReader may be empty, in which case the cache and pool sizes are never reported. */
    explicit MemoryCollector(MemoryStatusReader reader, PerformanceInfoReader performanceReader = nullptr);
    ~MemoryCollector() override = default;

    [[nodiscard]] std::optional<domain::MemorySample> collect() override;

private:
    MemoryStatusReader m_reader;
    PerformanceInfoReader m_performanceReader;
};

} // namespace sysmon::platform
