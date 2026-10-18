#include "platform/windows/hidden_adapters.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include <windows.h>
#include <netcfgx.h>
#include <objbase.h>
#include <setupapi.h>

#include <spdlog/spdlog.h>

namespace sysmon::platform
{

namespace
{

// The device setup class of network adapters, GUID_DEVCLASS_NET in devguid.h.
// devguid.h defines it only after initguid.h, which the include order cannot
// guarantee, so the value is repeated here. It never changes.
constexpr GUID kNetworkAdapterClass{0x4d36e972, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

// A device's NetCfgInstanceId is a GUID in braces: 38 characters and a null.
constexpr std::size_t kGuidTextLength = 39;

struct DeviceInfoListDeleter
{
    void operator()(HDEVINFO devices) const noexcept
    {
        SetupDiDestroyDeviceInfoList(devices);
    }
};

using ScopedDeviceInfoList = std::unique_ptr<std::remove_pointer_t<HDEVINFO>, DeviceInfoListDeleter>;

struct RegistryKeyDeleter
{
    void operator()(HKEY key) const noexcept
    {
        RegCloseKey(key);
    }
};

using ScopedRegistryKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegistryKeyDeleter>;

// Returns the interface GUID of the adapter whose driver key is key, if its
// driver marks it hidden; std::nullopt if it does not, or the key cannot be read.
std::optional<GUID> hiddenAdapterGuid(HKEY key)
{
    DWORD characteristics = 0;
    DWORD size = sizeof(characteristics);
    if (RegGetValueW(key, nullptr, L"Characteristics", RRF_RT_REG_DWORD, nullptr, &characteristics, &size) !=
            ERROR_SUCCESS ||
        (characteristics & NCF_HIDDEN) == 0) {
        return std::nullopt;
    }

    std::array<wchar_t, kGuidTextLength> text{};
    size = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    const LSTATUS idResult =
        RegGetValueW(key, nullptr, L"NetCfgInstanceId", RRF_RT_REG_SZ, nullptr, text.data(), &size);
    GUID guid{};
    if (idResult != ERROR_SUCCESS || FAILED(CLSIDFromString(text.data(), &guid))) {
        spdlog::debug("Skipped a hidden network adapter whose NetCfgInstanceId could not be read (error {})", idResult);
        return std::nullopt;
    }
    return guid;
}

} // namespace

std::optional<std::vector<GUID>> queryHiddenAdapterGuids(RepeatedFailureLog& failureLog)
{
    // No DIGCF_PRESENT: an adapter that is not present can still have an interface.
    HDEVINFO rawDevices = SetupDiGetClassDevsW(&kNetworkAdapterClass, nullptr, nullptr, 0);
    if (rawDevices == INVALID_HANDLE_VALUE) {
        failureLog.failure(spdlog::level::warn,
                           "SetupDiGetClassDevsW failed with error {}; no network adapter is treated as hidden",
                           GetLastError());
        return std::nullopt;
    }
    const ScopedDeviceInfoList devices(rawDevices);

    std::vector<GUID> hidden;
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(SP_DEVINFO_DATA);
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices.get(), index, &device) != FALSE; ++index) {
        HKEY rawKey = SetupDiOpenDevRegKey(devices.get(), &device, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_QUERY_VALUE);
        if (rawKey == INVALID_HANDLE_VALUE) {
            // A device whose driver is not installed has no driver key.
            continue;
        }
        const ScopedRegistryKey key(rawKey);
        if (const auto guid = hiddenAdapterGuid(key.get())) {
            hidden.push_back(*guid);
        }
    }

    const DWORD enumError = GetLastError();
    if (enumError != ERROR_NO_MORE_ITEMS) {
        failureLog.failure(spdlog::level::warn,
                           "SetupDiEnumDeviceInfo failed with error {}; no network adapter is treated as hidden",
                           enumError);
        return std::nullopt;
    }
    failureLog.success();
    return hidden;
}

} // namespace sysmon::platform
