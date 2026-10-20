#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

#include <gtest/gtest.h>

#include "domain/settings.h"
#include "persistence/settings_file.h"
#include "support/settings_samples.h"
#include "support/temporary_directory.h"

using namespace sysmon::persistence;
using sysmon::domain::Settings;
using sysmon::tests::nonDefaultSettings;
using sysmon::tests::TemporaryDirectory;

namespace
{

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
    EXPECT_TRUE(file.good()) << "cannot write the test's file";
}

std::ptrdiff_t entryCount(const std::filesystem::path& directory)
{
    std::error_code error;
    const std::filesystem::directory_iterator entries{directory, error};
    EXPECT_FALSE(error) << error.message();
    return std::distance(entries, std::filesystem::directory_iterator{});
}

} // namespace

TEST(SettingsFileIntegration, SaveThenLoad_NewDirectory_ReadsTheSettingsBack)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "System Monitor" / "settings.json";

    const auto saveError = saveSettingsFile(path, nonDefaultSettings());
    const auto loaded = loadSettingsFile(path);

    EXPECT_FALSE(saveError.has_value()) << *saveError;
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->settings, nonDefaultSettings());
    EXPECT_TRUE(loaded->problems.empty());
}

TEST(SettingsFileIntegration, LoadSettingsFile_NoFile_ReturnsNullopt)
{
    const TemporaryDirectory directory;

    EXPECT_FALSE(loadSettingsFile(directory.path() / "settings.json").has_value());
}

TEST(SettingsFileIntegration, SaveSettingsFile_ExistingFile_ReplacedWithoutATemporaryFileLeft)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "settings.json";
    writeFile(path, "not the settings");

    const auto saveError = saveSettingsFile(path, nonDefaultSettings());
    const auto loaded = loadSettingsFile(path);

    EXPECT_FALSE(saveError.has_value()) << *saveError;
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->settings, nonDefaultSettings());
    EXPECT_EQ(entryCount(directory.path()), 1);
}

TEST(SettingsFileIntegration, SaveSettingsFile_TemporaryFileCannotBeCreated_LeavesThePreviousFileWhole)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "settings.json";
    ASSERT_FALSE(saveSettingsFile(path, nonDefaultSettings()).has_value());
    // A directory in place of the temporary file, which settings_file.cpp names after the file.
    std::error_code error;
    std::filesystem::create_directory(directory.path() / "settings.json.tmp", error);
    ASSERT_FALSE(error) << error.message();

    const auto saveError = saveSettingsFile(path, Settings{});
    const auto loaded = loadSettingsFile(path);

    EXPECT_TRUE(saveError.has_value());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->settings, nonDefaultSettings());
}

TEST(SettingsFileIntegration, LoadSettingsFile_Directory_GivesTheDefaultsAndOneProblem)
{
    const TemporaryDirectory directory;

    const auto loaded = loadSettingsFile(directory.path());

    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->settings, Settings{});
    ASSERT_EQ(loaded->problems.size(), 1u);
    EXPECT_TRUE(loaded->problems.front().key.empty());
}

TEST(SettingsFileIntegration, SaveSettingsFile_FileInPlaceOfTheDirectory_ReturnsAnError)
{
    const TemporaryDirectory directory;
    writeFile(directory.path() / "System Monitor", "a file, not a directory");

    const auto saveError = saveSettingsFile(directory.path() / "System Monitor" / "settings.json", Settings{});

    EXPECT_TRUE(saveError.has_value());
}

TEST(SettingsFileIntegration, SaveSettingsFile_DirectoryInPlaceOfTheFile_ReturnsAnErrorAndRemovesTheTemporaryFile)
{
    const TemporaryDirectory directory;
    const auto path = directory.path() / "settings.json";
    std::error_code error;
    std::filesystem::create_directory(path, error);
    ASSERT_FALSE(error) << error.message();

    const auto saveError = saveSettingsFile(path, Settings{});

    EXPECT_TRUE(saveError.has_value());
    EXPECT_EQ(entryCount(directory.path()), 1);
}
