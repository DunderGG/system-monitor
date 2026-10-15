#pragma once

#include <windows.h>

namespace sysmon::platform
{

// Internal to platform/windows: exposed in a header only so the rule can be
// unit tested without calling the OS, by tests/platform/ only.
// Do not include from other modules or from tests/unit/.

/**
 * Returns true if a process whose token has this elevation and integrity
 * level runs elevated, in the sense of the rule that the monitor ends only
 * unelevated processes: its token is elevated (TokenElevation), as for a
 * program run as administrator, or its integrity level, the last
 * subauthority of TokenIntegrityLevel's SID, is SECURITY_MANDATORY_HIGH_RID
 * or above, as for a UIAccess program, a service, or another system process.
 */
[[nodiscard]] bool runsElevated(const TOKEN_ELEVATION& elevation, DWORD integrityRid) noexcept;

} // namespace sysmon::platform
