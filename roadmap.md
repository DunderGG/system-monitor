# System Monitor — Roadmap

This roadmap expands the scope and milestones from [architecture.md](architecture.md) into actionable implementation tasks. Each phase builds on the previous one. Tasks within a phase can often be worked on in parallel.

---

## Phase 0 — Project scaffolding

Set up the build system, dependencies, CI, and a runnable application shell before writing any monitoring logic.

- [x] Create root `CMakeLists.txt` with C++20 standard, project-wide compiler warnings, and module subdirectories.
- [x] Create `CMakePresets.json` with a `default` configure preset pointing to the vcpkg toolchain file.
- [x] Create `vcpkg.json` manifest with `qtbase` (widgets, gui, network), `nlohmann-json`, `gtest`, and `spdlog`. Disable default Qt features to reduce build time.
- [x] Create `vcpkg-configuration.json` pinning a vcpkg baseline for reproducible builds.
- [x] Add a GitHub Actions CI workflow that builds and runs tests on push and pull request.
- [x] Create the `src/` module directory structure: `app/`, `domain/`, `monitoring/`, `platform/windows/`, `persistence/`, `ui/`, `ui/charts/`.
- [x] Create the `tests/` directory structure: `unit/`, `integration/`.
- [x] Implement a minimal `main.cpp` in `src/app/` that creates a `QApplication`, shows an empty `QMainWindow`, and exits cleanly.
- [x] Configure spdlog in `src/app/` with console and rotating file sinks. Make log level configurable via command-line argument for development.
- [x] Add a placeholder GoogleTest target with a single passing test to verify the test pipeline.
- [ ] Verify the full clone → configure → build → test → run cycle works on a clean Windows machine.

---

## Phase 1 — Domain types and synthetic data pipeline

Define the core data types and prove the end-to-end data flow (scheduler → snapshot → UI) using synthetic/fake data before connecting to real Windows APIs.

### Domain types (`src/domain/`)

- [ ] Define `CpuSample`: total usage percent, per-core usage percentages, core count.
- [ ] Define `MemorySample`: total bytes, available bytes, usage percent, commit limit, commit current.
- [ ] Define `DiskSample`: per-volume total bytes, free bytes, usage percent.
- [ ] Define `NetworkSample`: per-adapter in/out bytes (cumulative), link speed, operational status, adapter name.
- [ ] Define `ConnectivityStatus`: connectivity level enum (None, LocalAccess, ConstrainedInternetAccess, InternetAccess), metered flag.
- [ ] Define `ProcessInfo`: PID, creation time, parent PID, image name, image path (optional), command line (optional), CPU user/kernel times, working set, private bytes, I/O read/write bytes, thread count, handle count, access-denied flag.
- [ ] Define `SystemSnapshot`: timestamp (`steady_clock`), `CpuSample`, `MemorySample`, `vector<DiskSample>`, `vector<NetworkSample>`, `ConnectivityStatus`, `vector<ProcessInfo>`.
- [ ] Define error/access types: represent missing or inaccessible data explicitly using `std::optional` or error variants.
- [ ] Write unit tests for all domain types (construction, equality, serialization where needed).

### Monitoring infrastructure (`src/monitoring/`)

- [ ] Define `ICollector` interface (or concept) with a `collect()` method returning a typed result.
- [ ] Implement `RingBuffer<T>` — fixed-capacity circular buffer for time-series samples. Unit test capacity limits, overwrite behavior, and iteration.
- [ ] Implement `SamplingScheduler` — owns a `std::jthread`, ticks at a configured interval, calls registered collectors, assembles `SystemSnapshot`, emits via Qt signal. Uses `std::stop_token` for clean shutdown.
- [ ] Implement a `SyntheticCpuCollector` and `SyntheticMemoryCollector` that return fake data with realistic variation (e.g., sine wave CPU usage). These are for validating the pipeline and remain useful for testing and demos.
- [ ] Wire the scheduler to the synthetic collectors. Write tests verifying snapshot assembly, timing, and shutdown.

### Basic UI shell (`src/ui/`)

- [ ] Create a `QMainWindow` subclass with a tabbed layout (`QTabWidget`) for Dashboard, Performance, Processes, and Network views.
- [ ] Create placeholder `QWidget` subclasses for each tab.
- [ ] Wire the `SystemSnapshot` signal from the scheduler to the main window via `Qt::QueuedConnection`. Verify the snapshot arrives on the UI thread.
- [ ] Display raw synthetic values (CPU %, memory %) as text labels on the dashboard placeholder to prove the pipeline works.

---

## Phase 2 — Real Windows collectors and dashboard

Replace synthetic collectors with real Windows API calls and build the dashboard view.

### CPU collector (`src/platform/windows/`)

- [ ] Implement total CPU usage using `GetSystemTimes`. Calculate usage percent from idle/kernel/user time deltas over monotonic elapsed time.
- [ ] Implement per-core CPU usage using `NtQuerySystemInformation(SystemProcessorPerformanceInformation)`. Resolve the function pointer dynamically from `ntdll.dll`.
- [ ] Handle multi-processor-group systems (>64 logical cores) correctly.
- [ ] Write integration tests that run on the host and tolerate variable load.

### Memory and disk collector (`src/platform/windows/`)

- [ ] Implement memory sampling using `GlobalMemoryStatusEx`.
- [ ] Implement disk space sampling using `GetDiskFreeSpaceExW` for all fixed drives.
- [ ] Write integration tests.

### Network collector (`src/platform/windows/`)

- [ ] Implement adapter throughput using `GetIfTable2`. Use 64-bit counters. Filter loopback. Calculate bytes/sec from deltas.
- [ ] Implement adapter addresses and DNS servers using `GetAdaptersAddresses`.
- [ ] Implement connectivity status using `GetNetworkConnectivityHint`. Set up `NotifyNetworkConnectivityHintChange` for event-driven updates.
- [ ] Free resources correctly (`FreeMibTable`).
- [ ] Write integration tests tolerant of varying network environments.

### Dashboard view (`src/ui/`)

- [ ] Design a dashboard layout with resource cards: CPU, memory, disk, network, uptime.
- [ ] Each card shows current value, a mini sparkline, and a brief status label.
- [ ] Display system uptime using `GetTickCount64`.
- [ ] Display basic health status derived from resource thresholds (e.g., memory >90% → warning).
- [ ] Connect real collector data through the snapshot pipeline to the dashboard widgets.

---

## Phase 3 — Sparkline charts and performance views

### Custom sparkline widget (`src/ui/charts/`)

- [ ] Implement `SparklineWidget` as a `QWidget` subclass overriding `paintEvent`.
- [ ] Draw a `QPainterPath` polyline from ring buffer data.
- [ ] Fill below the line with a `QLinearGradient`.
- [ ] Draw a scrolling grid background offset by the sample index.
- [ ] Support configurable Y-axis range (0–100% for CPU/memory, auto-scale for network throughput).
- [ ] Support multiple series on one chart (e.g., per-core CPU).
- [ ] Display axis labels, current value, and min/max annotations.
- [ ] Test with synthetic data at various buffer sizes (60, 300, 1800 samples).

### Performance tab (`src/ui/`)

- [ ] Create a performance view with full-size sparkline charts for CPU (total and per-core), memory (usage and commit), disk (space per volume), and network (throughput per adapter).
- [ ] Show detailed numeric readouts alongside each chart (e.g., speed, processes, uptime, handles for CPU; total, cached, paged pool for memory).
- [ ] Use a left sidebar or selector to switch between CPU, memory, disk, and network detail views.
- [ ] Verify smooth 1 Hz updates with 5–30 minutes of history.

---

## Phase 4 — Process explorer

### Process collector (`src/platform/windows/`)

- [ ] Implement process enumeration using `NtQuerySystemInformation(SystemProcessInformation)`. Dynamically resolve from `ntdll.dll`. Use the resizing-buffer loop pattern.
- [ ] Parse the linked list of `SYSTEM_PROCESS_INFORMATION` structs to extract PID, parent PID, creation time, image name, thread count, handle count, working set, private bytes, I/O counters, and CPU times.
- [ ] Implement the process identity cache keyed by `(PID, creation time)`.
- [ ] On new process detection, query `QueryFullProcessImageNameW` and `NtQueryInformationProcess(ProcessCommandLineInformation)` via `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)`. Cache the result.
- [ ] Skip opening handles to `lsass.exe` and `csrss.exe` to avoid EDR false positives. Use data from `NtQuerySystemInformation` directly for these.
- [ ] Calculate per-process CPU usage percent from CPU time deltas and monotonic elapsed time.
- [ ] Run the process collector on a slow-collector thread (every 2–5 seconds).
- [ ] Write integration tests verifying enumeration, cache behavior, and access-denied handling.

### Process explorer view (`src/ui/`)

- [ ] Create a process table using `QTreeView` + a custom `QAbstractItemModel` subclass.
- [ ] Display columns: Name, PID, CPU %, Memory (working set), Disk I/O (read + write bytes/sec), Network (placeholder until ETW), Threads, Handles.
- [ ] Implement a `QSortFilterProxyModel` for column sorting and text filtering.
- [ ] Implement process tree view: build parent-child relationships from parent PID. Allow toggling between flat and tree views.
- [ ] Efficiently diff/update the model on each snapshot rather than rebuilding the entire model.
- [ ] Highlight new processes briefly (e.g., green flash) and mark terminated processes before removal.
- [ ] Display an access indicator for processes where path/command-line could not be queried.

### Process termination (unelevated only)

- [ ] Implement `TerminateProcess` for processes the current user owns.
- [ ] Show a confirmation dialog before termination with the process name, PID, and a warning.
- [ ] Handle failure gracefully (process already exited, access denied) with a user-facing message.
- [ ] Do not attempt to terminate elevated or system processes in the MVP.

---

## Phase 5 — Network view

- [ ] Create a network tab showing all non-loopback adapters in a list or card layout.
- [ ] Per adapter: name/alias, type (Ethernet/Wi-Fi/etc.), operational status, link speed, IP addresses (IPv4 and IPv6), DNS servers.
- [ ] Per adapter: live throughput sparklines (inbound and outbound bytes/sec).
- [ ] Display overall connectivity status from `GetNetworkConnectivityHint`: connectivity level, metered/unmetered, roaming.
- [ ] Optionally display friendly network name from NLM (lazy, cached query).
- [ ] Update connectivity status reactively via `NotifyNetworkConnectivityHintChange` rather than polling.

---

## Phase 6 — Settings and polish

- [ ] Implement the JSON settings file: refresh interval, display units, enabled dashboard panels, log level.
- [ ] Create a settings dialog (`QDialog`) for editing preferences.
- [ ] Load settings at startup with defaults and validation. Save on change.
- [ ] Add an application icon and window title.
- [ ] Enable per-monitor DPI awareness in the application manifest.
- [ ] Implement sleep/resume detection (`WM_POWERBROADCAST`) — discard first sample and reset rate baselines on resume.
- [ ] Profile CPU and memory usage of the monitor itself on a lower-end system. Optimize if needed.
- [ ] Add an application manifest setting `requestedExecutionLevel` to `asInvoker`.

---

## Future phases (post-MVP)

These features are on the roadmap but not scheduled. Each can be picked up independently once the MVP is stable.

### Elevated process termination

- [ ] Create a small helper executable that accepts a PID and action via command-line arguments.
- [ ] Launch the helper via `ShellExecuteEx` with the `runas` verb to trigger a UAC prompt.
- [ ] Show a shield icon in the process explorer next to actions that require elevation.

### System tray

- [ ] Add a `QSystemTrayIcon` with a context menu (Open, Exit).
- [ ] Minimize to tray on window close (configurable).
- [ ] Show mini resource summary in the tray tooltip.
- [ ] Add a "Start minimized" setting.

### Dark mode and theme support

- [ ] Detect the Windows system theme (light/dark) at startup.
- [ ] Apply a dark stylesheet/palette when dark mode is active.
- [ ] Add a setting to override: Light / Dark / Follow System.
- [ ] Ensure sparkline charts and all custom widgets respect the active theme.

### Alert rules and notification history

- [ ] Define alert rules in settings: metric, threshold, direction, cooldown.
- [ ] Evaluate rules on each snapshot tick.
- [ ] Show toast notifications via `QSystemTrayIcon::showMessage` or Windows notification API.
- [ ] Maintain an in-memory alert history viewable in the UI.

### Persistent event and metric history

- [ ] Add SQLite via vcpkg behind the `IMetricsStore` interface.
- [ ] Batch-insert samples every 10–30 seconds on a background thread.
- [ ] Implement retention policies: downsample older data, expire after configurable duration.
- [ ] Add a history view or extend sparkline charts to show historical data ranges.

### Startup applications and services

- [ ] Enumerate startup entries from the registry (`Run`, `RunOnce`) and startup folder.
- [ ] Enumerate Windows services via `EnumServicesStatusEx`.
- [ ] Display in a dedicated tab with name, status, startup type.

### GPU monitoring

- [ ] Query GPU utilization and memory via NVML (NVIDIA), ADL (AMD), or `D3DKMTQueryStatistics`.
- [ ] Add GPU performance cards and sparklines.

### ETW-powered diagnostics

- [ ] Implement an ETW real-time trace session consumer.
- [ ] Subscribe to `Microsoft-Windows-Kernel-Network` for per-process network bandwidth.
- [ ] Add short "diagnostic recording" sessions with start/stop and export.
- [ ] Handle ETW session limits (`ERROR_NO_SYSTEM_RESOURCES`) and permission requirements gracefully.

### CSV/JSON export

- [ ] Export current snapshot or ring buffer history as CSV or JSON.
- [ ] Add export actions to the UI (file dialog).

### DevEnv
- [x] Add a PowerShell bootstrap script that detects or installs build prerequisites, sets up vcpkg and its binary cache, and verifies the toolchain.
