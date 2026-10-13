#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "process_identity.h"

namespace sysmon::domain
{

/**
 * Whether ProcessInfo::imagePath and ::commandLine could be read and, if not,
 * why. Reading them requires opening the process. Everything else in
 * ProcessInfo comes from the system process list, which covers every process
 * without opening any, so it is always present.
 */
enum class ProcessDetailsStatus
{
    // Not queried: a process the collector never opens (lsass.exe and
    // csrss.exe, see architecture.md, "Antivirus and EDR considerations"), or
    // a ProcessInfo that did not come from a collector that reads details.
    NotQueried,
    // The image path and the command line were both read.
    Available,
    // Windows denied opening the process or reading a detail. Details that
    // could be read are still set.
    AccessDenied,
    // A detail could not be read for another reason: the process exited before
    // it was queried, or it has no image file or command line (System,
    // Registry, and similar kernel processes). Details that could be read are
    // still set.
    Unavailable,
};

/**
 * One process in the system process list.
 *
 * The process is identified by (pid, creationTime); see identity().
 * creationTime is sourced from SYSTEM_PROCESS_INFORMATION.CreateTime (a
 * FILETIME value), converted to system_clock at the platform boundary.
 *
 * CPU time fields (cpuUserTimeMs, cpuKernelTimeMs) are cumulative totals in
 * milliseconds. Rate calculation (CPU %) is done by the collector using deltas
 * between successive snapshots over measured steady_clock intervals.
 *
 * imagePath and commandLine are optional because they are read by opening the
 * process, once per identity, and that can fail; detailsStatus says why a
 * detail is missing. The process itself is never omitted, so the UI can show
 * it with a clear indicator.
 */
struct ProcessInfo
{
    uint32_t pid{0};
    std::chrono::system_clock::time_point creationTime;
    uint32_t parentPid{0};
    std::string imageName;
    std::optional<std::string> imagePath;
    std::optional<std::string> commandLine;
    uint64_t cpuUserTimeMs{0};
    uint64_t cpuKernelTimeMs{0};
    uint64_t workingSetBytes{0};
    // Commit charge: the private memory committed for the process.
    uint64_t privateBytes{0};
    // Cumulative bytes moved by the process's read and write I/O operations,
    // which include files, pipes, and devices, not only disk.
    uint64_t ioReadBytes{0};
    uint64_t ioWriteBytes{0};
    uint32_t threadCount{0};
    uint32_t handleCount{0};
    ProcessDetailsStatus detailsStatus{ProcessDetailsStatus::NotQueried};

    /** Returns the (pid, creationTime) pair that identifies this process. */
    [[nodiscard]] ProcessIdentity identity() const
    {
        return ProcessIdentity{.pid = pid, .creationTime = creationTime};
    }
};

} // namespace sysmon::domain
