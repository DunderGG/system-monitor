#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "domain/process_info.h"
#include "monitoring/collector.h"

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
 * Not thread-safe; collect() must be called from one thread at a time.
 */
class ProcessCollector : public monitoring::IProcessCollector
{
public:
    /** Reads the process list with NtQuerySystemInformation. */
    ProcessCollector();

    /** Reads the process list with an injected reader, for unit testing. */
    explicit ProcessCollector(ProcessListReader listReader);

    ~ProcessCollector() override = default;

    [[nodiscard]] std::optional<std::vector<domain::ProcessInfo>> collect() override;

private:
    ProcessListReader m_listReader;
};

} // namespace sysmon::platform
