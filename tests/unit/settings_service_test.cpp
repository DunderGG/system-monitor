#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/settings_service.h"
#include "domain/settings.h"
#include "monitoring/sampling_scheduler.h"
#include "persistence/settings_file.h"
#include "support/log_capture.h"

using namespace std::chrono_literals;
using sysmon::app::applyRefreshInterval;
using sysmon::app::logSettingsRead;
using sysmon::monitoring::SamplingScheduler;
using sysmon::persistence::SettingsProblem;
using sysmon::persistence::SettingsReadResult;
using sysmon::tests::ScopedLogCapture;

TEST(SettingsService, ApplyRefreshInterval_SlowCollectorsNeverMoreOftenThanTheTickOrEveryTwoSeconds)
{
    struct Case
    {
        const char* name;
        std::chrono::milliseconds interval;
        std::chrono::milliseconds expectedSlowInterval;
    };
    const std::vector<Case> cases{
        {"the shortest", 500ms, SamplingScheduler::kDefaultSlowInterval},
        {"the default", 1000ms, SamplingScheduler::kDefaultSlowInterval},
        {"the slow collectors' own", SamplingScheduler::kDefaultSlowInterval, SamplingScheduler::kDefaultSlowInterval},
        {"longer than theirs", 2500ms, 2500ms},
        {"the longest", 10'000ms, 10'000ms},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        SamplingScheduler scheduler;

        applyRefreshInterval(scheduler, testCase.interval);

        EXPECT_EQ(scheduler.interval(), testCase.interval);
        EXPECT_EQ(scheduler.slowInterval(), testCase.expectedSlowInterval);
    }
}

TEST(SettingsService, LogSettingsRead_NoFile_SaysTheDefaultsApply)
{
    const ScopedLogCapture logCapture;

    logSettingsRead("C:\\Settings\\settings.json", std::nullopt);

    EXPECT_EQ(logCapture.output(),
              "info No settings file at C:\\Settings\\settings.json; using the default settings\n");
}

TEST(SettingsService, LogSettingsRead_FileRead_SaysSoAndWarnsOfEachProblem)
{
    const ScopedLogCapture logCapture;
    const SettingsReadResult result{
        .problems = {SettingsProblem{.key = "a", .description = "a is not a setting; ignoring it"},
                     SettingsProblem{.key = "b", .description = "b is not a setting; ignoring it"}},
    };

    logSettingsRead("C:\\Settings\\settings.json", result);

    EXPECT_EQ(logCapture.output(), "info Settings read from C:\\Settings\\settings.json\n"
                                   "warning Settings file: a is not a setting; ignoring it\n"
                                   "warning Settings file: b is not a setting; ignoring it\n");
}
