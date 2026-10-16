#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "domain/network_profile.h"
#include "monitoring/collector.h"
#include "platform/windows/connectivity_collector.h"

namespace sysmon::platform
{

/**
 * Reads the networks the system is connected to (the Network List Manager in
 * production). Returns std::nullopt on failure.
 */
using NetworkProfileReader = std::function<std::optional<std::vector<domain::NetworkProfile>>()>;

/**
 * Creates the reader on the thread that calls it. The reader is also destroyed
 * on that thread, so it can hold thread state, such as the production
 * reader's COM apartment.
 */
using NetworkProfileReaderFactory = std::function<NetworkProfileReader()>;

/**
 * Collector for the names and categories of the connected networks, from the
 * Network List Manager (INetworkListManager), matched to adapters by their
 * interface LUID (NetworkSample::interfaceId).
 *
 * The query is a COM call into a system service, which can take milliseconds
 * and has no time limit. So the collector reads on a worker thread of its own
 * (a std::jthread), and collect() only copies the latest result. A stuck
 * service then delays only the names, never the process list or connectivity
 * on the scheduler's slow-collector thread. The worker reads:
 * - at once when it starts,
 * - after a connectivity change notification (NotifyNetworkConnectivityHintChange),
 *   which Windows sends when the system joins or leaves a network, and
 * - once ReadIntervals::maximumAge has passed since the last read, which picks
 *   up a renamed network or a changed category, neither of which changes
 *   connectivity,
 * but never sooner than ReadIntervals::minimum after the last read began, so a
 * burst of notifications costs one read. A failed read is stored as
 * std::nullopt and retried at the next of these.
 *
 * It is an event-driven collector (docs/architecture.md, "Event-driven
 * collectors"): the notification callback only marks the names stale and
 * wakes the worker. The worker stores each result and, if it differs from the
 * previous one, calls the change listener, so the scheduler collects the new
 * names at once. The subscription is the last member, so it is cancelled first,
 * and the worker is stopped and joined next, before the state they share is
 * destroyed. Destruction waits for a read in progress. If subscribing fails,
 * the maximum age alone refreshes the names.
 */
class NetworkProfileCollector : public monitoring::INetworkProfileCollector
{
public:
    static constexpr std::chrono::milliseconds kMinimumReadInterval{1000};
    static constexpr std::chrono::milliseconds kMaximumAge{60'000};

    /** When the worker reads, after its first read. */
    struct ReadIntervals
    {
        /** The least time from the start of one read to the start of the next. */
        std::chrono::milliseconds minimum{kMinimumReadInterval};
        /** The most time from the start of one read to the start of the next. */
        std::chrono::milliseconds maximumAge{kMaximumAge};
    };

    /** Uses INetworkListManager and NotifyNetworkConnectivityHintChange. */
    NetworkProfileCollector();

    /** Injected reader factory, change subscriber, and intervals, for testing. */
    NetworkProfileCollector(NetworkProfileReaderFactory readerFactory, ConnectivitySubscriber subscriber,
                            ReadIntervals intervals = {});

    ~NetworkProfileCollector() override = default;

    // Not copyable or movable: the subscription's handler and the worker capture this.
    NetworkProfileCollector(const NetworkProfileCollector&) = delete;
    NetworkProfileCollector& operator=(const NetworkProfileCollector&) = delete;
    NetworkProfileCollector(NetworkProfileCollector&&) = delete;
    NetworkProfileCollector& operator=(NetworkProfileCollector&&) = delete;

    /** Returns the result of the latest read; std::nullopt before the first read finishes. */
    [[nodiscard]] std::optional<std::vector<domain::NetworkProfile>> collect() override;

    void setChangeListener(monitoring::ChangeListener listener) override;

    /** The number of reads that have finished, for tests and diagnostics. Thread-safe. */
    [[nodiscard]] std::uint64_t readCount() const;

private:
    void run(std::stop_token stopToken);

    // Waits until a read is due, given when the last one began. Returns false
    // if a stop was requested instead. lock holds m_mutex.
    bool waitUntilReadIsDue(std::unique_lock<std::mutex>& lock, const std::stop_token& stopToken,
                            std::optional<std::chrono::steady_clock::time_point> lastRead);

    // Marks the names stale and wakes the worker. Thread-safe; called from
    // notification callbacks.
    void markStale();

    NetworkProfileReaderFactory m_readerFactory;
    ReadIntervals m_intervals;

    // Guards everything below it but the worker and the subscription. The
    // listener is called with it held, so setChangeListener() waits for a call
    // in progress.
    mutable std::mutex m_mutex;
    std::condition_variable_any m_wake;
    bool m_isStale{false};
    std::optional<std::vector<domain::NetworkProfile>> m_latest;
    std::uint64_t m_readCount{0};
    monitoring::ChangeListener m_changeListener;

    // Declared after the state it uses, so it is stopped and joined first.
    std::jthread m_worker;

    // Declared last so it is destroyed first: notifications are cancelled
    // before the worker they wake and the state they write go away.
    std::unique_ptr<ConnectivitySubscription> m_subscription;
};

} // namespace sysmon::platform
