#pragma once

#include <chrono>
#include <functional>

namespace sysmon::platform
{

/**
 * Returns the current std::chrono::steady_clock time. Rate-based collectors take
 * one so unit tests can control elapsed time; production code uses
 * std::chrono::steady_clock::now().
 */
using SteadyClockReader = std::function<std::chrono::steady_clock::time_point()>;

} // namespace sysmon::platform
