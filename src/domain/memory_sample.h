#pragma once

#include <cstdint>
#include <optional>

namespace sysmon::domain
{

/**
 * A single snapshot of system physical and virtual memory state.
 * All byte values are in bytes. usagePercent is 0–100.
 * commitLimit and commitCurrent describe the system commit charge (paged pool
 * + private bytes across all processes). They can exceed physical RAM when a
 * page file is present.
 *
 * The remaining fields come from a separate query and are std::nullopt when
 * it fails:
 * - cachedBytes: the system cache, i.e. the standby list plus the system
 *   working set. Memory the system can repurpose, which Task Manager shows as
 *   "Cached".
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
    std::optional<uint64_t> cachedBytes;
    std::optional<uint64_t> pagedPoolBytes;
    std::optional<uint64_t> nonPagedPoolBytes;
};

} // namespace sysmon::domain
