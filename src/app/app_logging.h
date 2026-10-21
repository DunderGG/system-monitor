#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <spdlog/common.h>

#include "domain/settings.h"

namespace sysmon::app
{

/** Name of the logger configureLogging() installs as the default. */
inline constexpr std::string_view kApplicationLoggerName = "verdandi";

/**
 * Returns the level that name names: trace, debug, info, warn, error,
 * critical or off, as the --log-level option and the settings file name
 * them; std::nullopt for any other name.
 */
[[nodiscard]] std::optional<domain::LogLevel> parseLogLevel(std::string_view name);

/** Returns the name that parseLogLevel() reads as level, e.g. "warn". */
[[nodiscard]] std::string_view logLevelName(domain::LogLevel level);

/** Returns spdlog's level for level. */
[[nodiscard]] spdlog::level::level_enum toSpdlogLevel(domain::LogLevel level);

void configureLogging(const std::filesystem::path& logFilePath, spdlog::level::level_enum logLevel);

} // namespace sysmon::app
