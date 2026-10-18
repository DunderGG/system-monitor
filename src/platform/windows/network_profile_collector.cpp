#include "platform/windows/network_profile_collector.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
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
// multithreaded apartment, and uninitializes it again on destruction, which
// must happen on the same thread. If the thread already has a single-threaded
// apartment, COM calls use that one instead.
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
// connection whose adapter cannot be matched is left out, with a debug log
// line, so a network that shows "None" on every card leaves a trace.
std::vector<uint64_t> connectedInterfaceIds(INetwork& network)
{
    std::vector<uint64_t> interfaceIds;
    IEnumNetworkConnections* rawConnections = nullptr;
    const HRESULT connectionsResult = network.GetNetworkConnections(&rawConnections);
    if (FAILED(connectionsResult) || rawConnections == nullptr) {
        spdlog::debug("The connections of a network could not be listed (HRESULT {:#010x}); it matches no adapter",
                      static_cast<unsigned long>(connectionsResult));
        return interfaceIds;
    }
    const ScopedComPtr<IEnumNetworkConnections> connections(rawConnections);

    INetworkConnection* rawConnection = nullptr;
    while (connections->Next(1, &rawConnection, nullptr) == S_OK && rawConnection != nullptr) {
        const ScopedComPtr<INetworkConnection> connection(std::exchange(rawConnection, nullptr));
        GUID adapterId{};
        const HRESULT adapterResult = connection->GetAdapterId(&adapterId);
        if (FAILED(adapterResult)) {
            spdlog::debug("Skipped a network connection whose adapter could not be read (HRESULT {:#010x})",
                          static_cast<unsigned long>(adapterResult));
            continue;
        }
        NET_LUID luid{};
        const NETIO_STATUS luidResult = ConvertInterfaceGuidToLuid(&adapterId, &luid);
        if (luidResult != NO_ERROR) {
            spdlog::debug("Skipped a network connection whose adapter has no interface (error {})", luidResult);
            continue;
        }
        interfaceIds.push_back(luid.Value);
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

// Lists the connected networks with INetworkListManager, in the calling
// thread's apartment, which outlives every COM object created here. A network
// whose name cannot be read is left out; the list as a whole fails only when
// the manager or its enumeration cannot be reached.
std::optional<std::vector<domain::NetworkProfile>> queryConnectedNetworks(const ScopedComApartment& apartment,
                                                                          RepeatedFailureLog& failureLog)
{
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
          [] {
              // Called on the worker thread, which keeps this apartment until
              // the reader is destroyed there, after its last read.
              auto apartment = std::make_shared<const ScopedComApartment>();
              return NetworkProfileReader(
                  [apartment, failureLog = RepeatedFailureLog{"INetworkListManager"}]() mutable {
                      return queryConnectedNetworks(*apartment, failureLog);
                  });
          },
          subscribeToConnectivityChanges, ReadIntervals{})
{}

NetworkProfileCollector::NetworkProfileCollector(NetworkProfileReaderFactory readerFactory,
                                                 ConnectivitySubscriber subscriber, ReadIntervals intervals)
    : m_readerFactory(std::move(readerFactory)), m_intervals(intervals)
{
    assert(m_readerFactory && subscriber && "reader factory and subscriber are required");

    m_subscription = subscriber([this](domain::ConnectivityStatus /*status*/) { markStale(); });
    if (!m_subscription) {
        spdlog::warn("Connectivity change notifications unavailable; network names refresh every {}s",
                     std::chrono::duration_cast<std::chrono::seconds>(m_intervals.maximumAge).count());
    }
    m_worker = std::jthread([this](std::stop_token stopToken) { run(std::move(stopToken)); });
}

std::optional<std::vector<domain::NetworkProfile>> NetworkProfileCollector::collect()
{
    const std::lock_guard lock(m_mutex);
    return m_latest;
}

void NetworkProfileCollector::setChangeListener(monitoring::ChangeListener listener)
{
    const std::lock_guard lock(m_mutex);
    m_changeListener = std::move(listener);
}

std::uint64_t NetworkProfileCollector::readCount() const
{
    const std::lock_guard lock(m_mutex);
    return m_readCount;
}

void NetworkProfileCollector::markStale()
{
    const std::lock_guard lock(m_mutex);
    m_isStale = true;
    m_wake.notify_all();
}

bool NetworkProfileCollector::waitUntilReadIsDue(std::unique_lock<std::mutex>& lock, const std::stop_token& stopToken,
                                                 std::optional<std::chrono::steady_clock::time_point> lastRead)
{
    while (!stopToken.stop_requested()) {
        if (!lastRead) {
            return true;
        }
        const auto earliest = *lastRead + m_intervals.minimum;
        const auto due = m_isStale ? earliest : std::max(earliest, *lastRead + m_intervals.maximumAge);
        if (std::chrono::steady_clock::now() >= due) {
            return true;
        }
        if (m_isStale) {
            // Only the minimum interval is left: a notification cannot bring the read forward.
            m_wake.wait_until(lock, stopToken, due, [] { return false; });
        } else {
            // A notification makes the names stale and brings the read forward.
            m_wake.wait_until(lock, stopToken, due, [this] { return m_isStale; });
        }
    }
    return false;
}

void NetworkProfileCollector::run(std::stop_token stopToken)
{
    // Created and destroyed on this thread (see NetworkProfileReaderFactory).
    const NetworkProfileReader reader = m_readerFactory();
    std::optional<std::chrono::steady_clock::time_point> lastRead;
    while (true) {
        {
            std::unique_lock lock(m_mutex);
            if (!waitUntilReadIsDue(lock, stopToken, lastRead)) {
                return;
            }
            // Cleared before reading, so a change reported during the read
            // causes another read.
            m_isStale = false;
        }

        lastRead = std::chrono::steady_clock::now();
        auto profiles = reader();

        const std::lock_guard lock(m_mutex);
        ++m_readCount;
        if (profiles != m_latest) {
            m_latest = std::move(profiles);
            if (m_changeListener) {
                m_changeListener();
            }
        }
    }
}

} // namespace sysmon::platform
