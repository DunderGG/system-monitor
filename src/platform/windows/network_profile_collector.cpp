#include "platform/windows/network_profile_collector.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netlistmgr.h>
#include <oleauto.h>

#include <spdlog/spdlog.h>

#include "platform/windows/network_category_mapping.h"
#include "platform/windows/repeated_failure_log.h"
#include "platform/windows/utf8_conversion.h"

namespace sysmon::platform
{

namespace
{

// Initializes COM on the calling thread for the lifetime of the object, in the
// multithreaded apartment, and uninitializes it again. If the thread already
// has a single-threaded apartment, COM calls use that one instead.
class ScopedComApartment
{
public:
    ScopedComApartment() : m_result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

    ~ScopedComApartment()
    {
        // S_FALSE (already initialized) must be balanced too; RPC_E_CHANGED_MODE must not.
        if (SUCCEEDED(m_result)) {
            CoUninitialize();
        }
    }

    ScopedComApartment(const ScopedComApartment&) = delete;
    ScopedComApartment& operator=(const ScopedComApartment&) = delete;
    ScopedComApartment(ScopedComApartment&&) = delete;
    ScopedComApartment& operator=(ScopedComApartment&&) = delete;

    [[nodiscard]] bool isUsable() const
    {
        return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
    }

    [[nodiscard]] HRESULT result() const
    {
        return m_result;
    }

private:
    HRESULT m_result;
};

struct ComReleaser
{
    void operator()(IUnknown* object) const noexcept
    {
        if (object != nullptr) {
            object->Release();
        }
    }
};

// Owns one reference to a COM object.
template <typename T> using ScopedComPtr = std::unique_ptr<T, ComReleaser>;

struct BstrDeleter
{
    void operator()(BSTR text) const noexcept
    {
        SysFreeString(text);
    }
};

using ScopedBstr = std::unique_ptr<std::remove_pointer_t<BSTR>, BstrDeleter>;

// Returns the interface LUIDs of the adapters connected to network. A
// connection whose adapter cannot be matched is left out.
std::vector<uint64_t> connectedInterfaceIds(INetwork& network)
{
    std::vector<uint64_t> interfaceIds;
    IEnumNetworkConnections* rawConnections = nullptr;
    if (FAILED(network.GetNetworkConnections(&rawConnections)) || rawConnections == nullptr) {
        return interfaceIds;
    }
    const ScopedComPtr<IEnumNetworkConnections> connections(rawConnections);

    INetworkConnection* rawConnection = nullptr;
    while (connections->Next(1, &rawConnection, nullptr) == S_OK && rawConnection != nullptr) {
        const ScopedComPtr<INetworkConnection> connection(std::exchange(rawConnection, nullptr));
        GUID adapterId{};
        NET_LUID luid{};
        if (SUCCEEDED(connection->GetAdapterId(&adapterId)) &&
            ConvertInterfaceGuidToLuid(&adapterId, &luid) == NO_ERROR) {
            interfaceIds.push_back(luid.Value);
        }
    }
    return interfaceIds;
}

std::optional<domain::NetworkProfile> toNetworkProfile(INetwork& network)
{
    BSTR rawName = nullptr;
    if (FAILED(network.GetName(&rawName)) || rawName == nullptr) {
        return std::nullopt;
    }
    const ScopedBstr name(rawName);

    NLM_NETWORK_CATEGORY category{};
    const bool hasCategory = SUCCEEDED(network.GetCategory(&category));
    return domain::NetworkProfile{
        .name = wideToUtf8(std::wstring_view(name.get(), SysStringLen(name.get()))),
        .category = hasCategory ? toNetworkCategory(category) : std::nullopt,
        .interfaceIds = connectedInterfaceIds(network),
    };
}

// Lists the connected networks with INetworkListManager. A network whose name
// cannot be read is left out; the list as a whole fails only when the manager
// or its enumeration cannot be reached.
std::optional<std::vector<domain::NetworkProfile>> queryConnectedNetworks(RepeatedFailureLog& failureLog)
{
    // Declared first, so the COM objects below are released before COM is uninitialized.
    const ScopedComApartment apartment;
    if (!apartment.isUsable()) {
        failureLog.failure(spdlog::level::warn, "CoInitializeEx failed with HRESULT {:#010x}",
                           static_cast<unsigned long>(apartment.result()));
        return std::nullopt;
    }

    INetworkListManager* rawManager = nullptr;
    const HRESULT createResult =
        CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&rawManager));
    if (FAILED(createResult) || rawManager == nullptr) {
        failureLog.failure(spdlog::level::warn, "Creating the Network List Manager failed with HRESULT {:#010x}",
                           static_cast<unsigned long>(createResult));
        return std::nullopt;
    }
    const ScopedComPtr<INetworkListManager> manager(rawManager);

    IEnumNetworks* rawNetworks = nullptr;
    const HRESULT enumResult = manager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &rawNetworks);
    if (FAILED(enumResult) || rawNetworks == nullptr) {
        failureLog.failure(spdlog::level::warn, "INetworkListManager::GetNetworks failed with HRESULT {:#010x}",
                           static_cast<unsigned long>(enumResult));
        return std::nullopt;
    }
    const ScopedComPtr<IEnumNetworks> networks(rawNetworks);
    failureLog.success();

    std::vector<domain::NetworkProfile> profiles;
    INetwork* rawNetwork = nullptr;
    while (networks->Next(1, &rawNetwork, nullptr) == S_OK && rawNetwork != nullptr) {
        const ScopedComPtr<INetwork> network(std::exchange(rawNetwork, nullptr));
        if (auto profile = toNetworkProfile(*network)) {
            profiles.push_back(std::move(*profile));
        } else {
            spdlog::debug("Skipped a connected network whose name could not be read");
        }
    }
    return profiles;
}

} // namespace

NetworkProfileCollector::NetworkProfileCollector()
    : NetworkProfileCollector(
          [failureLog = RepeatedFailureLog{"INetworkListManager"}]() mutable {
              return queryConnectedNetworks(failureLog);
          },
          subscribeToConnectivityChanges, [] { return std::chrono::steady_clock::now(); })
{}

NetworkProfileCollector::NetworkProfileCollector(NetworkProfileReader reader, ConnectivitySubscriber subscriber,
                                                 SteadyClockReader clockReader)
    : m_reader(std::move(reader)), m_clockReader(std::move(clockReader))
{
    assert(m_reader && subscriber && m_clockReader && "reader, subscriber, and clock reader are required");

    // The query itself waits for the first collect(), on the slow-collector thread.
    m_subscription = subscriber([this](domain::ConnectivityStatus /*status*/) { markStale(); });
    if (!m_subscription) {
        spdlog::warn("Connectivity change notifications unavailable; network names refresh every {}s", kMaxAge.count());
    }
}

std::optional<std::vector<domain::NetworkProfile>> NetworkProfileCollector::collect()
{
    const auto now = m_clockReader();
    bool isDue = !m_lastRead || now - *m_lastRead >= kMaxAge;
    {
        // Cleared before reading, so a change reported during the read
        // triggers another read on the next call.
        const std::lock_guard lock(m_staleMutex);
        isDue = isDue || m_isStale;
        m_isStale = false;
    }

    if (isDue) {
        m_cached = m_reader();
        m_lastRead = now;
    }
    return m_cached;
}

void NetworkProfileCollector::setChangeListener(monitoring::ChangeListener listener)
{
    const std::lock_guard lock(m_staleMutex);
    m_changeListener = std::move(listener);
}

void NetworkProfileCollector::markStale()
{
    const std::lock_guard lock(m_staleMutex);
    // While the cache is already stale, a read is already due.
    if (m_isStale) {
        return;
    }
    m_isStale = true;
    if (m_changeListener) {
        m_changeListener();
    }
}

} // namespace sysmon::platform
