#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace sysmon::domain
{

// Uniquely identifies a process as (pid, creationTime).
// Windows reuses PIDs, so PID alone is insufficient for stable identity.
// creationTime is sourced from SYSTEM_PROCESS_INFORMATION.CreateTime
// (a FILETIME value), converted to system_clock at the platform boundary.
//
// CPU time fields (cpuUserTimeMs, cpuKernelTimeMs) are cumulative totals in
// milliseconds. Rate calculation (CPU %) is done by the collector using deltas
// between successive snapshots over measured steady_clock intervals.
//
// imagePath and commandLine are optional because:
//   - They are queried lazily (on first detection of a new PID+creationTime).
//   - They may be unavailable for protected or system processes even with
//     PROCESS_QUERY_LIMITED_INFORMATION.
//
// accessDenied is set when the process metrics could not be retrieved at all.
// The struct is still present in the snapshot so the UI can show the process
// name with a clear indicator rather than silently omitting it.
struct ProcessInfo
{
    uint32_t                                  pid{0};
    std::chrono::system_clock::time_point     creationTime;
    uint32_t                                  parentPid{0};
    std::string                               imageName;
    std::optional<std::string>                imagePath;
    std::optional<std::string>                commandLine;
    uint64_t                                  cpuUserTimeMs{0};
    uint64_t                                  cpuKernelTimeMs{0};
    uint64_t                                  workingSetBytes{0};
    uint64_t                                  privateBytes{0};
    uint64_t                                  ioReadBytes{0};
    uint64_t                                  ioWriteBytes{0};
    uint32_t                                  threadCount{0};
    uint32_t                                  handleCount{0};
    bool                                      accessDenied{false};
};

} // namespace sysmon::domain

