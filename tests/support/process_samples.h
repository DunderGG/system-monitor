#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "domain/process_info.h"

namespace sysmon::tests
{

/**
 * A process named name, created createdAfterEpoch after the epoch. By default
 * it was created pid seconds after the epoch, so lower PIDs are older and
 * every PID has its own identity. Tests set any other field on the result.
 */
[[nodiscard]] inline domain::ProcessInfo process(uint32_t pid, std::string name,
                                                 std::optional<std::chrono::seconds> createdAfterEpoch = std::nullopt)
{
    return domain::ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{createdAfterEpoch.value_or(std::chrono::seconds{pid})},
        .imageName = std::move(name),
    };
}

/** A process started by parentPid; otherwise like process(). */
[[nodiscard]] inline domain::ProcessInfo child(uint32_t pid, std::string name, uint32_t parentPid,
                                               std::optional<std::chrono::seconds> createdAfterEpoch = std::nullopt)
{
    domain::ProcessInfo info = process(pid, std::move(name), createdAfterEpoch);
    info.parentPid = parentPid;
    return info;
}

/** Returns info with its CPU usage, a share of all logical processors, replaced. */
[[nodiscard]] inline domain::ProcessInfo withCpuUsage(domain::ProcessInfo info, std::optional<float> cpuUsagePercent)
{
    info.cpuUsagePercent = cpuUsagePercent;
    return info;
}

} // namespace sysmon::tests
