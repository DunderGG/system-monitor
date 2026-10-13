#include "platform/windows/utf8_conversion.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace sysmon::platform
{

namespace
{

static_assert(sizeof(wchar_t) == 2, "wideToUtf8 expects UTF-16 wchar_t, as on Windows");

constexpr char32_t kReplacementCharacter = 0xFFFD;

constexpr bool isHighSurrogate(char32_t unit)
{
    return unit >= 0xD800 && unit <= 0xDBFF;
}

constexpr bool isLowSurrogate(char32_t unit)
{
    return unit >= 0xDC00 && unit <= 0xDFFF;
}

void appendUtf8(std::string& output, char32_t codePoint)
{
    if (codePoint < 0x80) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

} // namespace

std::string wideToUtf8(std::wstring_view text)
{
    std::string output;
    output.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t codePoint = static_cast<char16_t>(text[i]);
        if (isHighSurrogate(codePoint) && i + 1 < text.size()) {
            const char32_t next = static_cast<char16_t>(text[i + 1]);
            if (isLowSurrogate(next)) {
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (next - 0xDC00);
                ++i;
            }
        }
        if (isHighSurrogate(codePoint) || isLowSurrogate(codePoint)) {
            codePoint = kReplacementCharacter;
        }
        appendUtf8(output, codePoint);
    }
    return output;
}

} // namespace sysmon::platform
