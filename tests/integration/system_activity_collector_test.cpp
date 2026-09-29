#include <gtest/gtest.h>

#include "domain/system_activity_sample.h"
#include "platform/windows/system_activity_collector.h"

using namespace sysmon::platform;

TEST(SystemActivityCollectorIntegration, RealHost_CountsArePlausible)
{
    SystemActivityCollector collector;

    const auto activity = collector.collect();

    ASSERT_TRUE(activity.has_value());
    // This test process alone has a thread and several handles, and the
    // System and Idle processes always exist.
    EXPECT_GE(activity->processCount, 2u);
    EXPECT_GE(activity->threadCount, activity->processCount);
    EXPECT_GE(activity->handleCount, activity->processCount);
}
