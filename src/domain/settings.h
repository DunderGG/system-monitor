#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string_view>

namespace verdandi::domain
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

/** A log level and its name, as the settings file and the --log-level option give it. */
struct LogLevelName
{
    std::string_view name{};
    LogLevel level{};
};

/** Every log level with its name, from Trace to Off. */
inline constexpr std::array kLogLevelNames{
    LogLevelName{"trace", LogLevel::Trace}, LogLevelName{"debug", LogLevel::Debug},
    LogLevelName{"info", LogLevel::Info},   LogLevelName{"warn", LogLevel::Warn},
    LogLevelName{"error", LogLevel::Error}, LogLevelName{"critical", LogLevel::Critical},
    LogLevelName{"off", LogLevel::Off},
};

/** Returns the name of level, e.g. "warn". */
[[nodiscard]] constexpr std::string_view logLevelName(LogLevel level)
{
    const auto entry = std::ranges::find(kLogLevelNames, level, &LogLevelName::level);
    return entry != kLogLevelNames.end() ? entry->name : std::string_view{};
}

/** Returns the level that name names, or std::nullopt for any other name, including one in another case. */
[[nodiscard]] constexpr std::optional<LogLevel> parseLogLevel(std::string_view name)
{
    const auto entry = std::ranges::find(kLogLevelNames, name, &LogLevelName::name);
    return entry != kLogLevelNames.end() ? std::optional{entry->level} : std::nullopt;
}

/**
 * The shortest and the longest refresh interval a user can choose, the step
 * between the intervals that can be chosen, and the default.
 */
inline constexpr std::chrono::milliseconds kMinRefreshInterval{500};
inline constexpr std::chrono::milliseconds kMaxRefreshInterval{10'000};
inline constexpr std::chrono::milliseconds kRefreshIntervalStep{500};
inline constexpr std::chrono::milliseconds kDefaultRefreshInterval{1000};

static_assert(kMinRefreshInterval % kRefreshIntervalStep == std::chrono::milliseconds::zero() &&
                  kMaxRefreshInterval % kRefreshIntervalStep == std::chrono::milliseconds::zero() &&
                  kDefaultRefreshInterval % kRefreshIntervalStep == std::chrono::milliseconds::zero(),
              "the limits and the default are intervals that can be chosen");

/**
 * Returns the refresh interval that can be chosen nearest to interval: the
 * nearest multiple of kRefreshIntervalStep, a half step rounded up, from
 * kMinRefreshInterval to kMaxRefreshInterval (DC-123).
 */
[[nodiscard]] constexpr std::chrono::milliseconds nearestRefreshInterval(std::chrono::milliseconds interval)
{
    // Within the limits first, which are multiples of the step, so the rounding cannot leave them.
    const std::chrono::milliseconds clamped = std::clamp(interval, kMinRefreshInterval, kMaxRefreshInterval);
    return (clamped + kRefreshIntervalStep / 2) / kRefreshIntervalStep * kRefreshIntervalStep;
}

/**
 * The user's preferences, which persistence reads from and writes to the
 * settings file (DC-123). Default-constructed, they are what the monitor does
 * without a settings file.
 * refreshInterval — how often the fast collectors are sampled and a snapshot
 *                   is emitted: a multiple of kRefreshIntervalStep from
 *                   kMinRefreshInterval to kMaxRefreshInterval.
 */
struct Settings
{
    std::chrono::milliseconds refreshInterval{kDefaultRefreshInterval};
    DisplayUnits displayUnits{};
    DashboardPanels dashboardPanels{};
    LogLevel logLevel{LogLevel::Info};

    bool operator==(const Settings&) const = default;
};

} // namespace verdandi::domain
