#include "platform/windows/current_process.h"

#include <windows.h>

#include <spdlog/spdlog.h>

#include "platform/windows/process_handle.h"

namespace verdandi::platform
{

std::optional<domain::ProcessIdentity> currentProcessIdentity()
{
    // The pseudo handle needs no closing and has every right to this process.
    const auto creationTime = processCreationTime(::GetCurrentProcess());
    if (!creationTime) {
        spdlog::warn("GetProcessTimes failed for this process with error {}", ::GetLastError());
        return std::nullopt;
    }
    return domain::ProcessIdentity{.pid = ::GetCurrentProcessId(), .creationTime = *creationTime};
}

} // namespace verdandi::platform
