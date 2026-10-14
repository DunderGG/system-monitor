#pragma once

#include <cstdint>
#include <optional>

namespace sysmon::platform
{

/**
 * Returns the bytes per second between two readings of a cumulative byte
 * counter taken elapsedSeconds apart, rounded to the nearest byte. Returns
 * std::nullopt when no meaningful rate exists: a non-positive elapsed time,
 * or a counter that went backwards (a reset), across which the delta is
 * unknown.
 */
[[nodiscard]] std::optional<uint64_t> calculateByteRate(uint64_t previousBytes, uint64_t currentBytes,
                                                        double elapsedSeconds);

} // namespace sysmon::platform
