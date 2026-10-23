#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <spdlog/common.h>

#include "domain/settings.h"

namespace verdandi::app
{

/** Name of the logger configureLogging() installs as the default. */
inline constexpr std::string_view kApplicationLoggerName = "verdandi";

/** Returns spdlog's level for level. */
[[nodiscard]] spdlog::level::level_enum toSpdlogLevel(domain::LogLevel level);

/**
 * Makes the default logger write at logLevel to logFilePath, rotated at
 * 5 MB × 3 files, and to standard output when it is redirected. Creates the
 * file's directory. Returns std::nullopt when the file can be written, and
 * otherwise why it cannot; the logger then writes to standard output alone
 * (DC-135).
 */
[[nodiscard]] std::optional<std::string> configureLogging(const std::filesystem::path& logFilePath,
                                                          spdlog::level::level_enum logLevel);

} // namespace verdandi::app
