#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "verdandi_version.h"

// verdandi.exe as the build leaves it, whose path CMake passes in
// VERDANDI_APPLICATION_PATH, and the files beside it.

namespace
{

// A 64-bit PE image read whole from a file, whose parts are found by their
// relative virtual address (RVA) through the section table.
class PeImage
{
public:
    // Returns the image at path, or std::nullopt if the file cannot be read or
    // is not a 64-bit PE image.
    static std::optional<PeImage> read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        PeImage image;
        image.m_bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        IMAGE_DOS_HEADER dosHeader{};
        if (!image.copyAt(0, dosHeader) || dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew < 0 ||
            !image.copyAt(static_cast<std::size_t>(dosHeader.e_lfanew), image.m_ntHeaders) ||
            image.m_ntHeaders.Signature != IMAGE_NT_SIGNATURE ||
            image.m_ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return std::nullopt;
        }

        // The section table follows the optional header, whose size the file header gives.
        std::size_t offset = static_cast<std::size_t>(dosHeader.e_lfanew) +
                             offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                             image.m_ntHeaders.FileHeader.SizeOfOptionalHeader;
        for (WORD index = 0; index < image.m_ntHeaders.FileHeader.NumberOfSections; ++index) {
            IMAGE_SECTION_HEADER section{};
            if (!image.copyAt(offset, section)) {
                return std::nullopt;
            }
            image.m_sections.push_back(section);
            offset += sizeof(section);
        }
        return image;
    }

    [[nodiscard]] const IMAGE_NT_HEADERS64& ntHeaders() const
    {
        return m_ntHeaders;
    }

    // Returns the file offset of rva, or std::nullopt if no section holds it.
    [[nodiscard]] std::optional<std::size_t> offsetOf(DWORD rva) const
    {
        for (const IMAGE_SECTION_HEADER& section : m_sections) {
            if (rva >= section.VirtualAddress && rva - section.VirtualAddress < section.SizeOfRawData) {
                return std::size_t{section.PointerToRawData} + (rva - section.VirtualAddress);
            }
        }
        return std::nullopt;
    }

    // Returns the data directory at index, which is all zeros when the image has none.
    [[nodiscard]] IMAGE_DATA_DIRECTORY directory(std::size_t index) const
    {
        return index < m_ntHeaders.OptionalHeader.NumberOfRvaAndSizes ? m_ntHeaders.OptionalHeader.DataDirectory[index]
                                                                      : IMAGE_DATA_DIRECTORY{};
    }

    // Copies the bytes at file offset into value; false if the file ends first.
    template <typename T> bool copyAt(std::size_t offset, T& value) const
    {
        if (offset > m_bytes.size() || m_bytes.size() - offset < sizeof(T)) {
            return false;
        }
        std::memcpy(&value, m_bytes.data() + offset, sizeof(T));
        return true;
    }

    // Returns the null-terminated string at file offset, or an empty string if
    // the file ends first.
    [[nodiscard]] std::string stringAt(std::size_t offset) const
    {
        if (offset >= m_bytes.size()) {
            return {};
        }
        const auto begin = m_bytes.begin() + static_cast<std::ptrdiff_t>(offset);
        const auto end = std::find(begin, m_bytes.end(), '\0');
        return end != m_bytes.end() ? std::string(begin, end) : std::string{};
    }

private:
    std::vector<char> m_bytes;
    IMAGE_NT_HEADERS64 m_ntHeaders{};
    std::vector<IMAGE_SECTION_HEADER> m_sections;
};

// Returns the entries of the image's debug directory.
std::vector<IMAGE_DEBUG_DIRECTORY> debugEntries(const PeImage& image)
{
    const IMAGE_DATA_DIRECTORY directory = image.directory(IMAGE_DIRECTORY_ENTRY_DEBUG);
    std::vector<IMAGE_DEBUG_DIRECTORY> entries;
    const auto offset = image.offsetOf(directory.VirtualAddress);
    for (std::size_t index = 0; offset && index < directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++index) {
        IMAGE_DEBUG_DIRECTORY entry{};
        if (image.copyAt(*offset + index * sizeof(entry), entry)) {
            entries.push_back(entry);
        }
    }
    return entries;
}

// Returns the first debug entry of type, or std::nullopt if the image has none.
std::optional<IMAGE_DEBUG_DIRECTORY> debugEntry(const PeImage& image, DWORD type)
{
    const auto entries = debugEntries(image);
    const auto entry = std::ranges::find(entries, type, &IMAGE_DEBUG_DIRECTORY::Type);
    return entry != entries.end() ? std::optional{*entry} : std::nullopt;
}

std::optional<PeImage> applicationImage()
{
    return PeImage::read(VERDANDI_APPLICATION_PATH);
}

std::filesystem::path applicationDirectory()
{
    return std::filesystem::path{VERDANDI_APPLICATION_PATH}.parent_path();
}

} // namespace

TEST(ApplicationExecutableIntegration, Subsystem_IsWindowsGui)
{
    // Started from Explorer, a console program opens a console window beside
    // its own, and closing that window ends the monitor without its shutdown (DC-134).
    const auto image = applicationImage();

    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(image->ntHeaders().OptionalHeader.Subsystem, IMAGE_SUBSYSTEM_WINDOWS_GUI);
}

TEST(ApplicationExecutableIntegration, Mitigations_ControlFlowGuardAndCetShadowStackCompatibilityAreOn)
{
    // DC-137. The linker records CET compatibility in a debug entry of its own.
    const auto image = applicationImage();
    ASSERT_TRUE(image.has_value());
    const auto extendedCharacteristics = debugEntry(*image, IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS);
    DWORD extendedFlags = 0;
    const bool hasExtendedFlags =
        extendedCharacteristics && image->copyAt(extendedCharacteristics->PointerToRawData, extendedFlags);

    EXPECT_NE(image->ntHeaders().OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF, 0);
    ASSERT_TRUE(hasExtendedFlags);
    EXPECT_NE(extendedFlags & IMAGE_DLLCHARACTERISTICS_EX_CET_COMPAT, 0U);
}

TEST(ApplicationExecutableIntegration, ProgramDatabase_IsBesideTheExecutable)
{
    // Release builds link with /DEBUG too, so that crash dumps can be symbolized (DC-137).
    const auto image = applicationImage();
    ASSERT_TRUE(image.has_value());
    const auto codeView = debugEntry(*image, IMAGE_DEBUG_TYPE_CODEVIEW);
    ASSERT_TRUE(codeView.has_value());

    // An RSDS record: its signature, a GUID and an age, then the path of the program database.
    constexpr DWORD kRsdsSignature = 0x53445352;
    DWORD signature = 0;
    ASSERT_TRUE(image->copyAt(codeView->PointerToRawData, signature));
    EXPECT_EQ(signature, kRsdsSignature);
    const std::filesystem::path programDatabase =
        image->stringAt(codeView->PointerToRawData + sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD));

    EXPECT_EQ(programDatabase.filename(),
              std::filesystem::path{VERDANDI_APPLICATION_PATH}.replace_extension(".pdb").filename());
    EXPECT_TRUE(std::filesystem::exists(programDatabase)) << programDatabase.string();
}

TEST(ApplicationExecutableIntegration, VersionInformation_HasTheProjectVersionAndADescription)
{
    // Explorer's Details tab shows the version, and Task Manager the description (DC-136).
    const std::wstring path = std::filesystem::path{VERDANDI_APPLICATION_PATH}.wstring();
    const DWORD size = ::GetFileVersionInfoSizeW(path.c_str(), nullptr);
    ASSERT_NE(size, 0U) << "GetFileVersionInfoSizeW failed with error " << ::GetLastError();
    std::vector<std::byte> versionInformation(size);
    ASSERT_TRUE(::GetFileVersionInfoW(path.c_str(), 0, size, versionInformation.data()));

    void* fixedInformation = nullptr;
    UINT fixedSize = 0;
    ASSERT_TRUE(::VerQueryValueW(versionInformation.data(), L"\\", &fixedInformation, &fixedSize));
    ASSERT_GE(fixedSize, sizeof(VS_FIXEDFILEINFO));
    VS_FIXEDFILEINFO fixed{};
    std::memcpy(&fixed, fixedInformation, sizeof(fixed));
    void* description = nullptr;
    UINT descriptionLength = 0;
    const BOOL hasDescription = ::VerQueryValueW(
        versionInformation.data(), L"\\StringFileInfo\\040904B0\\FileDescription", &description, &descriptionLength);

    EXPECT_EQ(HIWORD(fixed.dwFileVersionMS), VERDANDI_VERSION_MAJOR);
    EXPECT_EQ(LOWORD(fixed.dwFileVersionMS), VERDANDI_VERSION_MINOR);
    EXPECT_EQ(HIWORD(fixed.dwFileVersionLS), VERDANDI_VERSION_PATCH);
    ASSERT_TRUE(hasDescription);
    EXPECT_EQ(std::wstring(static_cast<const wchar_t*>(description)), L"Verdandi");
}

TEST(ApplicationExecutableIntegration, LicenseAndThirdPartyNotices_AreBesideTheExecutable)
{
    // The About box links to both (DC-138).
    std::ifstream notices(applicationDirectory() / "THIRD_PARTY_NOTICES.txt");
    const std::string text{std::istreambuf_iterator<char>(notices), std::istreambuf_iterator<char>()};

    EXPECT_TRUE(std::filesystem::exists(applicationDirectory() / "LICENSE.txt"));
    EXPECT_NE(text.find("qtbase"), std::string::npos);
}

#ifdef NDEBUG
// Debug builds import the debug runtime, which may not be redistributed, so
// only Release builds deploy the runtime and have this test.

namespace
{

// Returns the names of the DLLs that the image imports, in lower case.
std::vector<std::string> importedModules(const PeImage& image)
{
    std::vector<std::string> modules;
    const auto offset = image.offsetOf(image.directory(IMAGE_DIRECTORY_ENTRY_IMPORT).VirtualAddress);
    IMAGE_IMPORT_DESCRIPTOR descriptor{};
    // The table ends with a descriptor of zeros.
    for (std::size_t index = 0;
         offset && image.copyAt(*offset + index * sizeof(descriptor), descriptor) && descriptor.Name != 0; ++index) {
        if (const auto nameOffset = image.offsetOf(descriptor.Name)) {
            std::string name = image.stringAt(*nameOffset);
            std::ranges::transform(name, name.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            modules.push_back(std::move(name));
        }
    }
    return modules;
}

} // namespace

TEST(ApplicationExecutableIntegration, ReleaseBuild_CppRuntimeThatItsImagesImportIsBesideTheExecutable)
{
    // Without it, a copy of the directory starts only where a Visual C++
    // Redistributable at least as new as the build's is installed (DC-137).
    const std::filesystem::path directory = applicationDirectory();
    std::vector<std::filesystem::path> images{VERDANDI_APPLICATION_PATH};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.path().extension() == ".dll") {
            images.push_back(entry.path());
        }
    }
    constexpr std::array kRuntimePrefixes{"msvcp", "vcruntime", "concrt"};

    for (const auto& path : images) {
        SCOPED_TRACE(path.string());
        const auto image = PeImage::read(path);
        ASSERT_TRUE(image.has_value());
        for (const std::string& module : importedModules(*image)) {
            const bool isRuntime = std::ranges::any_of(
                kRuntimePrefixes, [&module](const char* prefix) { return module.starts_with(prefix); });
            if (isRuntime) {
                EXPECT_TRUE(std::filesystem::exists(directory / module)) << module;
            }
        }
    }
}
#endif
