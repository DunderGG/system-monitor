#include <array>
#include <string>

#include <windows.h>

#include <gtest/gtest.h>

// src/app/system_monitor.manifest, which this test binary embeds like the app,
// read back from the process's default activation context, where Windows reads
// it at process start.

TEST(ApplicationManifestIntegration, DpiAwareness_IsPerMonitorV2)
{
    // Qt asks for the same awareness when the QApplication starts, so the
    // process's awareness would be PerMonitorV2 without the manifest too. The
    // setting is checked instead (DC-128).
    std::array<wchar_t, 64> value{};
    const BOOL isDeclared = ::QueryActCtxSettingsW(0, nullptr, L"http://schemas.microsoft.com/SMI/2016/WindowsSettings",
                                                   L"dpiAwareness", value.data(), value.size(), nullptr);
    const DWORD error = ::GetLastError();

    ASSERT_TRUE(isDeclared) << "QueryActCtxSettingsW failed with error " << error;
    EXPECT_EQ(std::wstring{value.data()}, L"PerMonitorV2");
}
