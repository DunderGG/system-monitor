#pragma once

#include <cstdint>

namespace sysmon::domain
{

/**
 * System-wide counts of running processes, threads, and open kernel object
 * handles at one point in time. All are counts.
 */
struct SystemActivitySample
{
    uint32_t processCount{0};
    uint32_t threadCount{0};
    uint32_t handleCount{0};
};

} // namespace sysmon::domain
