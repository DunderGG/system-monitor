#pragma once

#include <cstdint>
#include <optional>

namespace sysmon::domain
{

/**
 * A single snapshot of system physical and virtual memory state.
 * All byte values are in bytes. usagePercent is 0–100.
 * Used memory is totalBytes minus availableBytes, and usagePercent is its share
 * of the total. It includes the modified page list, which Task Manager counts
 * separately from its "In use" figure.
 * commitLimit and commitCurrent describe the system commit charge (paged pool
 * + private bytes across all processes). They can exceed physical RAM when a
 * page file is present.
 *
 * The remaining fields come from a separate query and are std::nullopt when
 * it fails:
 * - systemCacheBytes: PERFORMANCE_INFORMATION::SystemCache. Microsoft
 *   describes it as the standby list plus the system working set, but
 *   measured values match neither that sum nor Task Manager's "Cached"
 *   (standby plus modified lists); readings have fallen both below and above
 *   the standby list alone.
 * - pagedPoolBytes and nonPagedPoolBytes: kernel memory in the paged and
 *   non-paged pools.
 */
struct MemorySample
{
    uint64_t totalBytes{0};
    uint64_t availableBytes{0};
    float usagePercent{0.0f};
    uint64_t commitLimit{0};
    uint64_t commitCurrent{0};
    std::optional<uint64_t> systemCacheBytes;
    std::optional<uint64_t> pagedPoolBytes;
    std::optional<uint64_t> nonPagedPoolBytes;
};

} // namespace sysmon::domain
