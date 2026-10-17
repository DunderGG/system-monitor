#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "platform/windows/connectivity_hint_mapping.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

// A cost value this mapping does not know, such as one a later SDK adds.
const auto kCostFromALaterSdk = static_cast<NL_NETWORK_CONNECTIVITY_COST_HINT>(NetworkConnectivityCostHintVariable + 1);

} // namespace

TEST(ConnectivityHintMapping, ToConnectivityLevel_MapsEveryLevelHiddenAsUnknown)
{
    struct Case
    {
        const char* name;
        NL_NETWORK_CONNECTIVITY_LEVEL_HINT hint;
        ConnectivityLevel expected;
    };
    const std::array cases{
        Case{"None", NetworkConnectivityLevelHintNone, ConnectivityLevel::None},
        Case{"LocalAccess", NetworkConnectivityLevelHintLocalAccess, ConnectivityLevel::LocalAccess},
        Case{"InternetAccess", NetworkConnectivityLevelHintInternetAccess, ConnectivityLevel::InternetAccess},
        Case{"ConstrainedInternetAccess", NetworkConnectivityLevelHintConstrainedInternetAccess,
             ConnectivityLevel::ConstrainedInternetAccess},
        Case{"Unknown", NetworkConnectivityLevelHintUnknown, ConnectivityLevel::Unknown},
        Case{"Hidden", NetworkConnectivityLevelHintHidden, ConnectivityLevel::Unknown},
    };

    for (const auto& [name, hint, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(toConnectivityLevel(hint), expected);
    }
}

TEST(ConnectivityHintMapping, ToIsMetered_FixedAndVariableMeteredUnknownCostNullopt)
{
    struct Case
    {
        const char* name;
        NL_NETWORK_CONNECTIVITY_COST_HINT cost;
        std::optional<bool> expected;
    };
    const std::array cases{
        Case{"Unrestricted", NetworkConnectivityCostHintUnrestricted, false},
        Case{"Fixed", NetworkConnectivityCostHintFixed, true},
        Case{"Variable", NetworkConnectivityCostHintVariable, true},
        Case{"Unknown", NetworkConnectivityCostHintUnknown, std::nullopt},
        Case{"from a later SDK", kCostFromALaterSdk, std::nullopt},
    };

    for (const auto& [name, cost, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(toIsMetered(cost), expected);
    }
}

TEST(ConnectivityHintMapping, ToIsRoaming_FlagWhileTheCostIsKnownOtherwiseNullopt)
{
    struct Case
    {
        const char* name;
        NL_NETWORK_CONNECTIVITY_COST_HINT cost;
        BOOLEAN roaming;
        std::optional<bool> expected;
    };
    const std::array cases{
        Case{"Variable, roaming", NetworkConnectivityCostHintVariable, TRUE, true},
        Case{"Unrestricted, not roaming", NetworkConnectivityCostHintUnrestricted, FALSE, false},
        Case{"Unknown, roaming", NetworkConnectivityCostHintUnknown, TRUE, std::nullopt},
        Case{"Unknown, not roaming", NetworkConnectivityCostHintUnknown, FALSE, std::nullopt},
        Case{"from a later SDK, roaming", kCostFromALaterSdk, TRUE, std::nullopt},
        Case{"from a later SDK, not roaming", kCostFromALaterSdk, FALSE, std::nullopt},
    };

    for (const auto& [name, cost, roaming, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(toIsRoaming(cost, roaming), expected);
    }
}

TEST(ConnectivityHintMapping, ToConnectivityStatus_FullHint_MapsEveryField)
{
    const NL_NETWORK_CONNECTIVITY_HINT hint{
        .ConnectivityLevel = NetworkConnectivityLevelHintConstrainedInternetAccess,
        .ConnectivityCost = NetworkConnectivityCostHintFixed,
        .Roaming = TRUE,
    };

    const auto status = toConnectivityStatus(hint);

    EXPECT_EQ(status.level, ConnectivityLevel::ConstrainedInternetAccess);
    EXPECT_EQ(status.isMetered, std::optional<bool>{true});
    EXPECT_EQ(status.isRoaming, std::optional<bool>{true});
}
