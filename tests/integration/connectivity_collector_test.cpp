#include <chrono>
#include <memory>
#include <thread>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "platform/windows/connectivity_collector.h"
#include "platform/windows/connectivity_hint_mapping.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

bool isValidLevel(ConnectivityLevel level)
{
    return level == ConnectivityLevel::Unknown || level == ConnectivityLevel::None ||
           level == ConnectivityLevel::LocalAccess || level == ConnectivityLevel::ConstrainedInternetAccess ||
           level == ConnectivityLevel::InternetAccess;
}

} // namespace

// These tests tolerate any host state: the host may be online, offline,
// metered, behind a captive portal, or have its connectivity change mid-run.

TEST(ConnectivityCollectorIntegration, FirstCollect_MatchesDirectConnectivityHint)
{
    NL_NETWORK_CONNECTIVITY_HINT hint{};
    if (GetNetworkConnectivityHint(&hint) != NO_ERROR) {
        GTEST_SKIP() << "GetNetworkConnectivityHint unavailable on this host";
    }
    const auto expected = toConnectivityStatus(hint);

    ConnectivityCollector collector;
    const auto status = collector.collect();

    // The cache is seeded synchronously, so no wait is needed. This can only
    // differ if connectivity genuinely changes between the two reads.
    EXPECT_EQ(status.level, expected.level);
    EXPECT_EQ(status.isMetered, expected.isMetered);
    EXPECT_EQ(status.isRoaming, expected.isRoaming);
}

TEST(ConnectivityCollectorIntegration, ConsecutiveSamples_ReturnValidLevels)
{
    ConnectivityCollector collector;

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(isValidLevel(collector.collect().level));
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

TEST(ConnectivityCollectorIntegration, RepeatedConstructionAndDestruction_CancelsNotificationsCleanly)
{
    // Exercises CancelMibChangeNotify2 while the initial notification may still
    // be in flight on the system thread pool.
    for (int i = 0; i < 20; ++i) {
        auto collector = std::make_unique<ConnectivityCollector>();
        EXPECT_TRUE(isValidLevel(collector->collect().level));
    }
}
