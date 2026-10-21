#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

namespace sysmon::tests
{

/**
 * A new, empty directory under the temporary directory, named after the
 * running test, removed with everything in it on destruction. Its name has a
 * character outside the ANSI code pages (U+0416, Cyrillic), so the paths of
 * the files in it must be passed to Windows as UTF-16.
 */
class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        std::error_code error;
        m_path = std::filesystem::temp_directory_path(error);
        EXPECT_FALSE(error) << error.message();
        std::wstring name = L"verdandi_settings_";
        name += static_cast<wchar_t>(0x0416);
        m_path /= name;
        m_path += ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::filesystem::remove_all(m_path, error); // Left by a run that crashed.
        std::filesystem::create_directories(m_path, error);
        EXPECT_FALSE(error) << error.message();
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

} // namespace sysmon::tests
