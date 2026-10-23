#include <vector>

#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "app/app_logging.h"
#include "domain/settings.h"

using verdandi::app::toSpdlogLevel;
using verdandi::domain::LogLevel;
using verdandi::domain::logLevelName;

TEST(AppLogging, ToSpdlogLevel_EachLevel)
{
    struct Case
    {
        LogLevel level;
        spdlog::level::level_enum spdlogLevel;
    };
    const std::vector<Case> cases{
        {LogLevel::Trace, spdlog::level::trace}, {LogLevel::Debug, spdlog::level::debug},
        {LogLevel::Info, spdlog::level::info},   {LogLevel::Warn, spdlog::level::warn},
        {LogLevel::Error, spdlog::level::err},   {LogLevel::Critical, spdlog::level::critical},
        {LogLevel::Off, spdlog::level::off},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(logLevelName(testCase.level));
        EXPECT_EQ(toSpdlogLevel(testCase.level), testCase.spdlogLevel);
    }
}
