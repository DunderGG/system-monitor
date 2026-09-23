#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <spdlog/common.h>

namespace sysmon::app
{

[[nodiscard]] std::optional<spdlog::level::level_enum> parseLogLevel(std::string_view value);

void configureLogging(const std::filesystem::path &logFilePath, spdlog::level::level_enum logLevel);

} // namespace sysmon::app
