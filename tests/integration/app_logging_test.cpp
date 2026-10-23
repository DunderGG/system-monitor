#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include <windows.h>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "app/app_logging.h"
#include "support/temporary_directory.h"

using verdandi::app::configureLogging;
using verdandi::app::kApplicationLoggerName;
using verdandi::tests::TemporaryDirectory;

namespace
{

// Restores the default logger that configureLogging() replaces, and closes
// the log file, on destruction.
class ScopedDefaultLogger
{
public:
    ScopedDefaultLogger() : m_previousLogger(spdlog::default_logger()) {}

    ~ScopedDefaultLogger()
    {
        spdlog::set_default_logger(m_previousLogger);
        // set_default_logger() keeps the replaced logger registered; dropping it closes the file.
        spdlog::drop(std::string{kApplicationLoggerName});
    }

    ScopedDefaultLogger(const ScopedDefaultLogger&) = delete;
    ScopedDefaultLogger& operator=(const ScopedDefaultLogger&) = delete;
    ScopedDefaultLogger(ScopedDefaultLogger&&) = delete;
    ScopedDefaultLogger& operator=(ScopedDefaultLogger&&) = delete;

private:
    std::shared_ptr<spdlog::logger> m_previousLogger;
};

} // namespace

TEST(AppLoggingIntegration, ProcessCodePage_IsUtf8)
{
    // Set by src/app/verdandi.manifest, which this test binary embeds like the app.
    EXPECT_EQ(::GetACP(), static_cast<UINT>(CP_UTF8));
}

TEST(AppLoggingIntegration, ConfigureLogging_PathOutsideAnsiCodePage_WritesLogFile)
{
    // U+65E5 U+5FD7 (CJK) and U+0416 (Cyrillic) cannot be represented in Western code
    // pages such as 1252. Without a UTF-8 process code page, path::string() inside
    // configureLogging throws. Written as code units so the source stays ASCII.
    std::wstring directoryName = L"verdandi_log_";
    directoryName += static_cast<wchar_t>(0x65E5);
    directoryName += static_cast<wchar_t>(0x5FD7);
    directoryName += static_cast<wchar_t>(0x0416);
    std::error_code error;
    const auto directory = std::filesystem::temp_directory_path(error) / directoryName;
    ASSERT_FALSE(error) << error.message();
    std::filesystem::remove_all(directory, error); // Left by a run that crashed.
    const auto logFilePath = directory / "verdandi.log";

    std::optional<std::string> problem;
    {
        const ScopedDefaultLogger restorer;
        problem = configureLogging(logFilePath, spdlog::level::info);
        spdlog::warn("Log file path test"); // The logger flushes on warn.
    }
    const bool isLogFileWritten = std::filesystem::file_size(logFilePath, error) > 0 && !error;
    const auto removedCount = std::filesystem::remove_all(directory, error);

    // configureLogging() creates the directory.
    EXPECT_EQ(problem, std::nullopt);
    EXPECT_TRUE(isLogFileWritten);
    EXPECT_FALSE(error) << "Log directory not removed (file still open?): " << error.message();
    EXPECT_GT(removedCount, 0u);
}

TEST(AppLoggingIntegration, ConfigureLogging_ReadOnlyLogFile_ReportsItAndLogsWithoutTheFile)
{
    // spdlog's file sink throws when it cannot open its file, which ended the
    // monitor at startup (code review F-47).
    const TemporaryDirectory directory;
    const auto logFilePath = directory.path() / "verdandi.log";
    std::ofstream{logFilePath}.close();
    std::error_code error;
    // MSVC makes a file read-only when every write permission is removed.
    using std::filesystem::perms;
    std::filesystem::permissions(logFilePath, perms::owner_write | perms::group_write | perms::others_write,
                                 std::filesystem::perm_options::remove, error);
    ASSERT_FALSE(error) << error.message();

    std::optional<std::string> problem;
    std::shared_ptr<spdlog::logger> logger;
    {
        const ScopedDefaultLogger restorer;
        problem = configureLogging(logFilePath, spdlog::level::info);
        logger = spdlog::default_logger();
        spdlog::warn("Logged without the file");
    }
    const auto logFileSize = std::filesystem::file_size(logFilePath, error);
    // Writable again, so that the directory can be removed.
    std::filesystem::permissions(logFilePath, perms::owner_write, std::filesystem::perm_options::add, error);

    EXPECT_EQ(problem, std::optional<std::string>{"Cannot open the file for writing"});
    // The default logger is the application's still, with standard output alone.
    EXPECT_EQ(logger->name(), kApplicationLoggerName);
    EXPECT_EQ(logger->level(), spdlog::level::info);
    EXPECT_EQ(logger->sinks().size(), 1u);
    EXPECT_EQ(logFileSize, 0u);
}

TEST(AppLoggingIntegration, ConfigureLogging_DirectoryCannotBeCreated_ReportsWhy)
{
    // The log's directory would be inside a file.
    const TemporaryDirectory directory;
    const auto file = directory.path() / "file";
    std::ofstream{file}.close();

    std::optional<std::string> problem;
    {
        const ScopedDefaultLogger restorer;
        problem = configureLogging(file / "verdandi.log", spdlog::level::info);
        spdlog::warn("Logged without the file");
    }

    ASSERT_TRUE(problem.has_value());
    EXPECT_TRUE(problem->starts_with("Cannot create the file's directory: ")) << *problem;
}
