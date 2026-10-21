#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>

#include <QObject>

#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"

namespace verdandi::tests
{

/**
 * Starts scheduler, waits until it emits a snapshot for which isWanted
 * returns true, then stops scheduler and returns that snapshot, or
 * std::nullopt if none came within timeout. isWanted runs on the scheduler's
 * tick thread, once per snapshot. The timeout is only reached when a test
 * fails, so it allows for a busy machine.
 */
[[nodiscard]] inline std::optional<domain::SystemSnapshot>
waitForSnapshot(monitoring::SamplingScheduler& scheduler,
                const std::function<bool(const domain::SystemSnapshot&)>& isWanted,
                std::chrono::milliseconds timeout = std::chrono::seconds{5})
{
    std::mutex mutex;
    std::condition_variable found;
    std::optional<domain::SystemSnapshot> wanted;
    // Without a context object the lambda runs on the emitting thread, the tick thread.
    const QMetaObject::Connection connection = QObject::connect(
        &scheduler, &monitoring::SamplingScheduler::snapshotReady, [&](const domain::SystemSnapshot& snapshot) {
            if (!isWanted(snapshot)) {
                return;
            }
            const std::lock_guard lock(mutex);
            if (!wanted.has_value()) {
                wanted = snapshot;
                found.notify_all();
            }
        });

    scheduler.start();
    {
        std::unique_lock lock(mutex);
        found.wait_for(lock, timeout, [&wanted] { return wanted.has_value(); });
    }
    scheduler.stop();
    QObject::disconnect(connection);

    return wanted;
}

} // namespace verdandi::tests
