#pragma once

#include <cstdint>

namespace sysmon::domain
{

// A single snapshot of system physical and virtual memory state.
// All byte values are in bytes. usagePercent is 0–100.
// commitLimit and commitCurrent describe the system commit charge (paged pool
// + private bytes across all processes). They can exceed physical RAM when a
// page file is present.
struct MemorySample
{
    uint64_t totalBytes{0};
    uint64_t availableBytes{0};
    float usagePercent{0.0f};
    uint64_t commitLimit{0};
    uint64_t commitCurrent{0};
};

} // namespace sysmon::domain
