#include <optional>

#include <gtest/gtest.h>

#include "domain/network_profile.h"
#include "platform/windows/network_category_mapping.h"

using namespace verdandi::domain;
using namespace verdandi::platform;

TEST(NetworkCategoryMapping, ToNetworkCategory_EveryCategory_Maps)
{
    EXPECT_EQ(toNetworkCategory(NLM_NETWORK_CATEGORY_PUBLIC), std::optional{NetworkCategory::Public});
    EXPECT_EQ(toNetworkCategory(NLM_NETWORK_CATEGORY_PRIVATE), std::optional{NetworkCategory::Private});
    EXPECT_EQ(toNetworkCategory(NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED),
              std::optional{NetworkCategory::DomainAuthenticated});
}

TEST(NetworkCategoryMapping, ToNetworkCategory_UnknownValue_ReturnsNullopt)
{
    EXPECT_FALSE(toNetworkCategory(static_cast<NLM_NETWORK_CATEGORY>(42)).has_value());
}
