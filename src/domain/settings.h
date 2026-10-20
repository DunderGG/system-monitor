#pragma once

#include <chrono>

namespace sysmon::domain
{

/**
 * The units of byte quantities, such as memory, disk space and I/O rates.
 * Binary  — powers of 1,024 with IEC names: KiB, MiB, GiB.
 * Decimal — powers of 1,000 with SI names: KB, MB, GB.
 */
enum class ByteUnits
{
    Binary,
    Decimal,
};

/**
 * The units of network throughput, the adapters' receive and send rates.
 * BytesPerSecond — bytes per second in the ByteUnits, e.g. "1.2 MiB/s".
 * BitsPerSecond  — bits per second in powers of 1,000, as link speeds are,
 *                  e.g. "10.1 Mbps", whatever the ByteUnits.
 */
enum class NetworkRateUnits
{
    BytesPerSecond,
    BitsPerSecond,
};

/** The units the UI shows quantities in. The defaults are the units it shows without settings. */
struct DisplayUnits
{
    ByteUnits bytes{ByteUnits::Binary};
    NetworkRateUnits networkRates{NetworkRateUnits::BytesPerSecond};

    bool operator==(const DisplayUnits&) const = default;
};

/** Which of the dashboard's resource cards are shown. */
struct DashboardPanels
{
    bool cpu{true};
    bool memory{true};
    bool disk{true};
    bool network{true};
    bool uptime{true};

    bool operator==(const DashboardPanels&) const = default;
};

/** The least severe level of message that the log keeps. Off keeps none. */
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

/** The shortest and the longest refresh interval a user can choose, and the default. */
inline constexpr std::chrono::milliseconds kMinRefreshInterval{500};
inline constexpr std::chrono::milliseconds kMaxRefreshInterval{10'000};
inline constexpr std::chrono::milliseconds kDefaultRefreshInterval{1000};

/**
 * The user's preferences, which persistence reads from and writes to the
 * settings file (DC-123). Default-constructed, they are what the monitor does
 * without a settings file.
 * refreshInterval — how often the fast collectors are sampled and a snapshot
 *                   is emitted, from kMinRefreshInterval to kMaxRefreshInterval.
 */
struct Settings
{
    std::chrono::milliseconds refreshInterval{kDefaultRefreshInterval};
    DisplayUnits displayUnits{};
    DashboardPanels dashboardPanels{};
    LogLevel logLevel{LogLevel::Info};

    bool operator==(const Settings&) const = default;
};

} // namespace sysmon::domain
