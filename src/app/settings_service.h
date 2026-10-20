#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "domain/settings.h"
#include "monitoring/sampling_scheduler.h"
#include "persistence/settings_file.h"

namespace sysmon::app
{

/**
 * Logs what reading the settings file at path found (DC-127): at info, that
 * there is no file there, so the defaults apply, or that it was read, and at
 * warn each problem in it, which says what was used instead. result is what
 * persistence::loadSettingsFile() returned. Call it once the logger is
 * configured, since the file holds the log level.
 */
void logSettingsRead(const std::filesystem::path& path, const std::optional<persistence::SettingsReadResult>& result);

/**
 * Sets scheduler's intervals from the refresh interval: the fast collectors
 * run every interval, and the slow collectors every
 * SamplingScheduler::kDefaultSlowInterval, or every interval if it is longer,
 * so that they never run more often than snapshots are sent. Thread-safe, as
 * the scheduler's setters are.
 */
void applyRefreshInterval(monitoring::SamplingScheduler& scheduler, std::chrono::milliseconds interval);

/**
 * Keeps the settings for the app once they are read (DC-127). change() takes
 * the settings that MainWindow::settingsChanged() emits, applies those that
 * are not the UI's to apply, and saves them to the settings file.
 *
 * Not thread-safe; use only on the UI thread.
 */
class SettingsService
{
public:
    /**
     * Keeps settings, which the file at path holds, or the defaults while
     * there is no file. Applies nothing: the caller has already set the
     * logger's level and scheduler's intervals. scheduler must outlive this.
     */
    SettingsService(std::filesystem::path path, const domain::Settings& settings,
                    monitoring::SamplingScheduler& scheduler);

    /**
     * Makes settings the current settings and saves them to the file. If they
     * change the refresh interval, it is applied with applyRefreshInterval();
     * if they change the log level, the default logger takes it, so a level
     * given with --log-level holds while other settings change. Returns
     * std::nullopt when the file was saved, and otherwise why it was not,
     * which is also logged as an error. The settings apply either way.
     */
    [[nodiscard]] std::optional<std::string> change(const domain::Settings& settings);

    [[nodiscard]] const domain::Settings& settings() const;
    [[nodiscard]] const std::filesystem::path& path() const;

private:
    std::filesystem::path m_path;
    domain::Settings m_settings;
    monitoring::SamplingScheduler& m_scheduler;
};

} // namespace sysmon::app
