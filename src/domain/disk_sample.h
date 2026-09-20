#pragma once

#include <cstdint>
#include <string>

namespace sysmon::domain
{

// A single snapshot of one logical volume's disk space.
// volumeName is a human-readable identifier, e.g. "C:\".
// All byte values are in bytes. usagePercent is 0–100.
struct DiskSample
{
    std::string volumeName;
    uint64_t    totalBytes{0};
    uint64_t    freeBytes{0};
    float       usagePercent{0.0f};
};

} // namespace sysmon::domain

