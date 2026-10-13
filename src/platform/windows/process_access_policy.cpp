#include "platform/windows/process_access_policy.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace sysmon::platform
{

namespace
{

constexpr std::array<std::string_view, 2> kNeverOpenedImageNames{"lsass.exe", "csrss.exe"};

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

bool isNeverOpenedProcess(std::string_view imageName)
{
    return std::ranges::any_of(kNeverOpenedImageNames,
                               [imageName](std::string_view name) { return equalsIgnoringAsciiCase(imageName, name); });
}

ProcessDetailsReader skippingNeverOpenedProcesses(ProcessDetailsReader reader)
{
    if (!reader) {
        return nullptr;
    }

    return [reader = std::move(reader)](const domain::ProcessInfo& process) {
        if (isNeverOpenedProcess(process.imageName)) {
            spdlog::debug("Not opening {} (PID {}); its details are not queried", process.imageName, process.pid);
            return ProcessDetails{};
        }
        return reader(process);
    };
}

} // namespace sysmon::platform
