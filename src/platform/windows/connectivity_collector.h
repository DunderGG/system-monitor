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
 * Reads the current connectivity synchronously (GetNetworkConnectivityHint in
 * production). Returns std::nullopt on failure.
 */
using ConnectivityReader = std::function<std::optional<domain::ConnectivityStatus>()>;

/** Receives each connectivity change. May be invoked on any thread. */
using ConnectivityChangeHandler = std::function<void(domain::ConnectivityStatus)>;

/**
 * An active change-notification registration. Destroying it cancels the
 * registration and must wait for any in-flight handler call to return, so the
 * handler is never invoked after destruction.
 */
class ConnectivitySubscription
{
public:
    virtual ~ConnectivitySubscription() = default;
};

/**
 * Registers handler for connectivity change notifications
 * (NotifyNetworkConnectivityHintChange in production). Returns nullptr if
 * registration fails. The handler may be invoked before this returns.
 */
using ConnectivitySubscriber = std::function<std::unique_ptr<ConnectivitySubscription>(ConnectivityChangeHandler)>;

/**
 * Collector for system-wide network connectivity status. An event-driven
 * collector (see docs/architecture.md, "Event-driven collectors"):
 * - Seeds the cache synchronously with the reader so the first collect() is
 *   accurate, then subscribes to change notifications. Seeding first means a
 *   notification can never be overwritten by an older seed value.
 * - Stores each notified status in a mutex-protected cache; collect() only
 *   returns the cached value on the notification path.
 * - Holds the subscription as its last member, so it is cancelled (waiting for
 *   in-flight callbacks) before the cache is destroyed.
 * - If subscribing fails, falls back to polling the reader on every collect().
 *
 * Missing data is explicit: before the first successful read, or after a failed
 * read, collect() returns ConnectivityLevel::Unknown with isMetered == std::nullopt.
 * collect() logs each change of status at info level, so notification callbacks
 * only store the value.
 */
class ConnectivityCollector : public sysmon::monitoring::IConnectivityCollector
{
public:
    /** Uses GetNetworkConnectivityHint and NotifyNetworkConnectivityHintChange. */
    ConnectivityCollector();

    /** Event-driven mode with injected reader and subscriber, for unit testing. */
    ConnectivityCollector(ConnectivityReader reader, ConnectivitySubscriber subscriber);

    /** Polling mode: the reader is called on every collect(). */
    explicit ConnectivityCollector(ConnectivityReader reader);

    ~ConnectivityCollector() override = default;

    // Not copyable or movable: the subscription's handler captures this.
    ConnectivityCollector(const ConnectivityCollector &) = delete;
    ConnectivityCollector &operator=(const ConnectivityCollector &) = delete;
    ConnectivityCollector(ConnectivityCollector &&) = delete;
    ConnectivityCollector &operator=(ConnectivityCollector &&) = delete;

    [[nodiscard]] domain::ConnectivityStatus collect() override;

private:
    // Stores the status in the cache and does nothing else. Thread-safe; called
    // from notification callbacks on OS thread-pool threads.
    void storeStatus(domain::ConnectivityStatus status);

    // Polled on every collect() in polling mode; empty on the notification path.
    ConnectivityReader m_pollingReader;

    // The last status collect() logged. Used only on the collecting thread.
    domain::ConnectivityStatus m_lastLoggedStatus;

    std::mutex m_mutex;
    domain::ConnectivityStatus m_cachedStatus;

    // Declared last so it is destroyed first: notifications are cancelled
    // before m_mutex and m_cachedStatus go away.
    std::unique_ptr<ConnectivitySubscription> m_subscription;
};

} // namespace sysmon::platform
