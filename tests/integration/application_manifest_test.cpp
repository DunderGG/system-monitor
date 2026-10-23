#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

// src/app/verdandi.manifest, which this test binary embeds like the app,
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

TEST(ApplicationManifestIntegration, SupportedOs_IsWindows10And11)
{
    // Components that look for the declaration treat a process without it as
    // written for Windows Vista (DC-137).
    SIZE_T size = 0;
    ::QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, nullptr, nullptr, CompatibilityInformationInActivationContext,
                   nullptr, 0, &size);
    std::vector<std::byte> information(size);
    const BOOL isQueried =
        ::QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, nullptr, nullptr,
                       CompatibilityInformationInActivationContext, information.data(), information.size(), &size);
    const DWORD error = ::GetLastError();
    ASSERT_TRUE(isQueried) << "QueryActCtxW failed with error " << error;

    // ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION: a count, then that many elements.
    constexpr std::size_t kElementsOffset = offsetof(ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION, Elements);
    ASSERT_GE(information.size(), kElementsOffset);
    DWORD count = 0;
    std::memcpy(&count, information.data(), sizeof(count));
    ASSERT_GE(information.size(), kElementsOffset + count * sizeof(COMPATIBILITY_CONTEXT_ELEMENT));
    std::vector<GUID> operatingSystems;
    for (DWORD index = 0; index < count; ++index) {
        COMPATIBILITY_CONTEXT_ELEMENT element{};
        std::memcpy(&element, information.data() + kElementsOffset + index * sizeof(element), sizeof(element));
        if (element.Type == ACTCTX_COMPATIBILITY_ELEMENT_TYPE_OS) {
            operatingSystems.push_back(element.Id);
        }
    }

    constexpr GUID kWindows10And11{0x8e0f7a12, 0xbfb3, 0x4fe8, {0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a}};
    EXPECT_EQ(operatingSystems, std::vector<GUID>{kWindows10And11});
}

TEST(ApplicationManifestIntegration, RequestedExecutionLevel_IsAsInvoker)
{
    // The monitor runs with its starter's rights and without an elevation prompt (DC-137).
    ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION runLevel{};
    const BOOL isQueried = ::QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, nullptr, nullptr,
                                          RunlevelInformationInActivationContext, &runLevel, sizeof(runLevel), nullptr);
    const DWORD error = ::GetLastError();

    ASSERT_TRUE(isQueried) << "QueryActCtxW failed with error " << error;
    EXPECT_EQ(runLevel.RunLevel, ACTCTX_RUN_LEVEL_AS_INVOKER);
    EXPECT_EQ(runLevel.UiAccess, 0U);
}
