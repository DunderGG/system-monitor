#pragma once

#include <cstdint>
#include <functional>

#include "domain/memory_sample.h"
#include "monitoring/collector.h"

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
 * Calculates a MemorySample from raw memory status data.
 * Pure function with no OS dependencies for deterministic unit testing.
 */
[[nodiscard]] domain::MemorySample calculateMemorySample(const MemoryStatusData &data);

/**
 * Physical and virtual memory metrics collector for Windows using GlobalMemoryStatusEx.
 */
class MemoryCollector : public monitoring::IMemoryCollector
{
public:
    using MemoryStatusReader = std::function<bool(MemoryStatusData &)>;

    explicit MemoryCollector();
    explicit MemoryCollector(MemoryStatusReader reader);
    ~MemoryCollector() override = default;

    [[nodiscard]] domain::MemorySample collect() override;

private:
    MemoryStatusReader m_reader;
};

} // namespace sysmon::platform

