#include "app_logging.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace sysmon::app
{

namespace
{

constexpr std::size_t kMaxLogFileSize = 5U * 1024U * 1024U;
constexpr std::size_t kMaxLogFiles = 3U;

} // namespace

std::optional<spdlog::level::level_enum> parseLogLevel(std::string_view value)
{
    using LevelName = std::pair<std::string_view, spdlog::level::level_enum>;
    constexpr std::array kLevels{
        LevelName{"trace", spdlog::level::trace}, LevelName{"debug", spdlog::level::debug},
        LevelName{"info", spdlog::level::info},   LevelName{"warn", spdlog::level::warn},
        LevelName{"error", spdlog::level::err},   LevelName{"critical", spdlog::level::critical},
        LevelName{"off", spdlog::level::off},
    };

    for (const auto& [name, level] : kLevels) {
        if (value == name) {
            return level;
        }
    }

    return std::nullopt;
}

void configureLogging(const std::filesystem::path& logFilePath, spdlog::level::level_enum logLevel)
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath.string(), kMaxLogFileSize, kMaxLogFiles);

    auto logger = std::make_shared<spdlog::logger>("system_monitor", spdlog::sinks_init_list{consoleSink, fileSink});
    logger->set_level(logLevel);
    logger->set_pattern("[%Y-%m-%d %T.%e] [%^%l%$] %v");
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(std::move(logger));
}

} // namespace sysmon::app
