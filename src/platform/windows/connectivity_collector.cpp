#include "platform/windows/connectivity_collector.h"

#include <cassert>
#include <memory>
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

// RAII registration of NotifyNetworkConnectivityHintChange. The callback runs on
// a system thread pool thread and only forwards the mapped status to the
// handler (event-driven collector rules in docs/architecture.md).
class WindowsConnectivitySubscription final : public ConnectivitySubscription
{
public:
    explicit WindowsConnectivitySubscription(ConnectivityChangeHandler handler)
        : m_handler(std::move(handler))
    {
    }

    ~WindowsConnectivitySubscription() override
    {
        if (m_handle != nullptr) {
            // Blocks until any in-flight callback has returned.
            CancelMibChangeNotify2(m_handle);
        }
    }

    WindowsConnectivitySubscription(const WindowsConnectivitySubscription &) = delete;
    WindowsConnectivitySubscription &operator=(const WindowsConnectivitySubscription &) = delete;
    WindowsConnectivitySubscription(WindowsConnectivitySubscription &&) = delete;
    WindowsConnectivitySubscription &operator=(WindowsConnectivitySubscription &&) = delete;

    // Registers with an initial notification, which closes the gap between the
    // caller's synchronous seed read and this registration.
    [[nodiscard]] bool registerNotifications()
    {
        const DWORD result = NotifyNetworkConnectivityHintChange(&onConnectivityChange, this, TRUE, &m_handle);
        if (result != NO_ERROR) {
            spdlog::warn("NotifyNetworkConnectivityHintChange failed with error {}", result);
            m_handle = nullptr;
            return false;
        }
        return true;
    }

private:
    static void WINAPI onConnectivityChange(PVOID callerContext, NL_NETWORK_CONNECTIVITY_HINT hint) noexcept
    {
        static_cast<WindowsConnectivitySubscription *>(callerContext)->m_handler(toConnectivityStatus(hint));
    }

    ConnectivityChangeHandler m_handler;
    HANDLE m_handle{nullptr};
};

std::unique_ptr<ConnectivitySubscription> subscribeToConnectivityChanges(ConnectivityChangeHandler handler)
{
    auto subscription = std::make_unique<WindowsConnectivitySubscription>(std::move(handler));
    if (!subscription->registerNotifications()) {
        return nullptr;
    }
    return subscription;
}

} // namespace

ConnectivityCollector::ConnectivityCollector()
    : ConnectivityCollector(queryConnectivityHint, subscribeToConnectivityChanges)
{
}

ConnectivityCollector::ConnectivityCollector(ConnectivityReader reader, ConnectivitySubscriber subscriber)
{
    assert(reader && subscriber && "reader and subscriber are required");

    // Seed synchronously so the first collect() is accurate without waiting for
    // an asynchronous notification, and before subscribing so a notification
    // can never be overwritten by an older seed value.
    if (const auto status = reader()) {
        applyStatus(*status);
    }

    m_subscription = subscriber([this](domain::ConnectivityStatus status) { applyStatus(status); });
    if (!m_subscription) {
        // Known deviation D-2 (docs/known_deviations.md): polls on the scheduler thread.
        spdlog::warn("Connectivity change notifications unavailable; falling back to polling");
        m_pollingReader = std::move(reader);
    }
}

ConnectivityCollector::ConnectivityCollector(ConnectivityReader reader)
    : m_pollingReader(std::move(reader))
{
}

domain::ConnectivityStatus ConnectivityCollector::collect()
{
    if (m_pollingReader) {
        applyStatus(m_pollingReader().value_or(domain::ConnectivityStatus{}));
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
