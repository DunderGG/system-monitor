#include "app_logging.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace verdandi::app
{

namespace
{

constexpr std::size_t kMaxLogFileSize = 5U * 1024U * 1024U;
constexpr std::size_t kMaxLogFiles = 3U;

// A level's name, as the --log-level option and the settings file give it, and spdlog's level.
struct LevelEntry
{
    std::string_view name{};
    domain::LogLevel level{};
    spdlog::level::level_enum spdlogLevel{};
};

constexpr std::array kLevels{
    LevelEntry{"trace", domain::LogLevel::Trace, spdlog::level::trace},
    LevelEntry{"debug", domain::LogLevel::Debug, spdlog::level::debug},
    LevelEntry{"info", domain::LogLevel::Info, spdlog::level::info},
    LevelEntry{"warn", domain::LogLevel::Warn, spdlog::level::warn},
    LevelEntry{"error", domain::LogLevel::Error, spdlog::level::err},
    LevelEntry{"critical", domain::LogLevel::Critical, spdlog::level::critical},
    LevelEntry{"off", domain::LogLevel::Off, spdlog::level::off},
};

const LevelEntry* entryOf(domain::LogLevel level)
{
    const auto entry = std::ranges::find(kLevels, level, &LevelEntry::level);
    assert(entry != kLevels.end() && "every log level has an entry");
    return entry != kLevels.end() ? &*entry : nullptr;
}

// Returns why the file at logFilePath cannot be written, after creating its
// directory, or std::nullopt if it can. spdlog's file sink throws when it
// cannot open its file, and the project uses no exceptions, so the file is
// opened here first (DC-135).
std::optional<std::string> logFileProblem(const std::filesystem::path& logFilePath)
{
    if (logFilePath.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(logFilePath.parent_path(), error);
        if (error) {
            return std::format("Cannot create the file's directory: {}", error.message());
        }
    }
    // Opened as the sink opens it: for appending, sharing reads and writes.
    const std::ofstream file(logFilePath, std::ios::app);
    if (!file) {
        return std::string{"Cannot open the file for writing"};
    }
    return std::nullopt;
}

} // namespace

std::optional<domain::LogLevel> parseLogLevel(std::string_view name)
{
    const auto entry = std::ranges::find(kLevels, name, &LevelEntry::name);
    return entry != kLevels.end() ? std::optional{entry->level} : std::nullopt;
}

std::string_view logLevelName(domain::LogLevel level)
{
    const LevelEntry* entry = entryOf(level);
    return entry != nullptr ? entry->name : std::string_view{};
}

spdlog::level::level_enum toSpdlogLevel(domain::LogLevel level)
{
    const LevelEntry* entry = entryOf(level);
    return entry != nullptr ? entry->spdlogLevel : spdlog::level::info;
}

std::optional<std::string> configureLogging(const std::filesystem::path& logFilePath,
                                            spdlog::level::level_enum logLevel)
{
    // verdandi.exe is a GUI program, so its standard output is a handle only
    // when the starter redirects it, as build.ps1 -Run does; without one the
    // sink writes nothing (DC-134).
    std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    std::optional<std::string> problem = logFileProblem(logFilePath);
    if (!problem) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath.string(), kMaxLogFileSize,
                                                                               kMaxLogFiles));
    }

    auto logger = std::make_shared<spdlog::logger>(std::string{kApplicationLoggerName}, sinks.begin(), sinks.end());
    logger->set_level(logLevel);
    logger->set_pattern("[%Y-%m-%d %T.%e] [%^%l%$] %v");
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(std::move(logger));
    return problem;
}

} // namespace verdandi::app
