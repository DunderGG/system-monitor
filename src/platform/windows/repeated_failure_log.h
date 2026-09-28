#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace sysmon::platform
{

/**
 * Logs the failures of a repeating operation without flooding the log.
 *
 * Collectors run their OS queries on every scheduler tick, so a persistent
 * failure (for example a BitLocker-locked volume) would otherwise be logged once
 * per second. The first failure after a success is logged at the requested
 * level and repeats go to debug. The first success after a failure is logged at
 * info with the number of consecutive failures.
 *
 * Not thread-safe; each instance belongs to one caller.
 */
class RepeatedFailureLog
{
public:
    /** operation names what failed in the recovery message, e.g. "GetSystemTimes". */
    explicit RepeatedFailureLog(std::string operation) : m_operation(std::move(operation)) {}

    /** Records a failure and logs it: at level the first time, at debug while it repeats. */
    template <typename... Args>
    void failure(spdlog::level::level_enum level, spdlog::format_string_t<Args...> format, Args&&... args)
    {
        ++m_consecutiveFailures;
        spdlog::log(m_consecutiveFailures == 1 ? level : spdlog::level::debug, format, std::forward<Args>(args)...);
    }

    /** Records a success and logs the recovery if the operation was failing. */
    void success()
    {
        if (m_consecutiveFailures > 0) {
            spdlog::info("{} succeeded again after {} consecutive failures", m_operation, m_consecutiveFailures);
            m_consecutiveFailures = 0;
        }
    }

    [[nodiscard]] uint64_t consecutiveFailures() const
    {
        return m_consecutiveFailures;
    }

private:
    std::string m_operation;
    uint64_t m_consecutiveFailures{0};
};

} // namespace sysmon::platform
