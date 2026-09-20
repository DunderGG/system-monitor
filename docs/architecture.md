# System Monitor Architecture

## Purpose

Build a lightweight Windows desktop system monitor inspired by Task Manager. The application will provide a clear view of system health, resource use, network connectivity, and running processes while keeping its own CPU, memory, and disk impact low.

## Technology stack

| Area | Choice | Rationale |
| --- | --- | --- |
| Language | C++20 | Team experience and direct, low-overhead access to Windows APIs. Leverage `std::jthread`, `std::stop_token`, `std::format`, concepts, and designated initializers. |
| Desktop UI | Qt 6 Widgets | Productive C++ UI development with strong model/view support. Widgets are the right fit for data-dense monitoring views; QML may be considered for future feature areas. |
| Charts | Custom `QWidget` sparklines | Task Manager-style rolling charts are simple polyline + gradient fills. A custom `QPainter` implementation is faster (<0.05 ms per frame), dependency-free, and avoids GPL licensing constraints of Qt Charts and QCustomPlot. |
| Build | CMake + CMake Presets | Standard, portable build configuration with good IDE support. Presets simplify contributor onboarding. |
| Dependencies | vcpkg manifest mode | All dependencies, including Qt 6, are declared in `vcpkg.json` for fully reproducible builds. Contributors clone the repo and build without manual dependency installation. |
| Logging | spdlog | High-performance structured logging with multiple sinks and size-based rotation. |
| Testing | GoogleTest | Familiar test framework. |
| Configuration | `nlohmann/json` | Simple, user-editable settings file. |
| Persistent history (later) | SQLite | Structured event history and time-series data once persistence is a deliberate feature. |

### Dependency management and contributor onboarding

All dependencies are managed through vcpkg manifest mode so that a contributor can clone the repository and build with a single CMake configure and build step. The `vcpkg.json` manifest and `CMakePresets.json` file together define the complete build environment.

Disable unnecessary default features for Qt to reduce build time:

```json
{
  "name": "system-monitor",
  "version-string": "0.1.0",
  "dependencies": [
    {
      "name": "qtbase",
      "default-features": false,
      "features": ["gui", "widgets", "network"]
    },
    "nlohmann-json",
    "gtest",
    "spdlog"
  ]
}
```

The first clean build will take 30–90 minutes while Qt compiles from source. Configure vcpkg binary caching (`VCPKG_DEFAULT_BINARY_CACHE`) so that subsequent clean builds and CI runs reuse the cached artifacts. Contributors who already have a local Qt 6 installation can override the vcpkg Qt package by setting `CMAKE_PREFIX_PATH` in their local CMake preset to skip the Qt build.

The initial MVP should use JSON plus bounded in-memory history. Do not add SQLite until persistent metric or event history is a product requirement.

## Scope and milestones

### MVP

1. Dashboard with CPU, memory, disk, network activity, uptime, and basic health status.
2. Performance views with live, rolling sparkline charts.
3. Process explorer with filtering, sorting, process tree, CPU, memory, I/O, PID, handles, and threads.
4. Network view with adapters, addresses, throughput, and a clearly defined connectivity state.
5. Safe process termination with an explicit confirmation step (unelevated processes only).

### Later features

- Elevated process termination via a UAC-prompted helper process.
- System tray icon with minimize-to-tray and quick status.
- Dark mode and theme support (light / dark / follow system).
- Alert rules and notification history.
- Persistent event and metric history.
- Startup application and service views.
- GPU monitoring.
- ETW-powered diagnostics and short diagnostic recording sessions.
- Per-process network bandwidth tracking (requires ETW).
- CSV/JSON export.

## System design

The UI must remain independent of the data-collection implementation. Collectors gather data in the background and publish immutable snapshots; presentation code renders those snapshots on the Qt UI thread.

```text
Qt 6 Widgets UI
  └─ View models / presentation state
       └─ Monitoring facade and snapshot subscriptions
            ├─ Sampling scheduler
            ├─ CPU collector
            ├─ Memory and storage collector
            ├─ Process collector
            ├─ Network collector
            ├─ Health/connectivity evaluator
            └─ In-memory time-series ring buffers
                 └─ Optional persistence adapter (future SQLite)

Windows integration layer (platform/windows)
  ├─ GetSystemTimes / NtQuerySystemInformation  (CPU)
  ├─ GlobalMemoryStatusEx / GetDiskFreeSpaceEx   (memory, disk)
  ├─ NtQuerySystemInformation                    (process list, per-process metrics)
  ├─ QueryFullProcessImageNameW                  (process paths, cached)
  ├─ GetIfTable2 / GetAdaptersAddresses          (network adapters, traffic)
  ├─ GetNetworkConnectivityHint                  (connectivity status)
  └─ WMI/CIM one-shot queries at startup only    (hardware inventory)
```

### Core modules

| Module | Responsibility |
| --- | --- |
| `app` | Application startup, dependency wiring, settings, logging configuration, and lifetime management. |
| `ui` | Qt Widgets, custom sparkline chart widgets, tables, and view models. Contains no direct Windows API calls. |
| `monitoring` | Collector interfaces, scheduler, snapshot aggregation, sampling policy, and metric ring buffers. |
| `platform/windows` | Narrow wrappers around Windows APIs and conversion into application domain types. |
| `domain` | Typed metrics, process identity, health state, error types, and commands. Free of Qt and Windows dependencies. |
| `persistence` | JSON settings initially; a repository interface and SQLite implementation when durable history is added. |
| `tests` | Unit and integration tests. |

### Threading model

```text
┌─────────────────────────────────────────────────┐
│                UI Thread (Qt)                    │
│  - Renders widgets, sparklines, process table    │
│  - Handles user interaction                      │
│  - Updates view models from received snapshots   │
└────────────────────▲────────────────────────────-┘
                     │ Qt::QueuedConnection (SystemSnapshot)
┌────────────────────┴────────────────────────────-┐
│          Scheduler Thread (std::jthread)          │
│  - Owns the sampling tick loop                   │
│  - Runs fast collectors inline:                  │
│      CPU (~0.01 ms), memory (~0.01 ms),          │
│      network counters (~0.1 ms)                  │
│  - Merges latest slow-collector results          │
│  - Assembles immutable SystemSnapshot            │
│  - Emits snapshot to UI thread via signal        │
│  - Uses std::stop_token for clean shutdown       │
└────────────────────▲────────────────────────────-┘
                     │ Async merge (latest result, mutex-protected)
┌────────────────────┴────────────────────────────-┐
│        Slow Collector Thread (std::jthread)       │
│  - Process enumeration via NtQuerySystemInfo     │
│    (~0.5–1.5 ms, but includes path cache lookup) │
│  - Connectivity probes                           │
│  - Runs at own cadence (e.g. every 2–5 seconds)  │
│  - Posts results for scheduler to pick up         │
│  - Uses std::stop_token for clean shutdown       │
└──────────────────────────────────────────────────┘
```

Key threading rules:

- Fast collectors (CPU, memory, disk, network counters) run synchronously on the scheduler tick. They complete in under 1 ms combined.
- Slow collectors (process enumeration, connectivity probes) run on separate `std::jthread`s at their own cadence. The scheduler always emits the latest available data, even if a slow collector has not refreshed yet.
- Snapshots cross from the scheduler thread to the UI thread via `Qt::QueuedConnection`. The snapshot is an immutable value type; no shared mutable state crosses the boundary.
- All background threads use `std::stop_token` for cooperative cancellation at shutdown.

### Data flow

1. The scheduler thread sleeps for the configured interval (default 1 second), then wakes and calls fast collectors for aggregate CPU, memory, disk, and network samples.
2. Fast collectors obtain data from the Windows integration layer and return typed results, including any access or availability errors.
3. The scheduler merges the latest results from slow collectors (process list, connectivity) with the fresh fast-collector samples into an immutable `SystemSnapshot` timestamped with `std::chrono::steady_clock`.
4. Bounded ring buffers retain recent samples for sparkline charts.
5. The scheduler emits the snapshot as a Qt signal. View models receive it via queued connection on the UI thread and update Qt item models.
6. Optional persistence receives batched writes on a background worker; it must never block sampling or rendering.

## Sampling and storage policy

- Use a single scheduler thread rather than a timer per chart or widget.
- Sample aggregate CPU, memory, and network activity about once per second.
- Refresh the process table at a lower cadence (every 2–5 seconds). With `NtQuerySystemInformation` this takes only 0.5–1.5 ms, but the process view model diff/update is more expensive and does not need per-second refresh.
- Calculate rates (CPU %, network throughput) using monotonic elapsed time from `std::chrono::steady_clock`, not wall-clock time.
- Keep a fixed-size ring buffer per metric (for example, 300–1800 samples for 5–30 minutes at 1 Hz).
- Do not write every sample to disk in the MVP.
- If SQLite is added, batch background inserts every 10–30 seconds and downsample or expire older data.

## Windows API choices

| Metric / function | API used |
| --- | --- |
| Total CPU | `GetSystemTimes` |
| Per-core CPU | `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` |
| Physical memory | `GlobalMemoryStatusEx` |
| Disk space | `GetDiskFreeSpaceExW` |
| Network throughput | `GetIfTable2` (64-bit counters; release with `FreeMibTable`) |
| Network addresses | `GetAdaptersAddresses` |
| Connectivity status | `GetNetworkConnectivityHint` / `NotifyNetworkConnectivityHintChange` |
| Process list + metrics | `NtQuerySystemInformation(SystemProcessInformation)` |
| Executable paths | `QueryFullProcessImageNameW` (lazy, cached by PID + creation time) |
| Command lines | `NtQueryInformationProcess(ProcessCommandLineInformation)` (lazy, cached) |
| Network profile names | `INetworkListManager` (COM, lazy, on change events only) |
| Hardware inventory | WMI `Win32_ComputerSystem` / `Win32_Processor` (startup only) |
| Per-disk I/O rates | PDH (only where no direct Win32 alternative exists) |

See [`design_decisions.md`](design_decisions.md) for the rationale behind each choice, including why `NtQuerySystemInformation` is preferred over Tool Help, why PDH is avoided for core metrics, and why ETW and WMI are restricted.

## Domain design guidelines

- Model resource data with explicit units: bytes, bytes/sec, percent, milliseconds, and counts.
- Represent missing and inaccessible data explicitly (e.g. `std::optional`, error variants) rather than substituting zero.
- Identify processes with `PID + creation time`, not PID alone, because Windows reuses PIDs.
- Return errors as normal sample metadata so one inaccessible process or counter does not break the whole view.
- Separate read-only monitoring commands from privileged operations such as terminating a process.
- Keep the process action surface small and require confirmation for destructive actions.

## Sparkline chart design

Implement Task Manager-style rolling charts as custom `QWidget` subclasses:

- Override `paintEvent` and draw with `QPainter`.
- Render a `QPainterPath` polyline from the ring buffer data (typically 60–300 points for 1–5 minutes).
- Fill below the line with a `QLinearGradient` for the characteristic Task Manager look.
- Draw a scrolling grid background by offsetting grid lines on each repaint.
- At 1 Hz refresh with <300 points, each repaint takes well under 1 ms. Double-buffering via `QPixmap` is unlikely to be needed but available if profiling shows benefit.
- This avoids third-party charting library dependencies and GPL licensing constraints.

## Configuration and persistence

Store small user preferences in a JSON settings file, including:

- refresh interval;
- preferred display units;
- enabled dashboard panels and layout;
- alert thresholds and whether alerts are enabled;
- log level.

Use `nlohmann/json` through vcpkg. Keep configuration access behind a small settings service so validation, defaults, and future migrations are centralized.

When durable metric history or an event log becomes a feature, add SQLite behind an interface such as `IMetricsStore`. This lets monitoring code remain agnostic to whether data is held only in memory, exported, or persisted.

## Logging

Use spdlog through vcpkg. Configure in the `app` module at startup.

| Level | Usage |
| --- | --- |
| `trace` | Sampling timing details, raw API return values. |
| `debug` | Collector lifecycle, cache hits/misses, thread start/stop. |
| `info` | Application startup, settings loaded, connectivity state changes. |
| `warn` | Process access denied, counter unavailable, connectivity probe timeout. |
| `error` | Collector failure, API error codes, unrecoverable configuration issues. |

Rotate log files by size (5 MB × 3 files). Make the log level configurable in the settings file.

## Elevation and security

### Run unelevated by default

Set `requestedExecutionLevel level="asInvoker"` in the application manifest. With `NtQuerySystemInformation`, standard users get complete CPU, memory, and I/O metrics for all processes without elevation. This is a significant advantage of the chosen API approach.

### Elevated actions (future)

When elevated process termination is added, launch a small helper executable via `ShellExecuteEx` with verb `L"runas"`. This shows a standard UAC dialog for just the single action. Do not require the entire application to run elevated. Show a shield icon next to actions that will trigger a UAC prompt.

### Antivirus and EDR considerations

- **Only request `PROCESS_QUERY_LIMITED_INFORMATION`** when opening process handles. Never use `PROCESS_ALL_ACCESS` or `PROCESS_VM_READ | PROCESS_VM_WRITE`.
- **Never call `OpenProcess` on `lsass.exe` or `csrss.exe`.** This triggers credential-dumping heuristic detections in most EDR products, even with benign access masks. Use the data already returned by `NtQuerySystemInformation` for these processes.
- Calling `NtQuerySystemInformation` from `ntdll.dll` via `GetProcAddress` is standard behavior used by legitimate system tools and does not trigger AV flags.
- Sign the binary (even self-signed during development) to avoid SmartScreen reputation warnings.
- Avoid process injection, API hooking, and raw syscall stubs.

## High-DPI support

Enable per-monitor DPI awareness in the application manifest. Qt 6 handles most scaling automatically, but verify icon and resource sizing. Test on 100%, 125%, 150%, and 200% scaling. This is a build-time configuration item, not a runtime feature toggle.

## Sleep and resume handling

After system sleep or hibernate, the first sample will have a large elapsed-time delta, producing incorrect rate calculations (CPU %, network throughput).

Mitigation: listen for `WM_POWERBROADCAST` / `PBT_APMRESUMEAUTOMATIC` events. On resume, discard the first sample and reset all rate-calculation baselines in every collector.

## Testing strategy

Use GoogleTest and structure code so operating-system calls are behind interfaces or small adapters.

- Unit test rate calculations, normalization, ring-buffer retention, health-state decisions, JSON settings validation, and view-model transformations.
- Use fake collectors to test scheduling and snapshot aggregation without relying on the host machine's state.
- Add integration tests for selected Windows adapters, tolerant of unavailable counters and permissions.
- Test Qt item models separately from widgets where possible.
- Include regression tests for PID reuse, elapsed-time gaps after sleep, denied process access, network state transitions, and `NtQuerySystemInformation` buffer resizing.
- Consider adding Google Benchmark for regression-testing sampling overhead.

## Principal risks and mitigations

| Risk | Mitigation |
| --- | --- |
| The monitor consumes too many resources | Use shared sampling, bounded buffers, adaptive process refresh, and profiling on lower-end systems. |
| Incorrect or noisy CPU/rate readings | Normalize multi-core CPU usage consistently and use measured monotonic intervals via `std::chrono::steady_clock`. |
| PID reuse corrupts process history | Pair PID with creation time from `NtQuerySystemInformation`. |
| Protected processes deny access | `NtQuerySystemInformation` returns full metrics for all processes even unelevated. Only path/command-line queries may be denied; display partial data with clear indicators. |
| PDH registry corruption | Core metrics use direct Win32 APIs, not PDH. PDH is used only for advanced counters and handles missing counters gracefully. |
| PDH process instance shifting | Per-process data comes from `NtQuerySystemInformation`, never PDH. |
| Ambiguous connectivity status | Use `GetNetworkConnectivityHint` for structured connectivity levels (None, LocalAccess, ConstrainedInternetAccess, InternetAccess) rather than a single "online" flag. |
| UI stutters under a large process list | Diff/update Qt item models efficiently. All collection and aggregation happens off the UI thread. Snapshots cross to UI via queued signals. |
| Unbounded memory or disk usage | Use fixed-size in-memory ring buffers and explicit persistence retention/downsampling policies. |
| Excessive privilege or security risk | Run unelevated by default. Only request `PROCESS_QUERY_LIMITED_INFORMATION`. Never open `lsass.exe` or `csrss.exe` handles. |
| Antivirus/EDR false positives | Avoid dangerous access masks, sensitive process handles, API hooking, and process injection. Sign binaries. |
| Sleep/hibernate causes rate calculation errors | Detect `WM_POWERBROADCAST` resume events. Discard first sample and reset rate baselines. |
| WMI service not running or repository corrupted | Use WMI only for optional startup hardware inventory. All core metrics use direct Win32 APIs. |
| Qt event loop starvation | Keep snapshot emission at 1 Hz. Batch model updates. Profile early. |
| ETW session exhaustion (future) | System has a hard 64-session limit shared with EDR. Defer ETW to post-MVP and handle `ERROR_NO_SYSTEM_RESOURCES` from `StartTrace`. |
| 32-bit network counter rollover | Use `GetIfTable2` which provides 64-bit counters. Never use legacy `GetIfTable`. |
