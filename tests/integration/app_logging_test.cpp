#include <filesystem>
#include <string>
#include <system_error>

#include <windows.h>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "app/app_logging.h"

TEST(AppLoggingIntegration, ProcessCodePage_IsUtf8)
{
    // Set by src/app/system_monitor.manifest, which this test binary embeds like the app.
    EXPECT_EQ(::GetACP(), static_cast<UINT>(CP_UTF8));
}

TEST(AppLoggingIntegration, ConfigureLogging_PathOutsideAnsiCodePage_WritesLogFile)
{
    // U+65E5 U+5FD7 (CJK) and U+0416 (Cyrillic) cannot be represented in Western code
    // pages such as 1252. Without a UTF-8 process code page, path::string() inside
    // configureLogging throws. Written as code units so the source stays ASCII.
    std::wstring directoryName = L"sysmon_log_";
    directoryName += static_cast<wchar_t>(0x65E5);
    directoryName += static_cast<wchar_t>(0x5FD7);
    directoryName += static_cast<wchar_t>(0x0416);
    std::error_code error;
    const auto directory = std::filesystem::temp_directory_path(error) / directoryName;
    ASSERT_FALSE(error) << error.message();
    std::filesystem::create_directories(directory, error);
    ASSERT_FALSE(error) << error.message();
    const auto logFilePath = directory / "system_monitor.log";
    const auto previousLogger = spdlog::default_logger();

    sysmon::app::configureLogging(logFilePath, spdlog::level::info);
    spdlog::warn("Log file path test"); // The logger flushes on warn.
    spdlog::set_default_logger(previousLogger);
    // set_default_logger() keeps the replaced logger registered; dropping it closes the file.
    spdlog::drop(std::string{sysmon::app::kApplicationLoggerName});
    const bool isLogFileWritten = std::filesystem::exists(logFilePath, error);
    const auto removedCount = std::filesystem::remove_all(directory, error);

    EXPECT_TRUE(isLogFileWritten);
    EXPECT_FALSE(error) << "Log directory not removed (file still open?): " << error.message();
    EXPECT_GT(removedCount, 0u);
}
