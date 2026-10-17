# Monitoring Module Rules

This module contains the metric collection infrastructure: collector interfaces, the sampling scheduler, snapshot aggregation, and health evaluation. Time-series history uses `domain::RingBuffer<T>`.

## Dependency constraints

- **No Windows headers.** Never include `<Windows.h>` or any Windows platform headers here. Concrete Windows collectors belong exclusively in `platform/windows/`.
- **No Qt GUI / Widgets headers.** May use `QObject` and Qt Core signals/slots to publish snapshots, but must not depend on `QtWidgets` or `QtGui`.
- **Can depend on `domain/`.** Uses typed domain metrics, process identities, and error types.

## Design and threading rules

- Schedulers manage the background threads they create using `std::jthread` with `std::stop_token` for clean shutdown.
- Fast collectors (CPU, memory, disk, network counters) run synchronously on the scheduler tick loop.
- Slow collectors (process enumeration, connectivity, network names) run on the scheduler's slow-collector thread at their own cadence (`setSlowInterval()`), and each tick merges their latest results into the snapshot.
- Event-driven collectors (OS callbacks, e.g. connectivity and network names) implement `IEventDrivenCollector` and return a cached value from `collect()`. They are still registered as slow collectors, so their queries and polling fallbacks never run on the tick. A collector whose query calls into another process, such as the Network List Manager, makes it on a worker thread of its own, so that the slow-collector thread only copies its result. The scheduler sets their change listener, which wakes the slow-collector thread to collect only the event-driven collectors when one reports a change; see [architecture.md — Event-driven collectors](../../docs/architecture.md#event-driven-collectors-os-callback-threads).
- Always protect shared state with `std::mutex` and `std::lock_guard` / `std::scoped_lock`. Keep critical sections minimal (copy under lock, process outside).
- Aggregate collected data into immutable `SystemSnapshot` objects timestamped with `std::chrono::steady_clock`.
- History kept here uses `domain::RingBuffer<T>` (fixed-capacity, caller-synchronized). Protect it with a mutex if it is shared across threads.
- Fake collectors for testing the pipeline without OS telemetry live with the tests (`tests/support/fake_collectors.h`, or the test file that alone uses one), not in this module.

