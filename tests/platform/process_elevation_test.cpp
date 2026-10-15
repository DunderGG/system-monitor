#include <array>

#include <windows.h>

#include <gtest/gtest.h>

#include "platform/windows/process_elevation.h"

using namespace sysmon::platform;

namespace
{

constexpr TOKEN_ELEVATION kElevated{.TokenIsElevated = 1};
constexpr TOKEN_ELEVATION kNotElevated{.TokenIsElevated = 0};

} // namespace

TEST(ProcessElevation, RunsElevated_ElevatedTokenAtHighIntegrity_True)
{
    EXPECT_TRUE(runsElevated(kElevated, SECURITY_MANDATORY_HIGH_RID));
}

TEST(ProcessElevation, RunsElevated_ElevatedTokenLoweredToMediumIntegrity_True)
{
    EXPECT_TRUE(runsElevated(kElevated, SECURITY_MANDATORY_MEDIUM_RID));
}

TEST(ProcessElevation, RunsElevated_UnelevatedTokenAtHighIntegrity_True)
{
    // As a UIAccess program, such as ctfmon.exe, runs for an administrator.
    EXPECT_TRUE(runsElevated(kNotElevated, SECURITY_MANDATORY_HIGH_RID));
}

TEST(ProcessElevation, RunsElevated_UnelevatedTokenAtSystemOrProtectedIntegrity_True)
{
    EXPECT_TRUE(runsElevated(kNotElevated, SECURITY_MANDATORY_SYSTEM_RID));
    EXPECT_TRUE(runsElevated(kNotElevated, SECURITY_MANDATORY_PROTECTED_PROCESS_RID));
}

TEST(ProcessElevation, RunsElevated_UnelevatedTokenBelowHighIntegrity_False)
{
    const std::array<DWORD, 5> rids{
        SECURITY_MANDATORY_UNTRUSTED_RID,   SECURITY_MANDATORY_LOW_RID,      SECURITY_MANDATORY_MEDIUM_RID,
        SECURITY_MANDATORY_MEDIUM_PLUS_RID, SECURITY_MANDATORY_HIGH_RID - 1,
    };

    for (const auto rid : rids) {
        SCOPED_TRACE(rid);
        EXPECT_FALSE(runsElevated(kNotElevated, rid));
    }
}
