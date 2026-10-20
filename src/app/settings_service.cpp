#include "settings_service.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include "app_logging.h"

namespace sysmon::app
{

void logSettingsRead(const std::filesystem::path& path, const std::optional<persistence::SettingsReadResult>& result)
{
    if (!result) {
        spdlog::info("No settings file at {}; using the default settings", path.string());
        return;
    }
    spdlog::info("Settings read from {}", path.string());
    for (const auto& problem : result->problems) {
        spdlog::warn("Settings file: {}", problem.description);
    }
}

void applyRefreshInterval(monitoring::SamplingScheduler& scheduler, std::chrono::milliseconds interval)
{
    scheduler.setInterval(interval);
    scheduler.setSlowInterval(std::max(monitoring::SamplingScheduler::kDefaultSlowInterval, interval));
}

SettingsService::SettingsService(std::filesystem::path path, const domain::Settings& settings,
                                 monitoring::SamplingScheduler& scheduler)
    : m_path(std::move(path)), m_settings(settings), m_scheduler(scheduler)
{}

std::optional<std::string> SettingsService::change(const domain::Settings& settings)
{
    if (settings.refreshInterval != m_settings.refreshInterval) {
        applyRefreshInterval(m_scheduler, settings.refreshInterval);
        spdlog::info("Refresh interval set to {} ms", settings.refreshInterval.count());
    }
    if (settings.logLevel != m_settings.logLevel) {
        spdlog::default_logger()->set_level(toSpdlogLevel(settings.logLevel));
        spdlog::info("Log level set to '{}'", logLevelName(settings.logLevel));
    }
    m_settings = settings;

    auto error = persistence::saveSettingsFile(m_path, m_settings);
    if (error) {
        spdlog::error("Settings not saved to {}: {}", m_path.string(), *error);
    } else {
        spdlog::info("Settings saved to {}", m_path.string());
    }
    return error;
}

const domain::Settings& SettingsService::settings() const
{
    return m_settings;
}

const std::filesystem::path& SettingsService::path() const
{
    return m_path;
}

} // namespace sysmon::app
