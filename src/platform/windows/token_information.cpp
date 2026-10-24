#include "platform/windows/token_information.h"

#include <cstring>

namespace verdandi::platform
{

std::optional<std::vector<uint64_t>> readTokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS infoClass)
{
    DWORD size = 0;
    if (!::GetTokenInformation(token, infoClass, nullptr, 0, &size) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }

    std::vector<uint64_t> buffer((size + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    if (!::GetTokenInformation(token, infoClass, buffer.data(), size, &size)) {
        return std::nullopt;
    }
    return buffer;
}

PSID userSid(const std::vector<uint64_t>& tokenUser)
{
    TOKEN_USER header{};
    std::memcpy(&header, tokenUser.data(), sizeof(header));
    return header.User.Sid;
}

} // namespace verdandi::platform
