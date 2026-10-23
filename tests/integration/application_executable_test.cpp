#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

// verdandi.exe as the build leaves it, whose path CMake passes in
// VERDANDI_APPLICATION_PATH.

namespace
{

// Returns the Subsystem field of the optional header of the 64-bit PE image
// at path, or std::nullopt if the file cannot be read or is not one.
std::optional<WORD> imageSubsystem(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    // The headers that lead to the field fit in the first page of an image.
    std::vector<char> headers(4096);
    file.read(headers.data(), static_cast<std::streamsize>(headers.size()));
    const auto size = static_cast<std::size_t>(file.gcount());

    IMAGE_DOS_HEADER dosHeader{};
    if (size < sizeof(dosHeader)) {
        return std::nullopt;
    }
    std::memcpy(&dosHeader, headers.data(), sizeof(dosHeader));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew < 0 ||
        static_cast<std::size_t>(dosHeader.e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > size) {
        return std::nullopt;
    }

    IMAGE_NT_HEADERS64 ntHeaders{};
    std::memcpy(&ntHeaders, headers.data() + dosHeader.e_lfanew, sizeof(ntHeaders));
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE || ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return std::nullopt;
    }
    return ntHeaders.OptionalHeader.Subsystem;
}

} // namespace

TEST(ApplicationExecutableIntegration, Subsystem_IsWindowsGui)
{
    // Started from Explorer, a console program opens a console window beside
    // its own, and closing that window ends the monitor without its shutdown (DC-134).
    EXPECT_EQ(imageSubsystem(VERDANDI_APPLICATION_PATH), std::optional<WORD>{IMAGE_SUBSYSTEM_WINDOWS_GUI});
}
