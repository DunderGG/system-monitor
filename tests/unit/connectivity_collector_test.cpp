#include <optional>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "platform/windows/connectivity_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

ConnectivityReader fixedReader(ConnectivityLevel level, std::optional<bool> isMetered)
{
    return [level, isMetered] {
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = level,
            .isMetered = isMetered,
        }};
    };
}

} // namespace

TEST(ConnectivityCollector, Collect_InternetAccess_ReturnsInternetAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(status.isMetered, false);
}

TEST(ConnectivityCollector, Collect_MeteredConnection_SetsIsMetered)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, true));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(status.isMetered, true);
}

TEST(ConnectivityCollector, Collect_LocalAccess_ReturnsLocalAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::LocalAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::LocalAccess);
    EXPECT_EQ(status.isMetered, false);
}

TEST(ConnectivityCollector, Collect_CaptivePortal_ReturnsConstrainedInternetAccess)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::ConstrainedInternetAccess, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::ConstrainedInternetAccess);
}

TEST(ConnectivityCollector, Collect_NoConnectivity_ReturnsNone)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::None, false));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::None);
}

TEST(ConnectivityCollector, Collect_UnknownCost_ReturnsNulloptIsMetered)
{
    ConnectivityCollector collector(fixedReader(ConnectivityLevel::InternetAccess, std::nullopt));

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::InternetAccess);
    EXPECT_FALSE(status.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderFails_ReturnsUnknown)
{
    ConnectivityCollector collector([] { return std::optional<ConnectivityStatus>{}; });

    const auto status = collector.collect();

    EXPECT_EQ(status.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(status.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderFailsAfterSuccess_ReturnsUnknownNotStaleStatus)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        if (callCount > 1) {
            return std::optional<ConnectivityStatus>{};
        }
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = ConnectivityLevel::InternetAccess,
            .isMetered = true,
        }};
    });

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(second.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(second.isMetered.has_value());
}

TEST(ConnectivityCollector, Collect_ReaderRecoversAfterFailure_ReturnsFreshStatus)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        if (callCount == 1) {
            return std::optional<ConnectivityStatus>{};
        }
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = ConnectivityLevel::LocalAccess,
            .isMetered = false,
        }};
    });

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first.level, ConnectivityLevel::Unknown);
    EXPECT_EQ(second.level, ConnectivityLevel::LocalAccess);
    EXPECT_EQ(second.isMetered, false);
}

TEST(ConnectivityCollector, Collect_StatusChangesBetweenTicks_ReflectsEachChange)
{
    int callCount = 0;
    ConnectivityCollector collector([&callCount] {
        ++callCount;
        const bool internet = (callCount % 2 == 1);
        return std::optional<ConnectivityStatus>{ConnectivityStatus{
            .level = internet ? ConnectivityLevel::InternetAccess : ConnectivityLevel::None,
            .isMetered = false,
        }};
    });

    const auto s1 = collector.collect();
    const auto s2 = collector.collect();
    const auto s3 = collector.collect();

    EXPECT_EQ(s1.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(s2.level, ConnectivityLevel::None);
    EXPECT_EQ(s3.level, ConnectivityLevel::InternetAccess);
    EXPECT_EQ(callCount, 3);
}
