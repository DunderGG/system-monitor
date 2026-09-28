#include "platform/windows/connectivity_collector.h"

#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include <winsock2.h>
// Required: netioapi.h only declares CancelMibChangeNotify2 when ws2ipdef.h is included.
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <spdlog/spdlog.h>

#include "platform/windows/connectivity_hint_mapping.h"

namespace sysmon::platform
{

namespace
{

std::string_view levelName(domain::ConnectivityLevel level) noexcept
{
    switch (level) {
        case domain::ConnectivityLevel::None:
            return "None";
        case domain::ConnectivityLevel::LocalAccess:
            return "LocalAccess";
        case domain::ConnectivityLevel::ConstrainedInternetAccess:
            return "ConstrainedInternetAccess";
        case domain::ConnectivityLevel::InternetAccess:
            return "InternetAccess";
        case domain::ConnectivityLevel::Unknown:
        default:
            return "Unknown";
    }
}

std::string_view meteredName(std::optional<bool> isMetered) noexcept
{
    if (!isMetered) {
        return "unknown";
    }
    return *isMetered ? "yes" : "no";
}

std::optional<domain::ConnectivityStatus> queryConnectivityHint()
{
    NL_NETWORK_CONNECTIVITY_HINT hint{};
    const DWORD result = GetNetworkConnectivityHint(&hint);
    if (result != NO_ERROR) {
        spdlog::warn("GetNetworkConnectivityHint failed with error {}", result);
        return std::nullopt;
    }
    return toConnectivityStatus(hint);
}

} // namespace

struct ConnectivityCollector::NotificationBridge
{
    // Invoked on a system thread pool thread; follows the event-driven collector
    // rules in docs/architecture.md (store only, cancelled before teardown).
    static void WINAPI onConnectivityChange(PVOID callerContext, NL_NETWORK_CONNECTIVITY_HINT hint) noexcept
    {
        auto *self = static_cast<ConnectivityCollector *>(callerContext);
        self->applyStatus(toConnectivityStatus(hint));
    }
};

void ConnectivityCollector::NotificationHandleDeleter::operator()(void *handle) const noexcept
{
    // Blocks until any in-flight callback has returned.
    CancelMibChangeNotify2(handle);
}

ConnectivityCollector::ConnectivityCollector()
{
    // Seed synchronously so the first collect() is accurate without waiting for
    // the asynchronous initial notification. Done before registering so a
    // callback can never be overwritten by an older seed value. The initial
    // notification still closes the gap between seeding and registration.
    if (const auto status = queryConnectivityHint()) {
        applyStatus(*status);
    }

    HANDLE handle = nullptr;
    const DWORD result = NotifyNetworkConnectivityHintChange(&NotificationBridge::onConnectivityChange, this,
                                                             TRUE, &handle);
    if (result != NO_ERROR) {
        // Known deviation D-2 (docs/known_deviations.md): polls on the scheduler thread.
        spdlog::warn("NotifyNetworkConnectivityHintChange failed with error {}; "
                     "falling back to polling GetNetworkConnectivityHint",
                     result);
        m_reader = queryConnectivityHint;
        return;
    }
    m_notificationHandle.reset(handle);
}

ConnectivityCollector::ConnectivityCollector(ConnectivityReader reader)
    : m_reader(std::move(reader))
{
}

domain::ConnectivityStatus ConnectivityCollector::collect()
{
    if (m_reader) {
        applyStatus(m_reader().value_or(domain::ConnectivityStatus{}));
    }

    const std::lock_guard lock(m_mutex);
    return m_cachedStatus;
}

void ConnectivityCollector::applyStatus(domain::ConnectivityStatus status)
{
    bool hasChanged = false;
    {
        const std::lock_guard lock(m_mutex);
        hasChanged = status != m_cachedStatus;
        m_cachedStatus = status;
    }

    if (hasChanged) {
        spdlog::info("Connectivity changed: level={}, metered={}", levelName(status.level),
                     meteredName(status.isMetered));
    }
}

} // namespace sysmon::platform
