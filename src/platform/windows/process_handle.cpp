#include "platform/windows/process_handle.h"

#include <cstdint>

#include "platform/windows/system_process_information.h"

namespace sysmon::platform
{

void HandleCloser::operator()(HANDLE handle) const noexcept
{
    if (handle != nullptr) {
        ::CloseHandle(handle);
    }
}

std::optional<std::chrono::system_clock::time_point> processCreationTime(HANDLE process)
{
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!::GetProcessTimes(process, &creation, &exit, &kernel, &user)) {
        return std::nullopt;
    }

    const auto creationTicks =
        static_cast<int64_t>((static_cast<uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime);
    return fileTimeToSystemClock(creationTicks);
}

} // namespace sysmon::platform
