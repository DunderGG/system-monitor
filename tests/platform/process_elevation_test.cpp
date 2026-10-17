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

TEST(ProcessElevation, RunsElevated_ElevatedTokenOrHighIntegrityOrAbove)
{
    struct Case
    {
        const char* name;
        TOKEN_ELEVATION elevation;
        DWORD integrityRid;
        bool expected;
    };
    const std::array cases{
        Case{"elevated at high integrity", kElevated, SECURITY_MANDATORY_HIGH_RID, true},
        Case{"elevated, lowered to medium integrity", kElevated, SECURITY_MANDATORY_MEDIUM_RID, true},
        // As a UIAccess program, such as ctfmon.exe, runs for an administrator.
        Case{"unelevated at high integrity", kNotElevated, SECURITY_MANDATORY_HIGH_RID, true},
        Case{"unelevated at system integrity", kNotElevated, SECURITY_MANDATORY_SYSTEM_RID, true},
        Case{"unelevated at protected process integrity", kNotElevated, SECURITY_MANDATORY_PROTECTED_PROCESS_RID, true},
        Case{"unelevated just below high integrity", kNotElevated, SECURITY_MANDATORY_HIGH_RID - 1, false},
        Case{"unelevated at medium plus integrity", kNotElevated, SECURITY_MANDATORY_MEDIUM_PLUS_RID, false},
        Case{"unelevated at medium integrity", kNotElevated, SECURITY_MANDATORY_MEDIUM_RID, false},
        Case{"unelevated at low integrity", kNotElevated, SECURITY_MANDATORY_LOW_RID, false},
        Case{"unelevated at untrusted integrity", kNotElevated, SECURITY_MANDATORY_UNTRUSTED_RID, false},
    };

    for (const auto& [name, elevation, integrityRid, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(runsElevated(elevation, integrityRid), expected);
    }
}
