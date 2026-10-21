#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/settings.h"

namespace verdandi::persistence
{

/** The format version that serializeSettings() writes as the file's "version". */
inline constexpr int kSettingsVersion = 1;

/**
 * A value in a settings file that was not used as it stood, and what was used
 * instead. key is the setting's path in the file, such as "refreshIntervalMs"
 * or "dashboardPanels.cpu", and is empty when the problem is with the whole
 * file. description is a sentence for the log, which names the key.
 */
struct SettingsProblem
{
    std::string key{};
    std::string description{};
};

/** Settings read from a settings file, and the problems found in it. */
struct SettingsReadResult
{
    domain::Settings settings{};
    std::vector<SettingsProblem> problems{};
};

/** Returns the JSON text of a settings file that holds settings, ending with a newline. */
[[nodiscard]] std::string serializeSettings(const domain::Settings& settings);

/**
 * Reads settings from the JSON text of a settings file, one setting at a time
 * (DC-123). A setting that is missing keeps its default. A whole number of
 * milliseconds that is not a refresh interval a user can choose takes the
 * nearest that is (domain::nearestRefreshInterval()), and any other value that
 * is not valid keeps its default. A key that is not a setting is ignored. Text that
 * is not a JSON object gives the defaults. Every value not used as it stood,
 * and every key ignored, is reported as a problem.
 */
[[nodiscard]] SettingsReadResult parseSettings(std::string_view text);

/**
 * Reads the settings file at path. Returns std::nullopt when there is no file
 * there, as before the first save, so the caller uses the defaults. A file
 * that cannot be opened gives the defaults and one problem.
 */
[[nodiscard]] std::optional<SettingsReadResult> loadSettingsFile(const std::filesystem::path& path);

/**
 * Writes settings to the file at path, creating its directory. The text goes
 * to a temporary file beside it first, which then replaces the file, so a save
 * that fails leaves the previous file whole. Returns std::nullopt when the
 * file was written, and otherwise why it was not.
 */
[[nodiscard]] std::optional<std::string> saveSettingsFile(const std::filesystem::path& path,
                                                          const domain::Settings& settings);

} // namespace verdandi::persistence
