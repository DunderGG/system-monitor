#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/uptime_collector.h"

using namespace sysmon::domain;
using namespace sysmon::monitoring;
using namespace sysmon::platform;

namespace
{

// GetTickCount64 advances in steps of the system timer interval (up to ~16 ms).
constexpr std::chrono::milliseconds kTickResolutionTolerance{32};

} // namespace

TEST(UptimeCollectorIntegration, RealHostSampling_ReturnsPositiveUptime)
{
    UptimeCollector collector;

    const auto uptime = collector.collect();

    EXPECT_GT(uptime, std::chrono::milliseconds::zero());
}

TEST(UptimeCollectorIntegration, ConsecutiveSamples_AdvanceByElapsedTime)
{
    UptimeCollector collector;

    const auto first = collector.collect();
    const auto wallStart = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    const auto second = collector.collect();
    const auto wallElapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wallStart);

    const auto uptimeElapsed = second - first;
    EXPECT_GE(uptimeElapsed, std::chrono::milliseconds{100} - kTickResolutionTolerance);
    EXPECT_LE(uptimeElapsed, wallElapsed + kTickResolutionTolerance);
}

TEST(UptimeCollectorIntegration, SchedulerSampleOnce_PopulatesSnapshotUptime)
{
    SamplingScheduler scheduler;
    scheduler.setUptimeCollector(std::make_unique<UptimeCollector>());

    const SystemSnapshot snapshot = scheduler.sampleOnce();

    ASSERT_TRUE(snapshot.uptime.has_value());
    EXPECT_GT(*snapshot.uptime, std::chrono::milliseconds::zero());
}
