#pragma once

#include <windows.h>

namespace verdandi::tests
{

/** Returns true if this process's token is elevated, false if it is not or cannot be read. */
inline bool isCurrentProcessElevated()
{
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    if (!::GetTokenInformation(::GetCurrentProcessToken(), TokenElevation, &elevation, sizeof(elevation), &size)) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

} // namespace verdandi::tests
