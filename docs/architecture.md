# System Monitor Architecture

## Purpose

Build a lightweight Windows desktop system monitor inspired by Task Manager. The application will provide a clear view of system health, resource use, network connectivity, and running processes while keeping its own CPU, memory, and disk impact low.

## Technology stack

| Area | Choice | Rationale |
| --- | --- | --- |
| Language | C++20 | Team experience and direct, low-overhead access to Windows APIs. Leverage `std::jthread`, `std::stop_token`, `std::format`, concepts, and designated initializers. |
| Desktop UI | Qt 6 Widgets | Productive C++ UI development with strong model/view support. Widgets are the right fit for data-dense monitoring views; QML may be considered for future feature areas. |
| Charts | Custom `QWidget` sparklines | Task Manager-style rolling charts are simple polyline + gradient fills. A custom `QPainter` implementation is dependency-free, paints only what the charts need, and avoids GPL licensing constraints of Qt Charts and QCustomPlot. [performance.md](performance.md) records what a tick of each page costs, repaint included. |
| Build | CMake + CMake Presets | Standard, portable build configuration with good IDE support. Presets simplify contributor onboarding. |
| Dependencies | vcpkg manifest mode | All dependencies, including Qt 6, are declared in `vcpkg.json` for fully reproducible builds. Contributors clone the repo and build without manual dependency installation. |
| Logging | spdlog | High-performance structured logging with multiple sinks and size-based rotation. |
| Testing | GoogleTest | Familiar test framework. |
| Configuration | `nlohmann/json` | Simple, user-editable settings file. |
| Persistent history (later) | SQLite | Structured event history and time-series data once persistence is a deliberate feature. |

### Dependency management and contributor onboarding

All dependencies are managed through vcpkg manifest mode so that a contributor can clone the repository and build with a single CMake configure and build step. The `vcpkg.json` manifest and `CMakePresets.json` file together define the complete build environment.

Disable unnecessary default features for Qt to reduce build time. Keep `png`: Qt's Windows platform plugin builds its split, hand, and drag cursors from PNG resources ([DC-86](design_decisions.md#dc-86)).

```json
{
  "name": "system-monitor",
  "version-string": "0.1.0",
  "dependencies": [
    {
      "name": "qtbase",
      "default-features": false,
      "features": ["gui", "network", "png", "widgets"]
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
  ├─ SetupDiGetClassDevsW                        (hidden adapters, on a new interface)
  ├─ GetNetworkConnectivityHint                  (connectivity status)
  ├─ INetworkListManager                         (network names, lazy, on its own thread)
  └─ WMI/CIM one-shot queries at startup only    (hardware inventory)
```

### Core modules

| Module | Responsibility |
| --- | --- |
| `app` | Application startup, dependency wiring, settings, logging configuration, and lifetime management. |
| `ui` | Qt Widgets, custom sparkline chart widgets, tables, and view models. Contains no direct Windows API calls. |
| `monitoring` | Collector interfaces, scheduler, snapshot aggregation, sampling policy, and health evaluation. Metric history uses `domain::RingBuffer`. |
| `platform/windows` | Narrow wrappers around Windows APIs and conversion into application domain types. Its collectors implement the interfaces in `monitoring/collector.h` (CMake target `sysmon_collector_interfaces`), its only dependency on `monitoring`. |
| `domain` | Typed metrics, process identity, health state, error types, commands, and the user's settings, plus the `RingBuffer<T>` container for time-series history. Free of Qt and Windows dependencies. |
| `persistence` | The JSON settings file initially; a repository interface and SQLite implementation when durable history is added. Free of Qt and Windows dependencies. |
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
│      network counters (~0.7 ms)                  │
│  - Merges latest slow-collector results          │
│  - Assembles immutable SystemSnapshot            │
│  - Emits snapshot to UI thread via signal        │
│  - Uses std::stop_token for clean shutdown       │
└────────────────────▲────────────────────────────-┘
                     │ Async merge (latest result, mutex-protected)
┌────────────────────┴────────────────────────────-┐
│        Slow Collector Thread (std::jthread)       │
│  - Process enumeration via NtQuerySystemInfo     │
│    (~3.5–6 ms for ~400 processes; the first      │
│    pass, which reads every path, ~18 ms)         │
│  - Connectivity (notification cache, or polling  │
│    fallback; see event-driven collectors below)  │
│  - Network names (a copy of the worker's latest  │
│    read, below)                                  │
│  - Runs at own cadence (default every 2 seconds) │
│  - Woken early to collect connectivity and       │
│    network names when either changes             │
│  - Posts results for scheduler to pick up         │
│  - Uses std::stop_token for clean shutdown       │
└────────────────────▲────────────────────────────-┘
                     │ Latest names (mutex-protected), then a wake-up
┌────────────────────┴────────────────────────────-┐
│   Network-name worker (std::jthread, owned by    │
│   NetworkProfileCollector)                       │
│  - Reads the names via INetworkListManager (COM, │
│    ~4.5–6 ms; the first read in the process      │
│    ~11 ms), at start, after a connectivity       │
│    change, or once a minute, and at most once a  │
│    second                                        │
│  - Holds a multithreaded COM apartment for its   │
│    lifetime                                      │
│  - Uses std::stop_token for clean shutdown       │
└──────────────────────────────────────────────────┘
```

Key threading rules:

- Fast collectors (CPU, memory, disk, network counters) run synchronously on the scheduler tick. They complete in about 1 ms combined; `GetIfTable2` dominates, and its cost grows with the number of interfaces (about 0.7 ms for 42 rows on a desktop). Adapter names, addresses, and DNS servers (`GetAdaptersAddresses`, about 2 ms) are cached and re-read only on change.
- Slow collectors (process enumeration, connectivity, and network names) run on the scheduler's slow-collector `std::jthread`, every 2 seconds, or every refresh interval when the settings choose a longer one (`SamplingScheduler::setSlowInterval()`, [DC-127](design_decisions.md#dc-127)). Each run stores its results under a mutex, and every tick copies the latest ones into the snapshot. The scheduler always emits the latest available data, even if a slow collector has not refreshed yet. Until the first slow refresh, the snapshot has no process list, no network names, and `ConnectivityLevel::Unknown`. The network names come from a COM query into the Network List Service, which has no time limit. `NetworkProfileCollector` makes it on a worker thread of its own, only after a connectivity change or once its answer is a minute old, and at most once a second, so the slow refreshes only copy its latest answer. A stuck service then delays only the names, not the process list or connectivity, although shutdown still waits for a read in progress ([DC-100](design_decisions.md#dc-100)).
- Event-driven collectors receive changes from the OS instead of polling (see below), so their `collect()` returns a cached value. Connectivity and the network names are event-driven, and are still registered as slow collectors so that their queries and the connectivity polling fallback never run on the tick. When one of them stores a change, it calls its change listener, which wakes the slow-collector thread to collect only these two at once. The change reaches the next tick's snapshot, about a second after the notification at most, instead of waiting for the next slow refresh. The process list keeps its own cadence, and no extra snapshot is emitted, so every history keeps one sample per tick ([DC-77](design_decisions.md#dc-77)).
- Snapshots cross from the scheduler thread to the UI thread via `Qt::QueuedConnection`. The snapshot is an immutable value type; no shared mutable state crosses the boundary.
- All background threads the application creates are `std::jthread`s and use `std::stop_token` for cooperative cancellation at shutdown.

#### Event-driven collectors (OS callback threads)

Some Windows APIs deliver change notifications by invoking a callback on a thread from the OS thread pool, for example `NotifyNetworkConnectivityHintChange`. The application does not own these threads, so the `std::jthread` rule cannot apply to them. A collector may use such a callback instead of a polling thread when it follows all of these rules:

1. **Store, then wake.** The callback converts the notification to a domain value and stores it in mutex-protected state (`std::lock_guard`). If the value changed, it then calls the collector's change listener (`monitoring::IEventDrivenCollector::setChangeListener()`), which the scheduler sets. The listener only sets a flag and notifies a condition variable. The callback does no other blocking or long-running work, and it never touches Qt objects or emits signals.
2. **Cancel before teardown.** The registration handle is an RAII member whose deleter cancels the registration with an API that waits for in-flight callbacks (e.g. `CancelMibChangeNotify2`). The handle is declared after the state the callback writes, so it is destroyed first.
3. **Seed synchronously.** The constructor reads the current value synchronously before registering, so the first `collect()` does not depend on callback timing.
4. **Read from the cache.** `collect()` only copies the cached value under the mutex; it makes no OS calls.
5. **Fall back to polling.** If registration fails, the collector polls on the slow-collector thread instead.

`ConnectivityCollector` is the reference implementation.

When the value is costly to read, a notification can instead mark a cache stale, so that the value is read again on a thread the application owns. The callback still follows rule 1, storing only the stale mark, and the registration is still cancelled first (rule 2). An expiry age replaces rule 5, because it keeps the value current when registration fails. There are two forms:

- **A worker of the collector's own**, for a read that calls into another process. `NetworkProfileCollector` owns a `std::jthread` that reads at start, when the names are stale, and when they have expired, but never sooner than a minimum interval after its last read. The callback marks the names stale and wakes the worker. The worker stores each result and, if it differs from the previous one, calls the change listener. `collect()` only copies the latest result (rule 4), and before the first read finishes it returns no names, in place of rule 3. The registration is declared last and the worker before it, so notifications are cancelled first and the worker is joined next, before the state they share is destroyed.
- **The collecting thread**, for a cheap in-process read. The cache starts stale, so the first `collect()` reads, and `collect()` reads again only when the cache is stale or has expired. The network collector's adapter details (`AdapterDetailsCache`) follow this form. They have no change listener, because the network collector is a fast collector, collected on every tick anyway.

### Data flow

1. The scheduler thread sleeps for the configured interval (default 1 second), then wakes and calls fast collectors for aggregate CPU, memory, disk, and network samples.
2. Fast collectors obtain data from the Windows integration layer and return typed results, including any access or availability errors.
3. The scheduler merges the latest results from slow collectors (process list, connectivity, network names) with the fresh fast-collector samples into an immutable `SystemSnapshot` timestamped with `std::chrono::steady_clock`.
4. Bounded ring buffers (`domain::RingBuffer`) retain recent samples for sparkline charts. The dashboard keeps its own short presentation history for its mini sparklines, the Network tab keeps a minute of throughput per adapter, and each Performance tab page keeps the history for its charts; histories needed outside the UI (e.g. for alert rules) may live in `monitoring`.
5. The scheduler emits the snapshot as a Qt signal. View models receive it via queued connection on the UI thread and update Qt item models.
6. Optional persistence receives batched writes on a background worker; it must never block sampling or rendering.

## Sampling and storage policy

- Use a single scheduler thread rather than a timer per chart or widget.
- Sample aggregate CPU, memory, and network activity about once per second.
- Refresh the process table at a lower cadence (every 2–5 seconds; the default is 2). With `NtQuerySystemInformation` this takes about 3.5–6 ms for 400 processes, most of it in the call itself ([DC-75](design_decisions.md#dc-75)), so it does not need per-second refresh. The process table model signals only the cells that changed, so a snapshot that repeats the previous list changes little ([DC-79](design_decisions.md#dc-79)); [performance.md](performance.md) records what applying a list and repainting the table cost.
- Calculate rates (CPU %, network throughput) using monotonic elapsed time from `std::chrono::steady_clock`, not wall-clock time.
- Keep a fixed-size ring buffer per metric (for example, 300–1800 samples for 5–30 minutes at 1 Hz).
- Do not write every sample to disk in the MVP.
- If SQLite is added, batch background inserts every 10–30 seconds and downsample or expire older data.

## Windows API choices

| Metric / function | API used |
| --- | --- |
| Total CPU | `GetSystemTimes` |
| Per-core CPU | `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` |
| CPU base speed | `CallNtPowerInformation(ProcessorInformation)` (once at startup) |
| Physical memory | `GlobalMemoryStatusEx` |
| Memory cache and kernel pools; process, thread, and handle counts | `GetPerformanceInfo` |
| Disk space | `GetDiskFreeSpaceExW` |
| Network throughput | `GetIfTable2` (64-bit counters; release with `FreeMibTable`) |
| Network addresses | `GetAdaptersAddresses` |
| Adapters Windows hides | SetupAPI (`SetupDiGetClassDevsW`, `SetupDiOpenDevRegKey`): the `NCF_HIDDEN` characteristic in each network adapter's driver key, read when an interface appears |
| Connectivity status | `GetNetworkConnectivityHint` / `NotifyNetworkConnectivityHintChange` |
| Process list + metrics | `NtQuerySystemInformation(SystemProcessInformation)` |
| Executable paths | `QueryFullProcessImageNameW` (lazy, cached by PID + creation time) |
| Command lines | `NtQueryInformationProcess(ProcessCommandLineInformation)` (lazy, cached) |
| Process owner (before termination) | `OpenProcessToken` + `GetTokenInformation(TokenUser)`, compared with `EqualSid` |
| Process elevation (before termination) | `GetTokenInformation(TokenElevation)` + `GetTokenInformation(TokenIntegrityLevel)` |
| Process termination | `TerminateProcess` (unelevated processes the current user owns only) |
| Network profile names | `INetworkListManager` (COM, lazy: after a connectivity change, or once a minute, and at most once a second, on the collector's own thread) |
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
- Render a polyline from the ring buffer data: 60, 300, or 1800 points per series for the 1, 5, and 30-minute chart windows.
- When a chart has more points than pixel columns, keep each column's lowest and highest point and draw a one-pixel cosmetic line, so the cost is bounded by the chart's width ([DC-48](design_decisions.md#dc-48)).
- Fill below the line with a `QLinearGradient` for the characteristic Task Manager look.
- Draw a scrolling grid background by offsetting grid lines on each repaint.
- `.\scripts\build.ps1 -Benchmark` measures a 1 Hz tick in a Release build with worst-case data: recording a snapshot, refreshing the page, and repainting the window ([DC-66](design_decisions.md#dc-66)). [performance.md](performance.md) records the figures for the Performance, Network and Processes tabs, with the build and the machine they were measured on. Double-buffering via `QPixmap` has not been needed.
- This avoids third-party charting library dependencies and GPL licensing constraints.

## Configuration and persistence

Store small user preferences in a JSON settings file. It holds:

- the refresh interval, from 0.5 to 10 seconds in steps of half a second;
- the display units: binary (KiB) or decimal (KB) byte units, and network throughput in bytes or bits per second;
- which dashboard panels are shown;
- the log level.

The dashboard's layout, and alert thresholds and whether alerts are enabled, can join it when those features exist.

Use `nlohmann/json` through vcpkg. Keep configuration access behind a small settings service so validation, defaults, and future migrations are centralized. `domain::Settings` holds the values, and its defaults are what the monitor does without a file. The `persistence` module's `settings_file.h` writes and reads the file. It reads one setting at a time, so a missing or invalid setting keeps its default without costing the others, and it returns each problem for the caller to log, because the log level is itself a setting ([DC-123](design_decisions.md#dc-123)). The file is `settings.json` in the user's roaming application data folder (`QStandardPaths::AppDataLocation`), while the log stays in the local one. `main.cpp` reads it before it configures the logger, then logs what it found, and passes the settings to the window and to `app::SettingsService`, the settings service, which applies those that are not the UI's, the refresh interval to the scheduler and the log level to the logger, and saves the file on each change. A file with problems is left as it is until the user changes a setting ([DC-127](design_decisions.md#dc-127)). The user edits the settings in `ui::SettingsDialog`, which the main window's File menu opens on a copy of the window's settings. Nothing changes until OK; then, if the settings differ, `MainWindow` applies what its views show of them, as `setSettings()` does, and emits `settingsChanged()`, for `app` to save them and apply the rest, since `ui` may not depend on `persistence` ([DC-124](design_decisions.md#dc-124)). The charts' history is counted in samples at every refresh interval, and their time labels follow the interval ([DC-61](design_decisions.md#dc-61)).

When durable metric history or an event log becomes a feature, add SQLite behind an interface such as `IMetricsStore`. This lets monitoring code remain agnostic to whether data is held only in memory, exported, or persisted.

## Logging

Use spdlog through vcpkg. Configure in the `app` module at startup.

| Level | Usage |
| --- | --- |
| `trace` | Sampling timing details, raw API return values. |
| `debug` | Collector lifecycle, cache hits/misses, thread start/stop, a process denying a details query (routine when unelevated). |
| `info` | Application startup, settings loaded, connectivity state changes. |
| `warn` | Process action denied (e.g. termination), counter unavailable, connectivity probe timeout, a value in the settings file that is not used as it stands. |
| `error` | Collector failure, API error codes, unrecoverable configuration issues. |

Rotate log files by size (5 MB × 3 files). The settings file holds the log level, which `--log-level` replaces for one run without saving it ([DC-127](design_decisions.md#dc-127)).

Shut the logger down last. `main()` destroys the window, the scheduler and the collectors before it calls `spdlog::shutdown()`, because their teardown can log, and once the default logger has been dropped a log call dereferences a null pointer.

## Elevation and security

### Run unelevated by default

Set `requestedExecutionLevel level="asInvoker"` in the application manifest. With `NtQuerySystemInformation`, standard users get complete CPU, memory, and I/O metrics for all processes without elevation. This is a significant advantage of the chosen API approach.

### Unelevated process termination

The monitor terminates only processes that the current user owns and that do not run elevated. The user of the process's token must be the user of the monitor's own token. The token must not be elevated, and its integrity level must be below high, so system processes, services, and UIAccess programs are refused too. This holds even when the monitor itself runs elevated. `platform::terminateOwnedProcess()` takes the process's `(PID, creation time)` identity. It opens the process with `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE`, checks the creation time, and reads the token to check the owner and the elevation. Only then does it open the process again with `PROCESS_TERMINATE` added and call `TerminateProcess`. A process whose PID was reused is never touched, and `PROCESS_TERMINATE` is never requested for another user's process or for an elevated one. It returns a `domain::ProcessTerminationResult` that says why a process was left alone, so the UI can explain it ([DC-87](design_decisions.md#dc-87)).

The Processes tab asks first. Its End process action (a toolbar button, the table's context menu, or the Delete key) opens a dialog that names the process and its PID and warns that unsaved data is lost, with Cancel as the default. Confirming ends the process the dialog named, by its identity, even if the table changed while the dialog was open. The UI calls the function through a `ui::ProcessTerminator` that `main.cpp` sets, on the UI thread, because it does not wait for the process to exit. Once the dialog has closed, a message box says why a process was left running, or that it had already exited. A process that was ended gets no message, because its row is marked as exited at the next refresh. End process is disabled on a row marked as exited. On the monitor's own row, which `main.cpp` identifies by its `(PID, creation time)`, it offers to close the monitor instead, so the monitor shuts down the same way as when its window is closed ([DC-88](design_decisions.md#dc-88)).

### Elevated actions (future)

When elevated process termination is added, launch a small helper executable via `ShellExecuteEx` with verb `L"runas"`. This shows a standard UAC dialog for just the single action. Do not require the entire application to run elevated. Show a shield icon next to actions that will trigger a UAC prompt.

### Antivirus and EDR considerations

- **Only request `PROCESS_QUERY_LIMITED_INFORMATION`** when opening process handles. Never use `PROCESS_ALL_ACCESS` or `PROCESS_VM_READ | PROCESS_VM_WRITE`. The one exception is [unelevated process termination](#unelevated-process-termination), which also requests `SYNCHRONIZE`, and adds `PROCESS_TERMINATE` only for an unelevated process the current user owns.
- **Never call `OpenProcess` on `lsass.exe` or `csrss.exe`.** This triggers credential-dumping heuristic detections in most EDR products, even with benign access masks. Use the data already returned by `NtQuerySystemInformation` for these processes. They are recognised by the image name in the process list, so a process the list gives no name, other than the System Idle Process, is not opened either ([DC-13](design_decisions.md#dc-13)).
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
- Unit test Windows-to-domain conversions that need Windows SDK types in a separate `tests/platform/` target (SDK types and constants only, no API calls), so the main unit tests stay free of Windows headers.
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
| Ambiguous connectivity status | Use `GetNetworkConnectivityHint` for structured connectivity levels (None, LocalAccess, ConstrainedInternetAccess, InternetAccess, plus an explicit Unknown when the state cannot be determined) rather than a single "online" flag. |
| UI stutters under a large process list | Diff/update Qt item models efficiently. All collection and aggregation happens off the UI thread. Snapshots cross to UI via queued signals. |
| Unbounded memory or disk usage | Use fixed-size in-memory ring buffers and explicit persistence retention/downsampling policies. |
| Excessive privilege or security risk | Run unelevated by default. Only request `PROCESS_QUERY_LIMITED_INFORMATION`, except that termination adds `PROCESS_TERMINATE` for unelevated processes the current user owns. Never open `lsass.exe` or `csrss.exe` handles. |
| Antivirus/EDR false positives | Avoid dangerous access masks, sensitive process handles, API hooking, and process injection. Sign binaries. |
| Sleep/hibernate causes rate calculation errors | Detect `WM_POWERBROADCAST` resume events. Discard first sample and reset rate baselines. |
| WMI service not running or repository corrupted | Use WMI only for optional startup hardware inventory. All core metrics use direct Win32 APIs. |
| Qt event loop starvation | Keep snapshot emission at 1 Hz. Batch model updates. Profile early. |
| ETW session exhaustion (future) | System has a hard 64-session limit shared with EDR. Defer ETW to post-MVP and handle `ERROR_NO_SYSTEM_RESOURCES` from `StartTrace`. |
| 32-bit network counter rollover | Use `GetIfTable2` which provides 64-bit counters. Never use legacy `GetIfTable`. |
