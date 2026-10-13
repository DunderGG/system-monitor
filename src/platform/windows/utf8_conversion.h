#pragma once

#include <string>
#include <string_view>

namespace sysmon::platform
{

/**
 * Converts UTF-16 text from a Windows API (wchar_t is 16 bits on Windows) to
 * UTF-8, the encoding of every domain string (DC-15). An unpaired surrogate
 * becomes U+FFFD, as WideCharToMultiByte does. Implemented in portable C++, so
 * it makes no OS call and unit tests can use it.
 */
[[nodiscard]] std::string wideToUtf8(std::wstring_view text);

} // namespace sysmon::platform
