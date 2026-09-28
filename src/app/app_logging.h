#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <spdlog/common.h>

namespace sysmon::app
{

/** Name of the logger configureLogging() installs as the default. */
inline constexpr std::string_view kApplicationLoggerName = "system_monitor";

[[nodiscard]] std::optional<spdlog::level::level_enum> parseLogLevel(std::string_view value);

void configureLogging(const std::filesystem::path& logFilePath, spdlog::level::level_enum logLevel);

} // namespace sysmon::app
