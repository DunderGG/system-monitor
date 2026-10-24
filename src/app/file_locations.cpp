#include "file_locations.h"

#include <string_view>
#include <system_error>
#include <utility>

namespace verdandi::app
{

namespace
{

constexpr std::wstring_view kSettingsFileName = L"settings.json";
constexpr std::wstring_view kLogFileName = L"verdandi.log";

} // namespace

FileLocations chooseFileLocations(const std::filesystem::path& executableDirectory,
                                  const std::filesystem::path& roamingDirectory,
                                  const std::filesystem::path& localDirectory)
{
    std::filesystem::path portableSettingsPath = executableDirectory / kSettingsFileName;
    std::error_code error;
    if (std::filesystem::exists(portableSettingsPath, error)) {
        return {
            .settingsPath = std::move(portableSettingsPath),
            .logFilePath = executableDirectory / kLogFileName,
            .isPortable = true,
        };
    }
    return {
        .settingsPath = roamingDirectory / kSettingsFileName,
        .logFilePath = localDirectory / kLogFileName,
    };
}

} // namespace verdandi::app
