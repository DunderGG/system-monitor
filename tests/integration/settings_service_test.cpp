#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include <gtest/gtest.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "app/settings_service.h"
#include "domain/settings.h"
#include "monitoring/sampling_scheduler.h"
#include "persistence/settings_file.h"
#include "support/log_capture.h"
#include "support/settings_samples.h"
#include "support/temporary_directory.h"

using sysmon::app::SettingsService;
using sysmon::domain::ByteUnits;
using sysmon::domain::LogLevel;
using sysmon::domain::Settings;
using sysmon::monitoring::SamplingScheduler;
using sysmon::persistence::loadSettingsFile;
using sysmon::tests::nonDefaultSettings;
using sysmon::tests::ScopedLogCapture;
using sysmon::tests::TemporaryDirectory;

TEST(SettingsServiceIntegration, Change_AppliesTheIntervalAndSavesTheSettings)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "Verdandi" / "settings.json";
    const ScopedLogCapture logCapture;
    SamplingScheduler scheduler;
    SettingsService service(path, Settings{}, scheduler);

    const auto error = service.change(nonDefaultSettings());

    EXPECT_EQ(error, std::nullopt);
    EXPECT_EQ(service.path(), path);
    EXPECT_EQ(service.settings(), nonDefaultSettings());
    EXPECT_EQ(scheduler.interval(), nonDefaultSettings().refreshInterval);
    EXPECT_EQ(scheduler.slowInterval(), nonDefaultSettings().refreshInterval);
    const auto loaded = loadSettingsFile(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->settings, nonDefaultSettings());
    EXPECT_TRUE(loaded->problems.empty());
}

TEST(SettingsServiceIntegration, Change_LogLevel_SetsTheLoggersLevelOnlyWhenItChanges)
{
    const TemporaryDirectory directory;
    const ScopedLogCapture logCapture;
    // As --log-level debug would, over the settings' info.
    spdlog::default_logger()->set_level(spdlog::level::debug);
    SamplingScheduler scheduler;
    SettingsService service(directory.path() / "settings.json", Settings{}, scheduler);
    Settings settings;
    settings.displayUnits.bytes = ByteUnits::Decimal;

    EXPECT_EQ(service.change(settings), std::nullopt);
    const auto levelAfterOtherChange = spdlog::default_logger()->level();
    settings.logLevel = LogLevel::Warn;
    EXPECT_EQ(service.change(settings), std::nullopt);

    EXPECT_EQ(levelAfterOtherChange, spdlog::level::debug);
    EXPECT_EQ(spdlog::default_logger()->level(), spdlog::level::warn);
}

TEST(SettingsServiceIntegration, Change_FileCannotBeSaved_AppliesThemLogsAndReturnsWhy)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "settings.json";
    std::error_code error;
    std::filesystem::create_directory(path, error); // A directory in place of the file.
    ASSERT_FALSE(error) << error.message();
    const ScopedLogCapture logCapture;
    SamplingScheduler scheduler;
    SettingsService service(path, Settings{}, scheduler);

    const auto saveError = service.change(nonDefaultSettings());

    ASSERT_TRUE(saveError.has_value());
    EXPECT_FALSE(saveError->empty());
    EXPECT_EQ(service.settings(), nonDefaultSettings());
    EXPECT_EQ(scheduler.interval(), nonDefaultSettings().refreshInterval);
    EXPECT_NE(logCapture.output().find("error Settings not saved to " + path.string() + ": " + *saveError),
              std::string::npos)
        << logCapture.output();
}
