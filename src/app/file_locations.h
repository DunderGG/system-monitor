#pragma once

#include <filesystem>

namespace verdandi::app
{

/** Where the monitor keeps its settings file and its log (DC-131). */
struct FileLocations
{
    std::filesystem::path settingsPath{};
    std::filesystem::path logFilePath{};
    /** Whether both are in the executable's directory: the monitor is portable. */
    bool isPortable = false;
};

/**
 * Chooses where the monitor keeps its settings file and its log (DC-131).
 * When a settings.json lies in executableDirectory, as in the portable zip,
 * both are kept there: the settings file and verdandi.log. Otherwise
 * settings.json is in roamingDirectory, which roams with the user's profile,
 * and verdandi.log in localDirectory, which stays on this machine. The file's
 * presence decides, not whether it can be read: one that cannot gives the
 * defaults, and the first save replaces it. A directory whose entries cannot
 * be read counts as one without the file.
 */
[[nodiscard]] FileLocations chooseFileLocations(const std::filesystem::path& executableDirectory,
                                                const std::filesystem::path& roamingDirectory,
                                                const std::filesystem::path& localDirectory);

} // namespace verdandi::app
