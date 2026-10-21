#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "app/app_logging.h"
#include "domain/settings.h"

using verdandi::app::logLevelName;
using verdandi::app::parseLogLevel;
using verdandi::app::toSpdlogLevel;
using verdandi::domain::LogLevel;

TEST(AppLogging, LogLevels_NameAndSpdlogLevelOfEach)
{
    struct Case
    {
        std::string_view name;
        LogLevel level;
        spdlog::level::level_enum spdlogLevel;
    };
    const std::vector<Case> cases{
        {"trace", LogLevel::Trace, spdlog::level::trace}, {"debug", LogLevel::Debug, spdlog::level::debug},
        {"info", LogLevel::Info, spdlog::level::info},    {"warn", LogLevel::Warn, spdlog::level::warn},
        {"error", LogLevel::Error, spdlog::level::err},   {"critical", LogLevel::Critical, spdlog::level::critical},
        {"off", LogLevel::Off, spdlog::level::off},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        EXPECT_EQ(parseLogLevel(testCase.name), testCase.level);
        EXPECT_EQ(logLevelName(testCase.level), testCase.name);
        EXPECT_EQ(toSpdlogLevel(testCase.level), testCase.spdlogLevel);
    }
}

TEST(AppLogging, ParseLogLevel_OtherNames_Nullopt)
{
    // spdlog's own names for some levels, and a name in another case, are not the option's.
    for (const std::string_view name : {"", "Info", "warning", "err", "verbose"}) {
        SCOPED_TRACE(name);
        EXPECT_EQ(parseLogLevel(name), std::nullopt);
    }
}
