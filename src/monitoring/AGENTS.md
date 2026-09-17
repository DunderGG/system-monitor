# Monitoring Module Rules

This module contains the metric collection infrastructure: collector interfaces, the sampling scheduler, snapshot aggregation, and in-memory ring buffers.

## Dependency constraints

- **No Windows headers.** Never include `<Windows.h>` or any Windows platform headers here. Concrete Windows collectors belong exclusively in `platform/windows/`.
- **No Qt GUI / Widgets headers.** May use `QObject` and Qt Core signals/slots to publish snapshots, but must not depend on `QtWidgets` or `QtGui`.
- **Can depend on `domain/`.** Uses typed domain metrics, process identities, and error types.

## Design and threading rules

- Schedulers manage background threads using `std::jthread` with `std::stop_token` for clean shutdown.
- Fast collectors (CPU, memory, disk, network counters) run synchronously on the scheduler tick loop.
- Slow collectors (process enumeration, connectivity) run on dedicated threads at their own cadence and merge into snapshots asynchronously.
- Always protect shared state with `std::mutex` and `std::lock_guard` / `std::scoped_lock`. Keep critical sections minimal (copy under lock, process outside).
- Aggregate collected data into immutable `SystemSnapshot` objects timestamped with `std::chrono::steady_clock`.
- Bounded ring buffers (`RingBuffer<T>`) must be fixed-capacity, thread-safe or caller-synchronized, and never allocate dynamically on push after initialization.
- Provide fake/synthetic collectors here (or alongside tests) to enable testing the entire pipeline without OS telemetry dependencies.

