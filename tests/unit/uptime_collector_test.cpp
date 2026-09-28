#include <chrono>
#include <cstdint>

#include <gtest/gtest.h>

#include "platform/windows/uptime_collector.h"

using namespace sysmon::platform;

TEST(UptimeCollector, Collect_ReaderValue_ReturnsSameMilliseconds)
{
    UptimeCollector collector([] { return uint64_t{123'456}; });

    const auto uptime = collector.collect();

    EXPECT_EQ(uptime, std::chrono::milliseconds{123'456});
}

TEST(UptimeCollector, Collect_BeyondGetTickCountWrap_PreservesFull64BitValue)
{
    // 60 days exceeds the 2^32 ms (~49.7 days) wrap of the 32-bit GetTickCount.
    constexpr uint64_t kSixtyDaysMs = 60ULL * 24 * 60 * 60 * 1000;
    UptimeCollector collector([] { return kSixtyDaysMs; });

    const auto uptime = collector.collect();

    EXPECT_EQ(std::chrono::duration_cast<std::chrono::hours>(uptime), std::chrono::hours{60 * 24});
}

TEST(UptimeCollector, Collect_CalledTwice_ReadsFreshValueEachTime)
{
    uint64_t ticks = 1'000;
    UptimeCollector collector([&ticks] { return ticks; });

    const auto first = collector.collect();
    ticks = 2'500;
    const auto second = collector.collect();

    EXPECT_EQ(first, std::chrono::milliseconds{1'000});
    EXPECT_EQ(second, std::chrono::milliseconds{2'500});
}
