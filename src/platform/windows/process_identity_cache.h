#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/process_identity.h"
#include "domain/process_info.h"

namespace sysmon::platform
{

/** The details of a process that can only be read by opening it. */
struct ProcessDetails
{
    std::optional<std::string> imagePath;
    std::optional<std::string> commandLine;
    domain::ProcessDetailsStatus status{domain::ProcessDetailsStatus::NotQueried};
};

/**
 * Reads the details of one process from the list (in production: OpenProcess,
 * QueryFullProcessImageNameW, and NtQueryInformationProcess).
 */
using ProcessDetailsReader = std::function<ProcessDetails(const domain::ProcessInfo& process)>;

/**
 * Caches each process's details by its identity, (PID, creation time), so a
 * process is opened once in its lifetime rather than on every refresh.
 *
 * apply() fills in the details of every process in a fresh list: from the
 * cache for processes it has seen, and from the reader for new ones. The result
 * is cached whatever its status, so a process that denied access is not asked
 * again. Entries for processes missing from the list are then dropped, so the
 * cache never outgrows the process list, and a reused PID with a new creation
 * time is read again.
 *
 * Not thread-safe; each instance belongs to one caller.
 */
class ProcessIdentityCache
{
public:
    /** With a null reader, every process's details stay NotQueried. */
    explicit ProcessIdentityCache(ProcessDetailsReader reader);

    /** Fills imagePath, commandLine, and detailsStatus of every process in the list. */
    void apply(std::vector<domain::ProcessInfo>& processes);

    /** Returns the number of cached processes. */
    [[nodiscard]] std::size_t size() const;

private:
    struct Entry
    {
        ProcessDetails details;
        // The apply() call that last saw the process.
        uint64_t lastSeen{0};
    };

    ProcessDetailsReader m_reader;
    std::unordered_map<domain::ProcessIdentity, Entry, domain::ProcessIdentityHash> m_entries;
    uint64_t m_generation{0};
};

} // namespace sysmon::platform
