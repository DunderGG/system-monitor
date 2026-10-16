#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "domain/network_profile.h"
#include "monitoring/collector.h"
#include "platform/windows/connectivity_collector.h"
#include "platform/windows/steady_clock_reader.h"

namespace sysmon::platform
{

/**
 * Reads the networks the system is connected to (the Network List Manager in
 * production). Returns std::nullopt on failure.
 */
using NetworkProfileReader = std::function<std::optional<std::vector<domain::NetworkProfile>>()>;

/**
 * Collector for the names and categories of the connected networks, from the
 * Network List Manager (INetworkListManager), matched to adapters by their
 * interface LUID (NetworkSample::interfaceId).
 *
 * The query is a COM call into a system service, and its answer changes only
 * when the system joins or leaves a network, so collect() returns a cached
 * result and reads again only when:
 * - it has not read yet,
 * - a connectivity change notification (NotifyNetworkConnectivityHintChange)
 *   arrived since the last read began, which Windows sends when the system
 *   joins or leaves a network, or
 * - kMaxAge has passed since the last read, which picks up a renamed network
 *   or a changed category, neither of which changes connectivity.
 * A failed read is cached as std::nullopt, so it is retried at the next of
 * these rather than on every slow-collector refresh.
 *
 * It is an event-driven collector (docs/architecture.md, "Event-driven
 * collectors"): the notification callback only marks the cache stale and, if
 * it was not stale yet, calls the change listener, so the scheduler reads the
 * new names at once instead of at its next slow refresh. The subscription is
 * the last member, so it is cancelled before the state it writes is
 * destroyed. If subscribing fails, the maximum age alone refreshes the names.
 * The scheduler runs collect() on the slow-collector thread, so the query
 * never runs on the 1 Hz tick.
 */
class NetworkProfileCollector : public monitoring::INetworkProfileCollector
{
public:
    static constexpr std::chrono::seconds kMaxAge{60};

    /** Uses INetworkListManager and NotifyNetworkConnectivityHintChange. */
    NetworkProfileCollector();

    /** Injected reader, change subscriber, and clock, for unit testing. */
    NetworkProfileCollector(NetworkProfileReader reader, ConnectivitySubscriber subscriber,
                            SteadyClockReader clockReader);

    ~NetworkProfileCollector() override = default;

    // Not copyable or movable: the subscription's handler captures this.
    NetworkProfileCollector(const NetworkProfileCollector&) = delete;
    NetworkProfileCollector& operator=(const NetworkProfileCollector&) = delete;
    NetworkProfileCollector(NetworkProfileCollector&&) = delete;
    NetworkProfileCollector& operator=(NetworkProfileCollector&&) = delete;

    [[nodiscard]] std::optional<std::vector<domain::NetworkProfile>> collect() override;

    void setChangeListener(monitoring::ChangeListener listener) override;

private:
    // Marks the cache stale and calls the change listener if it was not stale
    // yet. Thread-safe; called from notification callbacks.
    void markStale();

    // Used only on the collecting thread.
    NetworkProfileReader m_reader;
    SteadyClockReader m_clockReader;
    std::optional<std::vector<domain::NetworkProfile>> m_cached;
    std::optional<std::chrono::steady_clock::time_point> m_lastRead;

    // Guards the stale mark and the listener. The listener is called with it
    // held, so setChangeListener() waits for a call in progress.
    std::mutex m_staleMutex;
    bool m_isStale{true};
    monitoring::ChangeListener m_changeListener;

    // Declared last so it is destroyed first: notifications are cancelled
    // before the state they write goes away.
    std::unique_ptr<ConnectivitySubscription> m_subscription;
};

} // namespace sysmon::platform
