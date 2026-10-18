#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace sysmon::domain
{

/**
 * Identifies one process instance. Windows reuses PIDs, so a PID alone does
 * not; the creation time tells a new process apart from an earlier one that
 * had the same PID. Use it as the key for anything kept per process across
 * refreshes (cached details, rate baselines, table rows).
 */
struct ProcessIdentity
{
    uint32_t pid{0};
    std::chrono::system_clock::time_point creationTime{};

    friend bool operator==(const ProcessIdentity&, const ProcessIdentity&) = default;
};

/** Hash for ProcessIdentity, for unordered containers. */
struct ProcessIdentityHash
{
    [[nodiscard]] std::size_t operator()(const ProcessIdentity& identity) const noexcept
    {
        const std::size_t timeHash = std::hash<int64_t>{}(identity.creationTime.time_since_epoch().count());
        const std::size_t pidHash = std::hash<uint32_t>{}(identity.pid);
        return timeHash ^ (pidHash + 0x9E3779B97F4A7C15ULL + (timeHash << 6) + (timeHash >> 2));
    }
};

} // namespace sysmon::domain
