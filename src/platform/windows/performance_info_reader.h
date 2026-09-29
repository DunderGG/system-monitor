#pragma once

#include <cstdint>
#include <functional>

namespace sysmon::platform
{

/**
 * Raw output of GetPerformanceInfo. Memory figures are in pages of pageSize
 * bytes; counts are system-wide.
 */
struct PerformanceInfoData
{
    uint64_t pageSize{0};
    uint64_t systemCachePages{0};
    uint64_t kernelPagedPages{0};
    uint64_t kernelNonPagedPages{0};
    uint32_t processCount{0};
    uint32_t threadCount{0};
    uint32_t handleCount{0};
};

/** Fills the data and returns true on success; returns false when the query fails. */
using PerformanceInfoReader = std::function<bool(PerformanceInfoData&)>;

/**
 * Returns a reader that calls GetPerformanceInfo. Each reader logs its own
 * repeated failures once (see RepeatedFailureLog), so collectors that share
 * the query report independently.
 */
[[nodiscard]] PerformanceInfoReader makePerformanceInfoReader();

} // namespace sysmon::platform
