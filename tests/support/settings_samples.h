#pragma once

#include <chrono>
#include <ostream>

#include "domain/settings.h"
#include "persistence/settings_file.h"

namespace sysmon::domain
{

/** Prints settings in a failed expectation as a settings file's JSON. GoogleTest finds it by argument lookup. */
inline void PrintTo(const Settings& settings, std::ostream* stream)
{
    *stream << persistence::serializeSettings(settings);
}

} // namespace sysmon::domain

namespace sysmon::tests
{

/** Settings in which every value differs from its default. */
[[nodiscard]] inline domain::Settings nonDefaultSettings()
{
    return domain::Settings{
        .refreshInterval = std::chrono::milliseconds{2500},
        .displayUnits = {.bytes = domain::ByteUnits::Decimal, .networkRates = domain::NetworkRateUnits::BitsPerSecond},
        .dashboardPanels = {.cpu = false, .memory = false, .disk = false, .network = false, .uptime = false},
        .logLevel = domain::LogLevel::Debug,
    };
}

} // namespace sysmon::tests
