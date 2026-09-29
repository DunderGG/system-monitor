#include <gtest/gtest.h>

#include "domain/system_activity_sample.h"
#include "platform/windows/performance_info_reader.h"
#include "platform/windows/system_activity_collector.h"

using namespace sysmon::platform;

TEST(SystemActivityCollector, CalculateSystemActivity_CopiesCounts)
{
    const PerformanceInfoData data{.processCount = 367, .threadCount = 8350, .handleCount = 200'157};

    const auto activity = calculateSystemActivity(data);

    ASSERT_TRUE(activity.has_value());
    EXPECT_EQ(activity->processCount, 367u);
    EXPECT_EQ(activity->threadCount, 8350u);
    EXPECT_EQ(activity->handleCount, 200'157u);
}

TEST(SystemActivityCollector, CalculateSystemActivity_ZeroProcesses_ReturnsNullopt)
{
    EXPECT_FALSE(calculateSystemActivity(PerformanceInfoData{}).has_value());
}

TEST(SystemActivityCollector, Collect_ReaderFails_ReturnsNullopt)
{
    SystemActivityCollector collector([](PerformanceInfoData& /*data*/) { return false; });

    EXPECT_FALSE(collector.collect().has_value());
}

TEST(SystemActivityCollector, Collect_ReaderSucceeds_ReturnsCounts)
{
    SystemActivityCollector collector([](PerformanceInfoData& data) {
        data.processCount = 10;
        data.threadCount = 100;
        data.handleCount = 1000;
        return true;
    });

    const auto activity = collector.collect();

    ASSERT_TRUE(activity.has_value());
    EXPECT_EQ(activity->threadCount, 100u);
}
