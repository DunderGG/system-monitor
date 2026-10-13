#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "domain/process_info.h"
#include "monitoring/collector.h"
#include "platform/windows/process_identity_cache.h"

namespace sysmon::platform
{

/**
 * Reads the running processes with the data the process list itself provides
 * (NtQuerySystemInformation in production). Returns std::nullopt on failure.
 */
using ProcessListReader = std::function<std::optional<std::vector<domain::ProcessInfo>>()>;

/**
 * Collector for the running processes.
 *
 * One NtQuerySystemInformation(SystemProcessInformation) call per collect()
 * returns the PID, parent PID, creation time, image name, thread and handle
 * counts, working set, private bytes, I/O byte counters, and CPU times of
 * every process, including protected and elevated ones, without opening any
 * of them. collect() returns std::nullopt when the list cannot be read.
 *
 * The image path and command line come from a ProcessDetailsReader, called
 * once per process identity through a ProcessIdentityCache. Without a details
 * reader they stay std::nullopt with ProcessDetailsStatus::NotQueried. The
 * reader is never called for lsass.exe or csrss.exe (isNeverOpenedProcess()),
 * whose details stay NotQueried.
 *
 * Not thread-safe; collect() must be called from one thread at a time.
 */
class ProcessCollector : public monitoring::IProcessCollector
{
public:
    /**
     * Reads the process list with NtQuerySystemInformation and each new
     * process's details with makeWindowsProcessDetailsReader().
     */
    ProcessCollector();

    /** Reads the process list and details with injected readers, for unit testing. */
    explicit ProcessCollector(ProcessListReader listReader, ProcessDetailsReader detailsReader = nullptr);

    ~ProcessCollector() override = default;

    [[nodiscard]] std::optional<std::vector<domain::ProcessInfo>> collect() override;

private:
    ProcessListReader m_listReader;
    ProcessIdentityCache m_identityCache;
};

} // namespace sysmon::platform
