#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "monitoring/collector.h"

namespace sysmon::platform
{

/**
 * Reader callback type — injected in unit tests instead of calling
 * GetTickCount64 directly. Returns milliseconds since system boot.
 */
using TickCountReader = std::function<uint64_t()>;

/**
 * System uptime collector for Windows using GetTickCount64.
 *
 * GetTickCount64 cannot fail and uses a 64-bit counter, so it does not wrap
 * after 49.7 days like GetTickCount. Its resolution is the system timer
 * interval (typically 10–16 ms), which is ample for displaying uptime.
 * The value includes time spent in sleep and hibernation.
 */
class UptimeCollector : public monitoring::IUptimeCollector
{
public:
    /** Default constructor using GetTickCount64. */
    UptimeCollector();

    /** Injected constructor for deterministic unit testing. */
    explicit UptimeCollector(TickCountReader reader);

    ~UptimeCollector() override = default;

    [[nodiscard]] std::chrono::milliseconds collect() override;

private:
    TickCountReader m_reader;
};

} // namespace sysmon::platform
