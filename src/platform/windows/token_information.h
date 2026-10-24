#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <windows.h>

namespace verdandi::platform
{

// Internal header: it exposes Windows types to the platform/windows sources.
// Other modules must not include it.

/**
 * Reads token information whose size varies, such as TOKEN_USER, into storage
 * aligned for it. Returns std::nullopt if GetTokenInformation fails;
 * GetLastError() then says why.
 */
[[nodiscard]] std::optional<std::vector<uint64_t>> readTokenInformation(HANDLE token,
                                                                        TOKEN_INFORMATION_CLASS infoClass);

/**
 * Returns the user SID in a TOKEN_USER that readTokenInformation() read. It
 * points into the buffer, so it is valid as long as the buffer is.
 */
[[nodiscard]] PSID userSid(const std::vector<uint64_t>& tokenUser);

} // namespace verdandi::platform
