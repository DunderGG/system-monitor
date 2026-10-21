#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "platform/windows/utf8_conversion.h"

using namespace verdandi::platform;

TEST(Utf8Conversion, WideToUtf8_Empty_ReturnsEmpty)
{
    EXPECT_EQ(wideToUtf8(L""), "");
}

TEST(Utf8Conversion, WideToUtf8_Ascii_Unchanged)
{
    EXPECT_EQ(wideToUtf8(L"explorer.exe"), "explorer.exe");
}

TEST(Utf8Conversion, WideToUtf8_TwoAndThreeByteCharacters_Encoded)
{
    // U+00F6 (two bytes) and U+20AC (three bytes).
    EXPECT_EQ(wideToUtf8(L"\x00F6\x20AC"), "\xC3\xB6\xE2\x82\xAC");
}

TEST(Utf8Conversion, WideToUtf8_SurrogatePair_EncodedAsOneFourByteCharacter)
{
    // U+1F600 is the surrogate pair D83D DE00.
    EXPECT_EQ(wideToUtf8(L"\xD83D\xDE00"), "\xF0\x9F\x98\x80");
}

TEST(Utf8Conversion, WideToUtf8_UnpairedSurrogates_BecomeReplacementCharacters)
{
    // A high surrogate followed by a non-surrogate, then a lone low surrogate.
    EXPECT_EQ(wideToUtf8(L"\xD83D"
                         L"a\xDE00"),
              "\xEF\xBF\xBD"
              "a"
              "\xEF\xBF\xBD");
}

TEST(Utf8Conversion, WideToUtf8_HighSurrogateAtEnd_BecomesReplacementCharacter)
{
    EXPECT_EQ(wideToUtf8(L"a\xD83D"), "a\xEF\xBF\xBD");
}

TEST(Utf8Conversion, WideToUtf8_EmbeddedNull_KeptBecauseLengthIsExplicit)
{
    const std::wstring_view text(L"a\0b", 3);

    EXPECT_EQ(wideToUtf8(text), std::string("a\0b", 3));
}
