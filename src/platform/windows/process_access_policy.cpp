#include "platform/windows/process_access_policy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace verdandi::platform
{

namespace
{

constexpr std::array<std::string_view, 2> kNeverOpenedImageNames{"lsass.exe", "csrss.exe"};

// The System Idle Process, which has no image name in the list.
constexpr uint32_t kIdleProcessId = 0;

constexpr char toLowerAscii(char character)
{
    return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a') : character;
}

bool equalsIgnoringAsciiCase(std::string_view text, std::string_view lowercase)
{
    return std::ranges::equal(text, lowercase,
                              [](char character, char expected) { return toLowerAscii(character) == expected; });
}

} // namespace

bool isNeverOpenedProcess(const domain::ProcessInfo& process)
{
    // A process without a name could be lsass.exe or csrss.exe, so it is not
    // opened either.
    if (process.imageName.empty()) {
        return process.pid != kIdleProcessId;
    }
    return std::ranges::any_of(kNeverOpenedImageNames, [&process](std::string_view name) {
        return equalsIgnoringAsciiCase(process.imageName, name);
    });
}

ProcessDetailsReader skippingNeverOpenedProcesses(ProcessDetailsReader reader)
{
    if (!reader) {
        return nullptr;
    }

    return [reader = std::move(reader)](const domain::ProcessInfo& process) {
        if (isNeverOpenedProcess(process)) {
            spdlog::debug("Not opening {} (PID {}); its details are not queried",
                          process.imageName.empty() ? std::string_view{"a process without a name"}
                                                    : std::string_view{process.imageName},
                          process.pid);
            return ProcessDetails{};
        }
        return reader(process);
    };
}

} // namespace verdandi::platform
