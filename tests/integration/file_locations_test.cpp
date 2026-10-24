#include <filesystem>
#include <fstream>
#include <system_error>

#include <gtest/gtest.h>

#include "app/file_locations.h"
#include "support/temporary_directory.h"

using namespace verdandi::app;
using verdandi::tests::TemporaryDirectory;

namespace
{

// Creates an empty file at path, and the directories above it.
void createEmptyFile(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    EXPECT_FALSE(error) << error.message();
    const std::ofstream file(path, std::ios::binary);
    EXPECT_TRUE(file.good()) << "cannot create the test's file";
}

} // namespace

TEST(FileLocationsIntegration, ChooseFileLocations_SettingsFileBesideTheExecutable_KeepsBothBesideIt)
{
    const TemporaryDirectory directory;
    const auto executableDirectory = directory.path() / "Verdandi-0.1.0-x64";
    const auto roamingDirectory = directory.path() / "Roaming";
    // Empty, so not a settings file that could be read, and one in the
    // user's folder too, which the one beside the executable wins over.
    createEmptyFile(executableDirectory / "settings.json");
    createEmptyFile(roamingDirectory / "settings.json");

    const FileLocations locations =
        chooseFileLocations(executableDirectory, roamingDirectory, directory.path() / "Local");

    EXPECT_EQ(locations.settingsPath, executableDirectory / "settings.json");
    EXPECT_EQ(locations.logFilePath, executableDirectory / "verdandi.log");
    EXPECT_TRUE(locations.isPortable);
}

TEST(FileLocationsIntegration, ChooseFileLocations_NoSettingsFileBesideTheExecutable_UsesTheUsersFolders)
{
    const TemporaryDirectory directory;
    const auto executableDirectory = directory.path() / "Verdandi-0.1.0-x64";
    const auto roamingDirectory = directory.path() / "Roaming";
    const auto localDirectory = directory.path() / "Local";
    createEmptyFile(executableDirectory / "verdandi.exe");

    const FileLocations locations = chooseFileLocations(executableDirectory, roamingDirectory, localDirectory);

    EXPECT_EQ(locations.settingsPath, roamingDirectory / "settings.json");
    EXPECT_EQ(locations.logFilePath, localDirectory / "verdandi.log");
    EXPECT_FALSE(locations.isPortable);
}
