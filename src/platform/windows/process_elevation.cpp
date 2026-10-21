#include "platform/windows/process_elevation.h"

namespace verdandi::platform
{

bool runsElevated(const TOKEN_ELEVATION& elevation, DWORD integrityRid) noexcept
{
    return elevation.TokenIsElevated != 0 || integrityRid >= SECURITY_MANDATORY_HIGH_RID;
}

} // namespace verdandi::platform
