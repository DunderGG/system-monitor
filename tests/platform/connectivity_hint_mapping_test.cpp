#include <optional>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "platform/windows/connectivity_hint_mapping.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(ConnectivityHintMapping, ToConnectivityLevel_None_ReturnsNone)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintNone), ConnectivityLevel::None);
}

TEST(ConnectivityHintMapping, ToConnectivityLevel_LocalAccess_ReturnsLocalAccess)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintLocalAccess), ConnectivityLevel::LocalAccess);
}

TEST(ConnectivityHintMapping, ToConnectivityLevel_InternetAccess_ReturnsInternetAccess)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintInternetAccess), ConnectivityLevel::InternetAccess);
}

TEST(ConnectivityHintMapping, ToConnectivityLevel_ConstrainedInternetAccess_ReturnsConstrainedInternetAccess)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintConstrainedInternetAccess),
              ConnectivityLevel::ConstrainedInternetAccess);
}

TEST(ConnectivityHintMapping, ToConnectivityLevel_Unknown_ReturnsUnknown)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintUnknown), ConnectivityLevel::Unknown);
}

TEST(ConnectivityHintMapping, ToConnectivityLevel_Hidden_ReturnsUnknown)
{
    EXPECT_EQ(toConnectivityLevel(NetworkConnectivityLevelHintHidden), ConnectivityLevel::Unknown);
}

TEST(ConnectivityHintMapping, ToIsMetered_Unrestricted_ReturnsFalse)
{
    EXPECT_EQ(toIsMetered(NetworkConnectivityCostHintUnrestricted), std::optional<bool>{false});
}

TEST(ConnectivityHintMapping, ToIsMetered_Fixed_ReturnsTrue)
{
    EXPECT_EQ(toIsMetered(NetworkConnectivityCostHintFixed), std::optional<bool>{true});
}

TEST(ConnectivityHintMapping, ToIsMetered_Variable_ReturnsTrue)
{
    EXPECT_EQ(toIsMetered(NetworkConnectivityCostHintVariable), std::optional<bool>{true});
}

TEST(ConnectivityHintMapping, ToIsMetered_Unknown_ReturnsNullopt)
{
    EXPECT_FALSE(toIsMetered(NetworkConnectivityCostHintUnknown).has_value());
}

TEST(ConnectivityHintMapping, ToIsRoaming_KnownCost_ReturnsFlag)
{
    EXPECT_EQ(toIsRoaming(NetworkConnectivityCostHintVariable, TRUE), std::optional<bool>{true});
    EXPECT_EQ(toIsRoaming(NetworkConnectivityCostHintUnrestricted, FALSE), std::optional<bool>{false});
}

TEST(ConnectivityHintMapping, ToIsRoaming_UnknownCost_ReturnsNullopt)
{
    EXPECT_FALSE(toIsRoaming(NetworkConnectivityCostHintUnknown, FALSE).has_value());
    EXPECT_FALSE(toIsRoaming(NetworkConnectivityCostHintUnknown, TRUE).has_value());
}

TEST(ConnectivityHintMapping, ToIsRoaming_CostFromALaterSdk_NulloptLikeTheCost)
{
    // A cost value this mapping does not know, such as one a later SDK adds.
    const auto laterCost = static_cast<NL_NETWORK_CONNECTIVITY_COST_HINT>(NetworkConnectivityCostHintVariable + 1);

    EXPECT_FALSE(toIsMetered(laterCost).has_value());
    EXPECT_FALSE(toIsRoaming(laterCost, TRUE).has_value());
    EXPECT_FALSE(toIsRoaming(laterCost, FALSE).has_value());
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
