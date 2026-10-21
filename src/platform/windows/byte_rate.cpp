#include "platform/windows/byte_rate.h"

#include <cmath>

namespace verdandi::platform
{

std::optional<uint64_t> calculateByteRate(uint64_t previousBytes, uint64_t currentBytes, double elapsedSeconds)
{
    if (elapsedSeconds <= 0.0 || currentBytes < previousBytes) {
        return std::nullopt;
    }
    const double deltaBytes = static_cast<double>(currentBytes - previousBytes);
    return static_cast<uint64_t>(std::round(deltaBytes / elapsedSeconds));
}

} // namespace verdandi::platform
