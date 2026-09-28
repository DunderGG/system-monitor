#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "domain/connectivity_status.h"
#include "monitoring/collector.h"

namespace sysmon::platform
{

/**
 * Reader callback type — injected in unit tests instead of calling
 * GetNetworkConnectivityHint directly. Returns std::nullopt on failure.
 */
using ConnectivityReader = std::function<std::optional<domain::ConnectivityStatus>()>;

/**
 * Collector for system-wide network connectivity status.
 *
 * Windows implementation:
 * - Seeds the cache synchronously with GetNetworkConnectivityHint so the first
 *   collect() is accurate, then registers NotifyNetworkConnectivityHintChange to
 *   receive a callback on every subsequent change.
 * - Stores the latest ConnectivityStatus in a mutex-protected cache updated from
 *   the system callback thread. collect() returns the cached value and never
 *   performs a kernel round-trip on the notification path.
 * - The notification is cancelled via CancelMibChangeNotify2 before the cache is
 *   destroyed, preventing callbacks after object teardown.
 *
 * If notification registration fails at runtime, collect() falls back to calling
 * GetNetworkConnectivityHint synchronously on every tick.
 *
 * Missing data is explicit: before the first successful read, or after a failed
 * read, collect() returns ConnectivityLevel::Unknown with isMetered == std::nullopt.
 * Each change of status is logged at info level.
 */
class ConnectivityCollector : public sysmon::monitoring::IConnectivityCollector
{
public:
    /** Default constructor using live Windows connectivity APIs. */
    ConnectivityCollector();

    /**
     * Injected constructor for deterministic unit testing.
     * The reader is called on every collect() invocation.
     */
    explicit ConnectivityCollector(ConnectivityReader reader);

    ~ConnectivityCollector() override = default;

    ConnectivityCollector(const ConnectivityCollector &) = delete;
    ConnectivityCollector &operator=(const ConnectivityCollector &) = delete;
    ConnectivityCollector(ConnectivityCollector &&) = delete;
    ConnectivityCollector &operator=(ConnectivityCollector &&) = delete;

    [[nodiscard]] domain::ConnectivityStatus collect() override;

private:
    // Hosts the OS callback; defined in the .cpp to keep Windows types out of this header.
    struct NotificationBridge;

    struct NotificationHandleDeleter
    {
        void operator()(void *handle) const noexcept;
    };

    using NotificationHandle = std::unique_ptr<void, NotificationHandleDeleter>;

    // Stores the status and logs if it differs from the previous one. Thread-safe.
    void applyStatus(domain::ConnectivityStatus status);

    // Polled on every collect() — the injected reader, or the synchronous
    // fallback when notification registration failed. Empty on the notification path.
    ConnectivityReader m_reader;

    std::mutex m_mutex;
    domain::ConnectivityStatus m_cachedStatus;

    // Declared last so it is destroyed first: the notification is cancelled
    // before m_mutex and m_cachedStatus go away.
    NotificationHandle m_notificationHandle;
};

} // namespace sysmon::platform
