# Design Decisions

Rationale for non-obvious implementation choices made during development, grouped by topic ([DC-116](#dc-116)). The line under each topic's heading says what it covers, and an entry's **History:** line names the phase or code-review finding that made it.

An entry holds a decision and its reasons ([DC-117](#dc-117)). Measured tick costs are in [performance.md](performance.md), what `build.ps1` does is in [build_system.md](build_system.md), the rules that every change follows are in [AGENTS.md](../AGENTS.md) and the [coding guidelines](coding_guidelines.md), with their reasons, and the tests, the program's text and where code lives are in the code.

Each entry has a permanent ID (`DC-01`, `DC-02`, …) that other documents and code comments can cite. A new entry takes the next unused number and goes at the end of its topic, so the entries within a topic are in ID order. Numbers are never reused or renumbered.

An entry states the decision as the code now has it ([DC-115](#dc-115)). A change to the decision rewrites the entry: its **Decision** and **Rationale** describe the choice as it now stands, and its last line, **History:**, which names the phase or code-review finding that made the decision, gains the one that changed it. The Rationale names an earlier choice only as a rejected alternative, when the reason it was dropped still matters. No note such as "Superseded in part by" or "Extended by" is added, and no new entry patches an old one. Earlier versions are in git: `git log -L ':DC-76\::docs/design_decisions.md'` lists every change to DC-76. An entry merged into another, or retired, keeps its anchor and its heading as a stub with one line that says where the decision is now, so that links to it still resolve.

Each entry heading has an anchor of its ID alone on the line above it, `<a id="dc-NN"></a>`, and links name that anchor: `design_decisions.md#dc-NN` from another file, `#dc-NN` within this one. A link to the heading's own slug breaks when the entry is retitled. `.\scripts\build.ps1 -Format` and `-CheckFormat` fail on a missing anchor, a link that does not resolve, and a `DC-NN` cited anywhere that is not an entry ([DC-114](#dc-114)).

## Contents

- [Windows APIs](#windows-apis)
  - [DC-01](#dc-01): Process enumeration: `NtQuerySystemInformation` over Tool Help
  - [DC-02](#dc-02): Core metrics: direct Win32 APIs over PDH
  - [DC-03](#dc-03): Connectivity status: `GetNetworkConnectivityHint` over ad-hoc probes
  - [DC-04](#dc-04): WMI restricted to startup hardware inventory
  - [DC-05](#dc-05): ETW deferred to post-MVP
  - [DC-06](#dc-06): Network names: `INetworkListManager`, read only after a change or once a minute, never on the tick
  - [DC-22](#dc-22): UTF-8 active code page through an application manifest
  - [DC-26](#dc-26): Retired, the rule is in AGENTS.md and the coding guidelines
- [Modules and dependencies](#modules-and-dependencies)
  - [DC-07](#dc-07): CMake library type: `INTERFACE` (header-only)
  - [DC-20](#dc-20): UI module dependency isolation and composition root in `src/app/`
  - [DC-24](#dc-24): Collector interfaces in their own Qt-free CMake target
  - [DC-47](#dc-47): `RingBuffer<T>` lives in `domain` so the UI can use it
- [Domain model](#domain-model)
  - [DC-08](#dc-08): `ProcessInfo::creationTime` type: `std::chrono::system_clock::time_point`
  - [DC-09](#dc-09): `SystemSnapshot::timestamp` type: `std::chrono::steady_clock::time_point`
  - [DC-10](#dc-10): `NetworkSample` stores cumulative byte counters, not rates
  - [DC-11](#dc-11): `OperationalStatus` enum: all 7 `IF_OPER_STATUS` values
  - [DC-12](#dc-12): Missing data is `std::optional` or an explicit Unknown, never a zero or an empty value
  - [DC-14](#dc-14): CPU times stored as cumulative milliseconds, not percentages
  - [DC-15](#dc-15): Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)
  - [DC-16](#dc-16): `RingBuffer<T>`: Mirrored contiguous storage for zero-copy `std::span`
  - [DC-43](#dc-43): Merged into DC-12
  - [DC-44](#dc-44): Merged into DC-12
- [Scheduler and collectors](#scheduler-and-collectors)
  - [DC-17](#dc-17): `SamplingScheduler`: Responsive cancellation via `std::condition_variable_any` with `std::stop_token`
  - [DC-18](#dc-18): `ICollector<T>`: one virtual interface for every collector, and no concept
  - [DC-19](#dc-19): `SamplingScheduler`: Pointer snapshotting during `sampleOnce()` to avoid holding `m_collectorMutex` during collections
  - [DC-37](#dc-37): Repeated collector failures logged once, with the recovery
  - [DC-77](#dc-77): A slow-collector thread for processes, connectivity and network names, woken when an event-driven collector stores a change
  - [DC-101](#dc-101): Merged into DC-77
- [CPU, memory, disk and uptime collectors](#cpu-memory-disk-and-uptime-collectors)
  - [DC-25](#dc-25): Total CPU utilization formula: `(deltaKernel + deltaUser - deltaIdle) / (deltaKernel + deltaUser)`
  - [DC-28](#dc-28): Per-core CPU utilization: `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` dynamic resolution and resizing-buffer pattern
  - [DC-29](#dc-29): Multi-processor-group support (>64 logical cores): `NtQuerySystemInformationEx` iteration and total CPU aggregation
  - [DC-31](#dc-31): Memory sampling: `GlobalMemoryStatusEx` and commit charge calculation
  - [DC-32](#dc-32): Disk space sampling: `GetDiskFreeSpaceExW` strictly filtered to `DRIVE_FIXED`
  - [DC-42](#dc-42): System uptime: `GetTickCount64` as a fast collector with an optional snapshot field
  - [DC-55](#dc-55): CPU base speed from `CallNtPowerInformation`, read once; no current clock speed
  - [DC-56](#dc-56): Memory detail and system activity counts from `GetPerformanceInfo`, as fast collectors
- [Process collector](#process-collector)
  - [DC-13](#dc-13): Process details status kept in-band, and the processes the monitor never opens
  - [DC-70](#dc-70): Process list read into a kept, growing buffer through an injected query
  - [DC-71](#dc-71): Process list parsed through a full `SYSTEM_PROCESS_INFORMATION` layout checked against `winternl.h`
  - [DC-72](#dc-72): Merged into DC-13
  - [DC-73](#dc-73): Process details cached by `(PID, creation time)` with a domain `ProcessIdentity` key
  - [DC-74](#dc-74): Merged into DC-13
  - [DC-75](#dc-75): Process details read with a limited handle, checked against the identity, and logged at debug
  - [DC-76](#dc-76): Per-process CPU usage and I/O rates computed in the collector, CPU as a share of all logical processors
  - [DC-94](#dc-94): Merged into DC-13
- [Network adapter collector](#network-adapter-collector)
  - [DC-34](#dc-34): Network adapter traffic & throughput: `GetIfTable2` with RAII `FreeMibTable` and delta rates
  - [DC-35](#dc-35): Adapter identity and details: `GetAdaptersAddresses` matched to `GetIfTable2` by LUID, cached, and reduced to what can be used
  - [DC-36](#dc-36): Merged into DC-35
  - [DC-59](#dc-59): Merged into DC-35
  - [DC-64](#dc-64): Merged into DC-35
  - [DC-103](#dc-103): Merged into DC-35 and DC-96
  - [DC-108](#dc-108): Merged into DC-35 and DC-96
- [Connectivity and network names](#connectivity-and-network-names)
  - [DC-39](#dc-39): Connectivity: a cache fed by `NotifyNetworkConnectivityHintChange`, an explicit Unknown, and a section on the Network tab
  - [DC-40](#dc-40): Merged into DC-39
  - [DC-41](#dc-41): Windows-to-domain connectivity mapping in a separate, testable header
  - [DC-99](#dc-99): Merged into DC-39
  - [DC-100](#dc-100): Network names from `INetworkListManager`, read on the collector's own thread at most once a second, and matched to adapters by LUID
  - [DC-107](#dc-107): Merged into DC-100
- [Charts](#charts)
  - [DC-48](#dc-48): `SparklineWidget`: a caller-owned history, series on one Y range, optional decorations, and cheap dense charts
  - [DC-49](#dc-49): Merged into DC-48
  - [DC-50](#dc-50): Merged into DC-48
  - [DC-51](#dc-51): Merged into DC-48
  - [DC-52](#dc-52): Merged into DC-48
  - [DC-60](#dc-60): Merged into DC-48
  - [DC-65](#dc-65): Merged into DC-48
  - [DC-67](#dc-67): Merged into DC-48 and DC-57
- [Main window and shared UI](#main-window-and-shared-ui)
  - [DC-54](#dc-54): Snapshot forwarding: every view records every snapshot, and hidden views skip repainting (code review F-4)
  - [DC-68](#dc-68): Retired, the headers say what they hold
  - [DC-102](#dc-102): Retired, the rule is in the coding guidelines
  - [DC-105](#dc-105): Retired, the rule is in the coding guidelines
- [Dashboard tab](#dashboard-tab)
  - [DC-45](#dc-45): Health evaluation: domain types, a pure evaluator in `monitoring`, and the result carried in the snapshot
  - [DC-46](#dc-46): Dashboard cards: `ResourceCard` widgets fed by pure formatting helpers
- [Performance tab](#performance-tab)
  - [DC-53](#dc-53): Performance view: a sidebar of resource pages that each own their history
  - [DC-57](#dc-57): Performance readouts: pure builders with stable names, shown in a `ReadoutGrid`
  - [DC-58](#dc-58): Performance pages fit an 800×600 window: re-flowing readouts and a scrolling per-core grid (code review F-20)
  - [DC-61](#dc-61): 30 minutes of performance history and a chart-window selector
  - [DC-63](#dc-63): Memory readouts name what `GetPerformanceInfo` reports, not Task Manager's figures (code review F-22)
- [Processes tab](#processes-tab)
  - [DC-79](#dc-79): Process table model: one row per `ProcessIdentity`, its columns, and updates that signal only the cells that changed
  - [DC-80](#dc-80): Merged into DC-79
  - [DC-81](#dc-81): Sorting and filtering: a proxy that compares domain values, with missing values lowest, and sorts once per refresh
  - [DC-82](#dc-82): Process tree and highlights: parents matched by PID, creation time and listing time, and new and exited rows marked for three seconds
  - [DC-83](#dc-83): Merged into DC-79 and DC-81
  - [DC-84](#dc-84): Merged into DC-82
  - [DC-85](#dc-85): Access indicator: a painted icon per details status, and a tooltip with the details or the reason
  - [DC-91](#dc-91): Merged into DC-79
  - [DC-92](#dc-92): Merged into DC-82
- [Process termination](#process-termination)
  - [DC-87](#dc-87): Process termination: identity, owner and elevation checked before `PROCESS_TERMINATE` is requested
  - [DC-88](#dc-88): End process: one action, a confirmation with Cancel as default, a message for each outcome but success, and an offer to close the monitor on its own row
  - [DC-89](#dc-89): Merged into DC-88
  - [DC-90](#dc-90): Merged into DC-87 and DC-88
  - [DC-93](#dc-93): Merged into DC-88
- [Network tab](#network-tab)
  - [DC-96](#dc-96): Network tab: a card per adapter in a fixed order, the adapters Windows hides behind "Show all adapters", and link details only while Up
  - [DC-97](#dc-97): Merged into DC-96
  - [DC-98](#dc-98): Adapter throughput charts: one minute per card, with the history shared with the Performance tab
  - [DC-106](#dc-106): Merged into DC-96
- [Build and tooling](#build-and-tooling)
  - [DC-23](#dc-23): Formatting enforced with a pinned clang-format and the existing house style
  - [DC-62](#dc-62): `build.ps1`: launch only with `-Run`, configure only when needed, and parallel tests
  - [DC-66](#dc-66): Committed tick-cost benchmarks, run with `build.ps1 -Benchmark` (code review F-25)
  - [DC-69](#dc-69): `build.ps1` retries a failed configure once from a clean cache
  - [DC-86](#dc-86): qtbase built with PNG support for Qt's own Windows cursors
  - [DC-95](#dc-95): Benchmarks print the build and the machine they ran on (code review F-33)
  - [DC-104](#dc-104): Qt's Windows style plugin deployed next to every executable (code review F-42)
- [Testing](#testing)
  - [DC-21](#dc-21): `test_main.cpp`: listing the tests creates no `QApplication`
  - [DC-27](#dc-27): Collectors take their OS calls as injected readers, beside pure calculation functions
  - [DC-30](#dc-30): Integration tests assert invariants of the real machine, and the process tests use a suspended child process
  - [DC-33](#dc-33): Merged into DC-27
  - [DC-38](#dc-38): Merged into DC-27
  - [DC-78](#dc-78): Merged into DC-30
  - [DC-109](#dc-109): Shared test builders and harnesses in `tests/support/`, header-only
  - [DC-110](#dc-110): One scheduler pipeline test, with the collectors `registerSystemCollectors()` registers
  - [DC-111](#dc-111): Fixed-value fake collectors in `tests/support/`, not synthetic collectors in `monitoring`
  - [DC-112](#dc-112): A mapping's values tested in one test, as a table of named cases
  - [DC-113](#dc-113): Tests run as GoogleTest shards, one CTest test each, not one process per test
- [Design decision log](#design-decision-log)
  - [DC-114](#dc-114): Links name an entry's ID anchor, not its heading, and `build.ps1` checks that they resolve
  - [DC-115](#dc-115): Living entries: an entry states the current decision, is rewritten when it changes, and ends with a one-line history
  - [DC-116](#dc-116): Entries grouped by topic, in ID order within each topic
  - [DC-117](#dc-117): Entries hold decisions: benchmark figures in `performance.md`, build-script behaviour in `build_system.md`, rules in the coding guidelines, and no test lists, copied UI text or code locations

---

## Windows APIs

Which Windows API supplies each kind of data, and the code page of the text that the program passes to Windows.

<a id="dc-01"></a>
### DC-01: Process enumeration: `NtQuerySystemInformation` over Tool Help

**Decision:** Use `NtQuerySystemInformation(SystemProcessInformation)` for process enumeration, not `CreateToolhelp32Snapshot`.

**Rationale:** `CreateToolhelp32Snapshot` internally calls `NtQuerySystemInformation`, strips out CPU, memory, and I/O metrics, and forces the caller to make hundreds of supplementary `OpenProcess` calls to reconstruct data the kernel already returned in that single call. On a system with ~300 processes this produces 300+ extra kernel transitions and takes 15–45 ms compared to 0.5–1.5 ms for the direct call. Beyond performance, `CreateToolhelp32Snapshot` denies access to metrics for 30–50% of processes on an unelevated caller because each `OpenProcess` can fail individually; `NtQuerySystemInformation` returns complete CPU, memory, and I/O data for all processes including protected and elevated ones. `NtQuerySystemInformation` is declared in `<winternl.h>`, has been ABI-stable since Windows 2000, and is the API used internally by Task Manager, Process Explorer, and System Informer.

**History:** Architecture, before Phase 1.

---

<a id="dc-02"></a>
### DC-02: Core metrics: direct Win32 APIs over PDH

**Decision:** Use `GetSystemTimes`, `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`, and `GetIfTable2` for core metrics. PDH is reserved only for advanced per-instance counters (e.g. per-physical-disk I/O rates) where no direct Win32 alternative exists.

**Rationale:** PDH has two structural problems that make it unsuitable for core metrics. First, PDH counter names are locale-dependent; `PdhAddCounterW` with a hard-coded English path fails on non-English Windows installations unless `PdhAddEnglishCounterW` is used, but even then PDH reads counter definitions from the registry — which is frequently corrupted on long-lived installations, causing `PDH_CSTATUS_NO_OBJECT` or `PDH_CSTATUS_NO_COUNTER` errors at runtime. The direct Win32 APIs (`GetSystemTimes` etc.) have zero registry dependency. Second, for per-process metrics PDH assigns index suffixes to processes that share a name (`chrome#0`, `chrome#1`) and silently re-indexes when one terminates; any open PDH query handle will silently read data from a different process after re-indexing, producing corrupt data with no error signal. `NtQuerySystemInformation` has no such concept — processes are identified by PID and creation time.

**History:** Architecture, before Phase 1.

---

<a id="dc-03"></a>
### DC-03: Connectivity status: `GetNetworkConnectivityHint` over ad-hoc probes

**Decision:** Use `GetNetworkConnectivityHint` (and `NotifyNetworkConnectivityHintChange` for async updates) to determine network connectivity state.

**Rationale:** Ad-hoc ICMP/TCP probes require choosing a target, have non-trivial latency (100–2000 ms), consume bandwidth, and produce a binary online/offline result that misses important states like captive portals and metered connections. `GetNetworkConnectivityHint` returns a structured `NL_NETWORK_CONNECTIVITY_LEVEL_HINT` — None, LocalAccess, ConstrainedInternetAccess (captive portal), or InternetAccess — along with cost information (metered vs. unmetered) and roaming state. This is a direct C-style kernel API with no COM overhead. The companion notification API delivers change events without polling. Requires Windows 10 version 2004 or later, which is within the project's target baseline.

**History:** Architecture, before Phase 1.

---

<a id="dc-04"></a>
### DC-04: WMI restricted to startup hardware inventory

**Decision:** WMI is queried at most once at application startup for static hardware information (`Win32_ComputerSystem`, `Win32_Processor`, `Win32_BaseBoard`). WMI is never polled for real-time metrics.

**Rationale:** WMI is an out-of-process COM call to `WmiPrvSE.exe`. Each query incurs 50–500 ms of latency and measurable CPU cost on the host. Polling it for metrics that change frequently (CPU usage, memory) would make the monitor a significant load source, contradicting the low-impact goal. WMI repository corruption is also common on long-lived Windows installations and manifests as silent data errors or hung calls. For the one legitimate use case — reading static hardware labels at startup — the latency is acceptable because it occurs once before the UI is shown. All real-time metrics use direct Win32 APIs.

**History:** Architecture, before Phase 1.

---

<a id="dc-05"></a>
### DC-05: ETW deferred to post-MVP

**Decision:** ETW (Event Tracing for Windows) is excluded from the MVP. It will be reconsidered only when per-process network bandwidth tracking or event-level diagnostics are a product requirement.

**Rationale:** ETW kernel trace sessions require administrator privileges, making them incompatible with the unelevated-by-default security posture. Windows enforces a hard system-wide limit of 64 concurrent ETW sessions, shared with EDR products, antivirus, and system loggers; exceeding this limit returns `ERROR_NO_SYSTEM_RESOURCES` and the session fails to start. Buffer sizing and flush tuning are non-trivial and highly workload-dependent. None of the MVP metrics require ETW — per-process CPU, memory, and I/O are fully available through `NtQuerySystemInformation`. The complexity and privilege cost are not justified until a specific feature (per-process network bandwidth) cannot be built without it.

**History:** Architecture, before Phase 1.

---

<a id="dc-06"></a>
### DC-06: Network names: `INetworkListManager`, read only after a change or once a minute, never on the tick

**Decision:** The COM-based Network List Manager (`INetworkListManager`) is used only for the name and category of each connected network (e.g. "Home-Wi-Fi"). The network-name collector reads it on a worker thread of its own, after a connectivity change notification or once its names are a minute old, and at most once a second. The 1-second tick never queries it ([DC-100](#dc-100)).

**Rationale:** COM initialization carries per-thread cost and `INetworkListManager` calls cross into a system service. For a value that changes only when the user switches networks, querying it on every 1-second tick would be unnecessary overhead. All connectivity state (level, cost, roaming) and all throughput data come from `GetNetworkConnectivityHint` and `GetIfTable2`, which are C-style kernel APIs. The NLM is used purely for the human-readable label that those APIs do not provide. A connectivity change is when the connected networks can change, and the minute picks up a renamed network or a changed category, which raise no such event.

**History:** Architecture, before Phase 1; Phase 5, network view (names also re-read once they are a minute old, for [DC-100](#dc-100)); code review F-38 (read on the collector's own thread, at most once a second, for [DC-100](#dc-100)).

---

<a id="dc-22"></a>
### DC-22: UTF-8 active code page through an application manifest

**Decision:** `src/app/system_monitor.manifest` sets `activeCodePage` to `UTF-8`, and CMake embeds it in `system_monitor.exe` and in `system_monitor_tests.exe`, so tests run under the same code page as the app. Narrow strings passed to Windows and the CRT are therefore UTF-8. The logging setup moved into the `sysmon_app_logging` library so an integration test can configure logging with a path that contains CJK and Cyrillic characters.

**Rationale:** `configureLogging()` passes `path::string()` to spdlog. Under a legacy ANSI code page, MSVC's `path::string()` throws `std::system_error` for characters that page cannot represent. A user profile path with a Chinese or Cyrillic name on a Western-locale system therefore made the app terminate at startup, and against the no-exceptions rule ([code review F-19](code_reviews/phase_2.md#f-19-log-file-path-is-converted-to-the-ansi-code-page)). The UTF-8 code page fixes every narrow-string path in the process, not only the log file, and it matches the decision that domain strings are UTF-8. It needs Windows 10 1903, below the project's existing 2004 requirement (`GetNetworkConnectivityHint`). The rejected alternative, building spdlog with wide file names, would fix only the log path and depends on what the vcpkg port offers. The manifest is also where the Phase 6 DPI-awareness setting belongs. A test checks `GetACP() == CP_UTF8`, so losing the manifest fails the build's tests.

**History:** Code review F-19.

---

<a id="dc-26"></a>
### DC-26: Retired, the rule is in AGENTS.md and the coding guidelines

That a header other modules include, such as `cpu_collector.h`, has no Windows types or headers is in [coding_guidelines.md — Windows API wrapping](coding_guidelines.md#windows-api-wrapping), and in short in [AGENTS.md](../AGENTS.md) and `src/platform/windows/AGENTS.md`.

---

## Modules and dependencies

Which CMake target holds what, and which modules may depend on which.

<a id="dc-07"></a>
### DC-07: CMake library type: `INTERFACE` (header-only)

**Decision:** `sysmon_domain` is declared as a CMake `INTERFACE` library with no compiled sources.

**Rationale:** Every domain type is a plain aggregate struct or `enum class`. There are no method bodies, no `.cpp` translation units, and nothing to link. An `INTERFACE` target is the idiomatic CMake choice in this situation: it propagates the `src/` include path transitively to any downstream target via `target_link_libraries`, so consumers never need to repeat `target_include_directories`. A `STATIC` library would produce an empty archive and add a link step with zero benefit.

**History:** Phase 1, domain types.

---

<a id="dc-20"></a>
### DC-20: UI module dependency isolation and composition root in `src/app/`

**Decision:** The `ui` module depends exclusively on `Qt6::Widgets` and `sysmon_domain`, with zero dependencies on `sysmon_monitoring` or Windows headers. Wiring between `SamplingScheduler` signals and `MainWindow` slots is handled strictly in `src/app/main.cpp` (the application composition root).

**Rationale:** Maintaining a one-way dependency flow prevents cyclic references and tight coupling. The UI layer only knows about domain values (`SystemSnapshot`). It does not know how or when data is collected, nor what thread model is used. By placing the signal/slot `Qt::QueuedConnection` wiring in `main.cpp`, `MainWindow` can be tested in isolation with mock snapshots, and collectors can be altered or replaced without recompiling any UI translation units.

**History:** Phase 1, basic UI shell.

---

<a id="dc-24"></a>
### DC-24: Collector interfaces in their own Qt-free CMake target

**Decision:** `monitoring/collector.h` is the header-only `INTERFACE` target `sysmon_collector_interfaces`, which depends only on `sysmon_domain`. `sysmon_monitoring` and `sysmon_platform_windows` both link it publicly. `platform/windows` may include `collector.h` and no other `monitoring` header.

**Rationale:** Every Windows collector implements an interface from `collector.h`, but `sysmon_platform_windows` linked only `sysmon_domain`. The include worked only because all modules share the `src/` include root, so CMake did not know about the dependency ([code review F-13](code_reviews/phase_2.md#f-13-platformwindows-depends-on-monitoring-headers-without-declaring-it)). Linking `sysmon_monitoring` instead would have pulled `Qt6::Core` into a module whose rules forbid Qt. A separate target states the real dependency, keeps Qt out, and is exercised by `system_monitor_platform_tests`, which links `sysmon_platform_windows` without Qt. Because the include root is shared, CMake cannot stop a `platform/windows` file from including another `monitoring` header. That rule is enforced by review and stated in `src/platform/windows/AGENTS.md`.

**History:** Code review F-13.

---

<a id="dc-47"></a>
### DC-47: `RingBuffer<T>` lives in `domain` so the UI can use it

**Decision:** Move `RingBuffer<T>` from `monitoring` to `domain` (`domain/ring_buffer.h`, namespace `sysmon::domain`) with no change to its behaviour. The dashboard's mini sparklines keep their own short presentation history in `RingBuffer`s owned by the UI. Longer histories (e.g. for the Performance view or alert rules) may still live in `monitoring`, using the same type.

**Rationale:** The UI must not depend on `monitoring` (`src/ui/AGENTS.md`), but sparklines need a bounded history with the contiguous-span access `RingBuffer` was designed for. `RingBuffer` is header-only and uses only the standard library, so it already meets the domain's dependency rules, and it had no production users, so the move is cheap. Alternatives considered:
- A UI-only `std::deque` history would duplicate `RingBuffer` with a less efficient design.
- Carrying history inside every `SystemSnapshot` is cheap for the dashboard but grows with Phase 3's longer, per-core histories.
- A new header-only `src/common/` module is semantically cleaner, since `domain` otherwise holds data types. It is deferred until a second shared utility justifies a new module; see the "Code organization" item in the roadmap.

**History:** Phase 2, dashboard view.

---

## Domain model

How the domain types represent time, counters, text and missing data, and the `RingBuffer` that holds sample histories.

<a id="dc-08"></a>
### DC-08: `ProcessInfo::creationTime` type: `std::chrono::system_clock::time_point`

**Decision:** The process creation timestamp is stored as `std::chrono::system_clock::time_point`, not `uint64_t` raw FILETIME or `steady_clock::time_point`.

**Rationale:** The source value is a Windows `FILETIME` — a 100-nanosecond tick count since 1601-01-01, which is a calendar epoch. `system_clock` is the C++ clock that maps to calendar time, making the conversion at the platform boundary lossless and semantically correct. `steady_clock` is reserved for `SystemSnapshot::timestamp` where monotonic elapsed-time arithmetic is needed (CPU %, network throughput rates). Storing a raw `uint64_t` would leak a Windows type into the domain layer, which must remain platform-free.

**History:** Phase 1, domain types.

---

<a id="dc-09"></a>
### DC-09: `SystemSnapshot::timestamp` type: `std::chrono::steady_clock::time_point`

**Decision:** `SystemSnapshot::timestamp` uses `steady_clock`, not `system_clock`.

**Rationale:** The timestamp exists solely for rate calculations (CPU usage %, network bytes/sec) between consecutive snapshots. `steady_clock` is monotonic and unaffected by system clock adjustments or DST transitions, making delta calculations reliable. It carries no calendar meaning; the UI must use `system_clock::now()` separately if it needs a human-readable time to display.

**History:** Phase 1, domain types.

---

<a id="dc-10"></a>
### DC-10: `NetworkSample` stores cumulative byte counters, not rates

**Decision:** `inBytesTotal` and `outBytesTotal` are cumulative totals as returned by `GetIfTable2`, not pre-computed bytes/sec rates.

**Rationale:** Rate computation requires dividing a byte delta by a precisely measured time delta. That measurement belongs in the collector, which owns the `steady_clock` baseline from the previous tick. Storing a rate in the domain type would mean the type carries a value that depends on when it was last queried — breaking the immutable-snapshot guarantee. Cumulative counters are also correct across tick-interval jitter, whereas a rate pre-computed with an assumed interval would drift if the scheduler tick is late.

**History:** Phase 1, domain types.

---

<a id="dc-11"></a>
### DC-11: `OperationalStatus` enum: all 7 `IF_OPER_STATUS` values

**Decision:** `OperationalStatus` includes all seven values from the Windows `IF_OPER_STATUS` enum (`Up`, `Down`, `Testing`, `Unknown`, `Dormant`, `NotPresent`, `LowerLayerDown`), not a trimmed subset.

**Rationale:** A 1:1 mapping between the domain enum and the Windows API enum makes the platform-boundary conversion trivial and keeps the domain type self-documenting. Collapsing values (e.g. merging `Dormant` and `NotPresent` into a generic `Inactive`) would lose information that the UI or future alert logic might need. The domain module has no Windows dependency — having the same *shape* as a Windows enum does not violate the no-Windows-headers rule.

**History:** Phase 1, domain types.

---

<a id="dc-12"></a>
### DC-12: Missing data is `std::optional` or an explicit Unknown, never a zero or an empty value

**Decision:** A value that could not be read, or cannot be computed yet, is `std::nullopt`, or Unknown for an enum, from the collector to the UI. No default value stands in for it.

- **Samples:** `ICpuCollector` and `IMemoryCollector` return `std::optional<CpuSample>` and `std::optional<MemorySample>`, and `SystemSnapshot::cpu` and `::memory` are optional, as `::uptime` is ([DC-42](#dc-42)). `CpuCollector` returns `std::nullopt` when both queries fail and on the sample that only establishes its baseline. `calculateCpuUsage` returns `std::nullopt` for non-positive elapsed time, any counter that went backwards, or zero elapsed CPU time. Per-core usages are all-or-nothing: an empty `coreUsagePercents` means "unavailable" (after a core-count change or an uncomputable core), never zero-filled. `calculateMemorySample` returns `std::nullopt` for a zero physical-memory total, and `MemoryCollector` returns it when `GlobalMemoryStatusEx` fails. `calculateDiskSample` returns `std::nullopt` for a zero-capacity volume, and `DiskCollector` skips that volume, as it skips volumes whose query fails.
- **Collections:** `IDiskCollector`, `INetworkCollector`, and `IProcessCollector` return `std::optional<std::vector<...>>`, and `SystemSnapshot::disks`, `::networks`, and `::processes` are optional. `std::nullopt` means no collector is registered or the query failed. An empty vector means the query succeeded and found nothing. `DiskCollector` returns `std::nullopt` when the fixed drives cannot be enumerated, and also when fixed drives exist but none of them could be read. `NetworkCollector` returns it when `GetIfTable2` fails.
- **Rates:** a rate that cannot be computed, as on the first sample of an adapter or a process, is `std::nullopt`, never 0 ([DC-34](#dc-34), [DC-76](#dc-76)).
- **Text:** `ProcessInfo::imagePath` and `::commandLine` are `std::optional<std::string>`, not empty strings, and `detailsStatus` says why one is missing ([DC-13](#dc-13)).
- **Enums:** `ConnectivityLevel::Unknown` is the default level, and the cost and roaming flags are `std::optional<bool>` ([DC-39](#dc-39)). Health has an Unknown level too ([DC-45](#dc-45)).
- **In the UI:** a missing value shows "N/A", and a value before the first snapshot "--". The dashboard reserves "No active adapter" for a successful read with no hardware adapter Up.

**Rationale:** The domain rules forbid representing missing data as a default value.

- **No zeros:** a 0% CPU or 0 GiB memory reading is indistinguishable from a real idle system or a broken query ([known deviation D-7](known_deviations.md#d-7-memory-and-cpu-data-report-zeros-when-missing)), and a zero-capacity volume would read as 0% used ([D-8](known_deviations.md#d-8-zero-capacity-disk-volumes-report-0-usage)). A zero would also mislead the health evaluation: a missing sample must not read as "healthy". A genuine 0% (a fully idle interval) is still reported as 0%. Rejecting deltas across a counter reset also fixed a latent bug: if only one counter went backwards, the saturating subtraction used before could report a meaningless rate such as 100%. Per-core values are all-or-nothing because a partially filled list would misalign core indices.
- **No empty collections:** with plain vectors, a failed query looked the same as a machine with no volumes or no adapters, and the dashboard's network card stated "No active adapter" when nothing had been observed ([code review F-12](code_reviews/phase_2.md#f-12-empty-disk-and-network-vectors-hide-collector-failures)). A partial disk result, where some volumes are readable, is still reported as a vector, because the readable volumes are real data. The unreadable ones are skipped and logged.
- **No empty strings:** the image path and command line are read once per process, when its `(PID, creation time)` is first seen, and may be permanently unavailable for protected or system processes. An empty string would be ambiguous: it could mean "not yet queried", "query failed", or "the process genuinely has no path". `std::nullopt` says that the data is not available, and the UI can show a clear indicator rather than a blank field.

**History:** Phase 1, domain types (the process path and command line); Phase 2, known deviation D-7 (CPU and memory samples); Phase 2, dashboard view (zero-capacity volumes, known deviation D-8); code review F-12 (collections); maintenance (DC-43 and DC-44 merged in).

---

<a id="dc-14"></a>
### DC-14: CPU times stored as cumulative milliseconds, not percentages

**Decision:** `ProcessInfo::cpuUserTimeMs` and `::cpuKernelTimeMs` are cumulative totals in milliseconds, not pre-computed CPU percentages.

**Rationale:** CPU percentage for a process requires comparing the CPU time delta between two snapshots against the wall-clock delta between those same snapshots. This calculation belongs in the collector or view model, which tracks both baselines. Storing a percentage in the domain struct would mean the value is meaningless without knowing the interval over which it was measured — and that interval is not part of the struct. Cumulative milliseconds are the raw truth; any derived value can be computed correctly from them.

**History:** Phase 1, domain types.

---

<a id="dc-15"></a>
### DC-15: Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)

**Decision:** All string fields in domain types use `std::string` with UTF-8 encoding. Wide-to-UTF-8 conversion happens at the `platform/windows` boundary.

**Rationale:** The domain layer must be platform-independent. `std::wstring` is a Windows-specific convention (16-bit code units); using it would leak a platform assumption into `src/domain/`. UTF-8 is the de facto standard encoding for cross-platform C++ and is natively supported by Qt (`QString::fromUtf8`), spdlog, and nlohmann/json. The Windows API wrapper functions in `platform/windows/` convert from `WCHAR*` / `std::wstring` to UTF-8 `std::string` using `WideCharToMultiByte(CP_UTF8, ...)` or equivalent. Supplementary-plane characters (e.g., emoji in filenames) round-trip correctly through UTF-8.

**History:** Code review F-6.

---

<a id="dc-16"></a>
### DC-16: `RingBuffer<T>`: Mirrored contiguous storage for zero-copy `std::span`

**Decision:** `RingBuffer<T>` pre-allocates $2 \times \text{capacity}$ contiguous storage and mirrors every push to both `pos` and `pos + capacity`.

**Rationale:** A traditional circular buffer wraps around the end of its storage array, requiring either linearizing elements into a temporary buffer (incurring heap allocation or copying) or forcing callers to consume two disjoint slices (`span1` and `span2`). Chart widgets (e.g. sparklines) and consumer models require a single contiguous `std::span<const T>` representing samples in chronological order. By pre-allocating twice the capacity and writing each sample to `pos` and `pos + capacity`, any window of length $\le \text{capacity}$ starting at the oldest element's index is guaranteed to be contiguous in memory. This achieves $O(1)$ zero-copy contiguous span access while strictly ensuring zero heap allocations on `push()` after construction.

**History:** Phase 1, monitoring infrastructure.

---

<a id="dc-43"></a>
### DC-43: Merged into DC-12

The optional CPU and memory samples are described in [DC-12](#dc-12).

---

<a id="dc-44"></a>
### DC-44: Merged into DC-12

The optional collections are described in [DC-12](#dc-12).

---

## Scheduler and collectors

`SamplingScheduler`'s threads, the collector interface, and what every collector does alike.

<a id="dc-17"></a>
### DC-17: `SamplingScheduler`: Responsive cancellation via `std::condition_variable_any` with `std::stop_token`

**Decision:** The scheduler thread tick loop uses `std::condition_variable_any::wait_until` with `std::stop_token` rather than `std::this_thread::sleep_for` or a polling sleep loop.

**Rationale:** A standard `sleep_for(m_interval)` cannot be interrupted until the interval expires, delaying application shutdown by up to 1000 ms. A polling loop (e.g. sleeping 10 ms repeatedly) wastes CPU cycles and adds wake jitter. In C++20, `std::condition_variable_any` natively accepts a `std::stop_token` in `wait_until`. If a stop is requested (e.g. when `SamplingScheduler::stop()` or the destructor is called), the condition variable wakes immediately and exits the thread. Furthermore, calculating `nextTick = tickStart + m_interval` prevents interval drift across ticks when collection work takes measurable time.

**History:** Phase 1, monitoring infrastructure.

---

<a id="dc-18"></a>
### DC-18: `ICollector<T>`: one virtual interface for every collector, and no concept

**Decision:** `template<typename T> class ICollector` (`monitoring/collector.h`) has one pure virtual `collect()` that returns the sample type `T`. Each collector derives from the alias for its sample (`ICpuCollector`, `IProcessCollector`, …), and `SamplingScheduler` holds each as a `std::unique_ptr<ICollector<T>>`. No concept describes collectors.

**Rationale:** `SamplingScheduler` stores collectors of different types (CPU, memory, disk, etc.), and their implementations are swapped between the real Windows collectors and the fakes that tests use. An un-templated `QObject` holding `std::unique_ptr<ICollector<T>>` keeps Qt's signal and slot metadata simple and avoids template bloat. `override` checks each collector's `collect()` against the interface at compile time. A `Collector<C, T>` concept beside the interface was dropped: no template takes collectors without virtual calls, and its only users were `static_assert`s in tests. If such a template is ever needed, it gets a concept written for that use.

**History:** Phase 1, monitoring infrastructure; maintenance (the `Collector<C, T>` concept removed).

---

<a id="dc-19"></a>
### DC-19: `SamplingScheduler`: Pointer snapshotting during `sampleOnce()` to avoid holding `m_collectorMutex` during collections

**Decision:** `sampleOnce()` locks `m_collectorMutex` only long enough to copy raw collector pointers (`get()`), then invokes `collect()` on each collector outside the lock. The invariant that collectors may only be registered before `start()` or after `stop()` is enforced with assertions and documented.

**Rationale:** In Phase 2, real Windows API collectors may take 0.5–1.5 ms (e.g. process enumeration) or query network tables. Holding `m_collectorMutex` for the duration of all collections would unnecessarily serialize collector configuration and sampling, and block any caller wanting to inspect or modify scheduler registration. Releasing the lock before collection keeps critical sections minimal ($O(1)$ pointer copies) while maintaining strict safety under the documented stopped-state registration invariant.

**History:** Code review F-1.

---

<a id="dc-37"></a>
### DC-37: Repeated collector failures logged once, with the recovery

**Decision:** Every OS query a collector repeats on each tick reports failures through a `RepeatedFailureLog` (`platform/windows/repeated_failure_log.h`). The first failure after a success is logged at the level the call site chooses, consecutive repeats are logged at `debug`, and the first success afterwards is logged at `info` with the failure count. The state lives with the query: in the default reader closures (`mutable` lambdas), in `WindowsAdapterReader`, or per volume for `GetDiskFreeSpaceExW`.

**Rationale:** Collectors run every second, so a persistent failure such as a BitLocker-locked fixed volume logged about 86,000 warnings a day. Because the logger flushes on `warn`, each of those also forced a disk write, and the 15 MB of rotating logs pushed out the useful history within a day or two ([code review F-11](code_reviews/phase_2.md#f-11-persistent-failures-are-logged-on-every-tick)). Logging only when the state changes follows the connectivity collector's change-only logging. It keeps the log readable and still leaves the full detail available at `debug`. Keeping the state beside each query, rather than in one shared object, lets a failing volume and a healthy one report independently. It also keeps the injected test readers unchanged.

**History:** Code review F-11.

---

<a id="dc-77"></a>
### DC-77: A slow-collector thread for processes, connectivity and network names, woken when an event-driven collector stores a change

**Decision:**

- **The thread:** `SamplingScheduler` owns a second `std::jthread` beside the tick thread. Every `slowInterval()` (2 seconds by default, `kDefaultSlowInterval`, adjustable with `setSlowInterval()`), it runs the registered slow collectors, processes, connectivity and network names, outside every lock. It then moves the results into a mutex-protected `SlowResults`. `sampleOnce()` runs only the fast collectors and copies the latest `SlowResults` into the snapshot under that mutex. Both threads wait on the scheduler's condition variable with their own stop tokens, so `stop()` interrupts either wait at once. Until the first slow refresh, snapshots have `processes == std::nullopt`, no network names, and `ConnectivityLevel::Unknown`. Public `refreshSlowCollectors()` runs the slow collectors synchronously for unit tests and asserts that the scheduler is stopped. The whole connectivity collector runs on this thread, not only its polling fallback. `registerSystemCollectors()`, which `main.cpp` calls, registers `ProcessCollector` with the others ([DC-110](#dc-110)). This resolved [known deviations D-1](known_deviations.md#d-1-sampling-scheduler-has-no-slow-collector-thread) and [D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread).
- **Event-driven collectors:** `monitoring::IEventDrivenCollector<T>` extends `ICollector<T>` with `setChangeListener()`, and `IConnectivityCollector` and `INetworkProfileCollector` are this interface. When the scheduler registers one of them, it sets a listener that marks an event refresh as pending under the sleep mutex and notifies the condition variable. The scheduler's destructor clears the listeners. The slow-collector thread waits until its next full refresh or until an event refresh is pending. An event refresh that comes before the full refresh is due runs only the event-driven collectors (`collectEventDrivenResults()`): it replaces the connectivity and the network names in the slow results, and keeps the process list and the time of the next full refresh. A full refresh also clears a pending request. `refreshEventDrivenCollectors()` runs the same step synchronously for tests.
- **When a listener is called:** `ConnectivityCollector` calls its listener after a notification that changed the cached status, but not after a polled change ([DC-39](#dc-39)). `NetworkProfileCollector` calls it once its worker has stored names that differ from the previous ones ([DC-100](#dc-100)). Both call it with their mutex held, so clearing a listener waits for a call in progress.

**Rationale:**

- **Off the tick:** a process refresh takes 3.5–6 ms on a 400-process desktop and about 18 ms the first time ([DC-75](#dc-75)), several times the roughly 1 ms the fast collectors share. Running it on the tick would delay every snapshot by that much and make the 1 Hz cadence uneven. One thread for all slow collectors is enough at this cadence and keeps a single place for the merge. Building the results before taking the mutex means the tick never waits for an enumeration, only for a move.
- **The whole connectivity collector:** moving all of it, rather than only its fallback, follows the roadmap. It needs no new interface for a collector to say whether it is polling, and the tick then makes no connectivity call at all.
- **No wait for the first refresh:** not waiting keeps `start()` and the first tick immediate. On the first snapshot connectivity reads Unknown, like the CPU sample, which has no baseline yet; the second snapshot has both.
- **The list copied on each tick:** each tick copies the latest process list into its snapshot, so a list is reused for about two ticks. Sharing it between snapshots without copying would change the snapshot's value type, so that waits until profiling of the process table shows a need.
- **Woken by a change:** collected only every 2 seconds and merged by the tick up to a second later, a notified change would reach the UI up to about 3 seconds after Windows reported it, because the scheduler would be polling the collector's cache. Waking the slow thread lets the notification drive the update. In a unit test with a 60-second slow interval, a change reaches the snapshots within a few ticks, and no extra process enumeration runs. The network names that a change makes stale are read again at once too ([DC-100](#dc-100)). Three alternatives were rejected:
  - **An extra snapshot on a change:** it would put two samples into the histories within one second, and every chart and rate assumes one sample per tick.
  - **Connectivity on the tick:** its polling fallback and the names' COM query would run on the tick again ([D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)).
  - **A full slow refresh on a change:** processes would be enumerated off their cadence, and changes come in bursts while an adapter reconnects.
- **The listener:** the scheduler sets it, rather than the collector's constructor taking one, because `main.cpp` creates the collectors before handing them over and the scheduler is what needs waking. Calling the listener with the collector's mutex held lets the scheduler's destructor clear it safely. The listener refers to the scheduler, and once `setChangeListener({})` returns, no call can still be running. Locks are always taken in the same order, the collector's mutex and then the scheduler's sleep mutex, and the scheduler never calls a collector while it holds the sleep mutex. A notification that repeats the cached status, such as the initial notification after the seed, does not wake the thread.

**History:** Phase 4, process collector; Phase 5, network view (an event-driven collector wakes the thread); code review F-38 (the network names wake it after their read, for [DC-100](#dc-100)); maintenance (DC-101 merged in).

---

<a id="dc-101"></a>
### DC-101: Merged into DC-77

Event-driven collectors waking the slow-collector thread are described in [DC-77](#dc-77).

---

## CPU, memory, disk and uptime collectors

The fast collectors behind the dashboard and the Performance tab.

<a id="dc-25"></a>
### DC-25: Total CPU utilization formula: `(deltaKernel + deltaUser - deltaIdle) / (deltaKernel + deltaUser)`

**Decision:** Calculate total CPU utilization by subtracting `deltaIdle` from `(deltaKernel + deltaUser)` and dividing by `(deltaKernel + deltaUser)` over monotonic elapsed time.

**Rationale:** Windows `GetSystemTimes` returns cumulative idle, kernel, and user times in 100 ns `FILETIME` ticks across all logical processors. A key characteristic of Windows kernel accounting is that `lpKernelTime` already includes the execution time of the idle thread. Thus, total system capacity elapsed across all processors is `deltaKernel + deltaUser`, and active busy time is `(deltaKernel + deltaUser) - deltaIdle`. This ratio is mathematically invariant to CPU frequency scaling (Intel SpeedStep / AMD Cool'n'Quiet), processor group topologies, and core count. We guard against multi-core non-atomic counter jitter by clamping usage to $[0.0\%, 100.0\%]$. When no rate can be computed (non-positive elapsed time, a counter that went backwards, or no elapsed CPU time), `calculateCpuUsage` returns `std::nullopt` rather than 0% (see [DC-12](#dc-12)).

**History:** Phase 2, CPU collector; Phase 2, known deviation D-7 (no usage rather than 0%, for [DC-12](#dc-12)).

---

<a id="dc-28"></a>
### DC-28: Per-core CPU utilization: `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` dynamic resolution and resizing-buffer pattern

**Decision:** Resolve `NtQuerySystemInformation` dynamically from `ntdll.dll` via `GetModuleHandleW` / `GetProcAddress` and query `SystemProcessorPerformanceInformation` (class 8) using a resizing-buffer loop. Per-core utilization is calculated by applying the same `calculateCpuUsage` logic to each core's `IdleTime`, `KernelTime`, and `UserTime`.

**Rationale:** `NtQuerySystemInformation` is the native NT kernel interface used by Task Manager and Process Explorer to query per-processor counters. In Windows, `ntdll.dll` is mapped into every user-mode process at process creation, so resolving the function pointer dynamically via `GetProcAddress` avoids static import dependencies while ensuring universal compatibility. The returned `SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION` structure includes idle time within `KernelTime` for each individual core, mirroring the aggregate behavior of `GetSystemTimes`. The resizing-buffer loop pattern starts with an estimated allocation equal to the detected logical core count and automatically handles `STATUS_INFO_LENGTH_MISMATCH` (0xC0000004) if processor topology or active counts change.

**History:** Phase 2, CPU collector.

---

<a id="dc-29"></a>
### DC-29: Multi-processor-group support (>64 logical cores): `NtQuerySystemInformationEx` iteration and total CPU aggregation

**Decision:** Use `NtQuerySystemInformationEx` to iterate through all active processor groups (`0 .. GetActiveProcessorGroupCount() - 1`), with fallback to `NtQuerySystemInformation` for Group 0 if `Ex` resolution or runtime calls fail. When more than one processor group is active (`GetActiveProcessorGroupCount() > 1`), compute total CPU usage by aggregating `idleTime`, `kernelTime`, and `userTime` across all cores rather than relying on `GetSystemTimes`. The decision is keyed on the group count, not the core count, because Windows can form several groups on machines with 64 or fewer logical processors (for example one group per NUMA node, or `bcdedit /set groupsize`). If the per-core query fails on such a system, no total is reported for that tick rather than presenting `GetSystemTimes`' single group as the whole machine ([code review F-15](code_reviews/phase_2.md#f-15-cpu-multi-group-aggregation-keys-on-core-count-not-group-count)).

**Rationale:** On 64-bit Windows, processor affinities are limited to 64 bits per group, so high-core machines (>64 logical processors) are partitioned into multiple processor groups. On such machines, `GetSystemTimes` and legacy `NtQuerySystemInformation` only report statistics for the primary group of the calling thread, rendering workloads in secondary groups invisible. Calling `NtQuerySystemInformationEx` with `USHORT group` in the input buffer retrieves each group's cores sequentially, guaranteeing that `CpuSample::coreUsagePercents` captures all logical cores in continuous processor index order. Furthermore, aggregating all cores' counter deltas to compute `totalUsagePercent` on multi-group systems ensures accurate system-wide utilization metrics regardless of how the OS scheduler distributes threads across groups.

**History:** Phase 2, CPU collector; code review F-15 (keyed on the group count, not the core count).

---

<a id="dc-31"></a>
### DC-31: Memory sampling: `GlobalMemoryStatusEx` and commit charge calculation

**Decision:** Sample physical and virtual memory using `GlobalMemoryStatusEx`. Compute `commitCurrent` as `ullTotalPageFile - ullAvailPageFile`. Clamp memory usage percent defensively to $[0.0\%, 100.0\%]$.

**Rationale:** `GlobalMemoryStatusEx` is an ultra-fast kernel query (< 1 microsecond) with zero external or registry dependencies. In `MEMORYSTATUSEX`, `ullTotalPhys` and `ullAvailPhys` report physical RAM. `ullTotalPageFile` represents the current system commit limit (physical RAM plus current pagefile allocation), and `ullAvailPageFile` represents the remaining commit space available to processes before allocations fail. Subtracting available pagefile from total pagefile yields the active system commit charge (`commitCurrent`), matching Task Manager's commit metric ("Committed x / y GB"). Defensive boundary clamping protects against transient kernel counter jitter.

**History:** Phase 2, memory and disk collector.

---

<a id="dc-32"></a>
### DC-32: Disk space sampling: `GetDiskFreeSpaceExW` strictly filtered to `DRIVE_FIXED`

**Decision:** Enumerate drive letters via `GetLogicalDrives()`, inspect each drive root using `GetDriveTypeW()`, and call `GetDiskFreeSpaceExW` only for drives returning `DRIVE_FIXED`.

**Rationale:** Calling disk space APIs on removable drives (`DRIVE_REMOVABLE`), optical media (`DRIVE_CDROM`), or remote shares (`DRIVE_REMOTE`) can cause blocking network timeouts or trigger Windows system modal dialogs ("Insert disk into drive..."), freezing the background scheduler tick. Restricting collection strictly to `DRIVE_FIXED` ensures fast, non-blocking queries of all local SSDs and HDDs. If `GetDiskFreeSpaceExW` fails for a specific drive (e.g. unformatted volume or BitLocker locked partition), that drive is skipped gracefully without interrupting collection for other healthy drives.

**History:** Phase 2, memory and disk collector.

---

<a id="dc-42"></a>
### DC-42: System uptime: `GetTickCount64` as a fast collector with an optional snapshot field

**Decision:** `UptimeCollector` returns `std::chrono::milliseconds` from `GetTickCount64` and runs on the scheduler tick as a fast collector. `SystemSnapshot::uptime` is `std::optional<std::chrono::milliseconds>`, empty when no uptime collector is registered.

**Rationale:** `GetTickCount64` is a trivial, non-failing call with a 64-bit counter, so unlike `GetTickCount` it does not wrap after 49.7 days. Its 10–16 ms resolution is irrelevant for displaying uptime. It counts time spent in sleep and hibernation, which is what users expect from "uptime". `QueryUnbiasedInterruptTime` was rejected because it excludes sleep time. The snapshot field is optional so a missing collector is explicit rather than a zero uptime (per the domain rules). Uptime is computed from a monotonic tick counter, not by subtracting a boot timestamp from wall-clock time, so it is unaffected by clock changes.

**History:** Phase 2, dashboard view.

---

<a id="dc-55"></a>
### DC-55: CPU base speed from `CallNtPowerInformation`, read once; no current clock speed

**Decision:** `CpuSample::baseSpeedMhz` carries the processor's rated clock speed. `CpuCollector`'s default constructor reads it once with `CallNtPowerInformation(ProcessorInformation)`, using a buffer sized for all active logical processors (`GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`), and reports the highest `MaxMhz` (`baseSpeedFromMaxMhz`). The value is `std::nullopt` when the call fails or every processor reports 0, as some virtual machines do. It feeds the Performance tab's "Base speed" readout. There is no current-speed readout.

**Rationale:** The rated speed does not change while the system runs, so one call at startup is enough. The highest value makes a hybrid processor report its performance cores. A current-speed readout was rejected: on the development machine the same structure's `CurrentMhz` equalled `MaxMhz` (4700 MHz) at idle, which is a known limitation of the field on current processors. Task Manager computes its "Speed" from the "% Processor Performance" counter, which would need PDH, and PDH is not used for core metrics ([DC-02](#dc-02)). Showing no current speed is better than showing a wrong one. `PROCESSOR_POWER_INFORMATION` is documented but missing from the SDK headers, so `cpu_collector.cpp` declares it, as it already does for `SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION`.

**History:** Phase 3, Performance tab.

---

<a id="dc-56"></a>
### DC-56: Memory detail and system activity counts from `GetPerformanceInfo`, as fast collectors

**Decision:** `GetPerformanceInfo` supplies the Performance tab's memory detail and its process, thread, and handle counts:
- `MemoryCollector` calls it after `GlobalMemoryStatusEx` and adds `systemCacheBytes` (`SystemCache`; see [DC-63](#dc-63)), `pagedPoolBytes`, and `nonPagedPoolBytes` (page counts times `PageSize`) to `MemorySample` as optional fields. If only this call fails, or the page size is 0, the memory sample is still reported, with these fields empty.
- A new `SystemActivityCollector` reports `SystemActivitySample` (process, thread, and handle counts) in the new `SystemSnapshot::activity` field. It returns `std::nullopt` on failure or for a process count of 0.

Both run on the scheduler tick as fast collectors. The call is wrapped once in `performance_info_reader.h`, and each collector has its own reader, and so its own `RepeatedFailureLog`.

**Rationale:** On the development machine (367 processes, about 200,000 handles) `GetPerformanceInfo` took about 19 µs per call, so it reads kernel counters rather than enumerating processes, and it fits the fast-collector budget. It is documented, has no registry or PDH dependency, and provides exactly the counts Task Manager shows, so it follows the direct-Win32 rule for core metrics. `GlobalMemoryStatusEx` stays the source of the core memory figures, as the rules require. Counting processes, threads, and handles from the Phase 4 `SystemProcessInformation` enumeration was rejected: it runs every 2–5 s on the slow-collector thread and does not exist yet, and these readouts should update at 1 Hz. The Phase 4 process list may still report its own totals. Calling `GetPerformanceInfo` from two collectors costs a second 19 µs call per tick. In return, memory detail stays in `MemorySample` and the counts get their own collector, and each collector remains independently testable. Merging them into one collector would either put process counts in a memory type or split one query's results across snapshot fields inside the scheduler.

**History:** Phase 3, Performance tab; code review F-22 (`systemCacheBytes`, for [DC-63](#dc-63)).

---

## Process collector

The process list, each process's details and rates, and the processes the monitor never opens.

<a id="dc-13"></a>
### DC-13: Process details status kept in-band, and the processes the monitor never opens

**Decision:**

- **In-band status:** every process stays in the list, and `ProcessInfo::detailsStatus`, a `ProcessDetailsStatus`, says whether its image path and command line could be read and, if not, why:
  - `NotQueried` (the default): the process is never opened (see below), or the `ProcessInfo` did not come from a collector that reads details.
  - `Available`: both details were read.
  - `AccessDenied`: Windows denied opening the process or reading a detail.
  - `Unavailable`: a detail could not be read for another reason, such as a process that exited before it was queried, or a kernel process with no image file or command line (System, Registry).

  A detail that could be read is set whatever the status, so the status describes the query as a whole. The metrics come from the process list whatever the status.
- **Never opened:** `isNeverOpenedProcess()` (`platform/windows/process_access_policy.h`) takes the `ProcessInfo`. It is true for `lsass.exe` and `csrss.exe`, matched by image name ignoring ASCII case, and for a process with an empty image name and any PID but 0. `ProcessCollector` wraps every details reader it is given with `skippingNeverOpenedProcesses()`, so the reader is never called for these processes, and they get `NotQueried`. The Windows details reader checks again just before `OpenProcess` ([DC-75](#dc-75)), and the terminator refuses them as `NeverOpened` before any `OpenProcess` call ([DC-87](#dc-87)). The filter went in before the `OpenProcess` reader, so no commit contains code that would open them.
- **The Idle process:** the System Idle Process (PID 0) has no name in the list ([DC-71](#dc-71)), and it was the only process without one on the development machine. It keeps its own handling: the details reader gets `Unavailable` from `OpenProcess`, and the terminator refuses it as `NotOwnedByCurrentUser`.
- **Callers:** the comments on `terminateOwnedProcess()`, `makeWindowsProcessDetailsReader()` and `ProcessDetailsReader` say that the image name must be the one the process list reported.
- **Explanations:** the logs and the UI give a process without a name a reason of its own, not the one for `lsass.exe` and `csrss.exe`. Its name tooltip (`processDetailsToolTip()`) and the message when a request to end it is refused (`processTerminationMessage()`) say that Windows listed it without a name, so it could be one of the processes the monitor never opens.

**Rationale:**

- **In the list, not beside it:** `NtQuerySystemInformation` returns the name, PID and metrics of every process, protected ones included, without opening any ([DC-01](#dc-01)). Omitting a process whose details are missing would silently hide it, while Task Manager shows all processes. A separate error list alongside `vector<ProcessInfo>` would force all UI code to merge two collections. In-band, the snapshot stays one coherent list, and the UI shows a partial row: the name and metrics, with an indicator for the missing details ([DC-85](#dc-85)), as Task Manager and Process Explorer do.
- **An enum, not a flag:** Phase 1 had a `ProcessInfo::accessDenied` flag, defined as "the process metrics could not be retrieved at all", which never happens. Only the details read by opening the process can be missing, and for several reasons the UI should tell apart. A process the monitor deliberately never opens is not one that refused access, and neither is a kernel process that has no command line. An enum is the error variant the domain rules ask for, and `NotQueried` as the default means a `ProcessInfo` never claims a query that did not happen.
- **`lsass.exe` and `csrss.exe`:** opening either process, even with `PROCESS_QUERY_LIMITED_INFORMATION`, trips credential-theft heuristics in most EDR products ([architecture.md — Antivirus and EDR considerations](architecture.md#antivirus-and-edr-considerations)). The process list already has their metrics, so only the details are lost, and an unelevated query would usually be refused anyway. Filtering in the collector, rather than inside the Windows reader only, makes the rule hold for any reader and lets unit tests show with a fake that the reader never sees these processes. Matching by name alone also skips any other process with one of these names. That costs only that process's path and command line, and never risks opening the real one, whose path does not need to be known.
- **A process without a name:** the rule that the monitor never opens these two processes has no exceptions, but the check compares a name that its caller supplies, and an empty name would pass it. Two paths produce one. The parser leaves a name empty when its characters do not lie inside the returned buffer ([DC-71](#dc-71)), and a caller of `terminateOwnedProcess()` can pass only an identity, as the integration tests do for PID 0. Either way, the process could be one of the two. Refusing a process without a name makes the check fail closed. It costs only the path, the command line and End process for such a process. The Idle process is exempt because the list never names it and `OpenProcess` rejects PID 0, so it cannot be either process, and its own status and refusal explain it better. A caller that passes a wrong name that is not empty still gets past the check. The function cannot know the real name without opening the process or reading the list again, so the comments state the requirement instead.

**History:** Phase 1, domain types (an `accessDenied` flag); Phase 4, process collector (`ProcessDetailsStatus`, and `lsass.exe` and `csrss.exe` never opened); code review F-32 (a process without an image name never opened); maintenance (DC-72, DC-74 and DC-94 merged in).

---

<a id="dc-70"></a>
### DC-70: Process list read into a kept, growing buffer through an injected query

**Decision:** `SystemProcessInformationReader` calls `NtQuerySystemInformation(SystemProcessInformation)`, resolved from `ntdll.dll`, in a resizing-buffer loop. When the call returns `STATUS_INFO_LENGTH_MISMATCH` or `STATUS_BUFFER_TOO_SMALL`, the buffer grows to the reported size plus an eighth (or doubles when no size is reported) and the call is repeated, up to five calls in all. The buffer starts at 256 KiB, is kept between reads, and never shrinks. A reported size above 64 MiB fails the read instead of being allocated. The query is a `std::function` that takes a `std::span<std::byte>` and returns the NTSTATUS as an `int32_t`, so unit tests can fake it without Windows headers. `resolveSystemProcessInformationQuery()` returns the real one.

**Rationale:** The list's size is not known in advance and changes between calls, so the API reports the size it needs and the caller retries. The headroom covers processes and threads created between the failed call and the retry, so one retry is normally enough. A desktop's list is a few hundred KiB: 256 bytes per process, 80 per thread, plus the image names. Keeping the buffer therefore costs little and saves an allocation and a failed call on every refresh. The 64 MiB cap is far above any real list, but it stops a corrupt reported size from exhausting memory, which would end the process because exceptions are not used. With the fake query, unit tests cover retry paths that a real machine rarely takes, as [architecture.md — Testing strategy](architecture.md#testing-strategy) asks for buffer resizing. An integration test starts the real query from a 1 KiB buffer to force the loop.

**History:** Phase 4, process collector; code review F-33 (growth without a reported size corrected to doubling).

---

<a id="dc-71"></a>
### DC-71: Process list parsed through a full `SYSTEM_PROCESS_INFORMATION` layout checked against `winternl.h`

**Decision:** `platform/windows/system_process_information.h` defines `FullSystemProcessInformation`, the complete entry layout with the field names Windows uses. `static_assert`s tie its size and field offsets to the partial `SYSTEM_PROCESS_INFORMATION` in `winternl.h`. `parseSystemProcessInformation()` follows `NextEntryOffset` from the start of the buffer. It copies each entry out with `memcpy` and turns it into a `ProcessInfo`, and returns `std::nullopt` if an entry does not fit in the buffer. Image names are read with their `Length` (they need not be null-terminated), and only when they lie inside the buffer; otherwise the name is empty. The field mapping:

- `privateWorkingSetBytes` is `WorkingSetPrivateSize`, the working set without shared pages (Task Manager's "Memory").
- `privateBytes` is `PrivatePageCount`, the commit charge (Process Explorer's "Private Bytes", Task Manager's "Commit size").
- `ioReadBytes` and `ioWriteBytes` are `ReadTransferCount` and `WriteTransferCount`. They count all of the process's read and write I/O, including pipes and devices, not only disk. The domain field comments say so.
- CPU times are truncated from 100 ns units to whole milliseconds, the domain unit ([DC-14](#dc-14)).
- A negative value in a signed counter (`LARGE_INTEGER`: the CPU times, `WorkingSetPrivateSize` and the transfer counts) reads as zero.
- The creation time converts from the FILETIME epoch to `system_clock` with fixed arithmetic ([DC-08](#dc-08)).
- The Idle process (PID 0) has no image name in the list, and its `imageName` stays empty. Naming it is left to the UI.

UTF-16 to UTF-8 conversion moved to `platform/windows/utf8_conversion.h` as `wideToUtf8(std::wstring_view)`, written in portable C++. The network collector uses it too, in place of its own `WideCharToMultiByte` wrapper. Unpaired surrogates become U+FFFD, as with `WideCharToMultiByte`.

**Rationale:** `winternl.h` hides the creation time, user and kernel times, parent PID, and I/O counters in `Reserved` fields, and the collector needs all of them. A complete definition names them, and the `static_assert`s catch a layout mistake at compile time instead of as wrong numbers at run time. Copying each entry avoids reading through a cast pointer and any dependence on the buffer's alignment. The bounds checks stop a corrupt list from reading past the buffer. The parser's tests build lists from SDK types (`UNICODE_STRING`, `SYSTEM_THREAD_INFORMATION`), so they live in `tests/platform/`, which may not call Windows APIs, and the parser calls the conversion. A conversion in plain C++ keeps those tests free of OS calls and can be unit tested directly. `PrivatePageCount` is the field `VM_COUNTERS_EX` calls `PrivateUsage`, the documented commit charge. The transfer counts are the only I/O byte counters the list has, so the comments describe what they include rather than calling them disk I/O.

**History:** Phase 4, process collector; code review F-29 (the private working set, for [DC-79](#dc-79)).

---

<a id="dc-72"></a>
### DC-72: Merged into DC-13

`ProcessDetailsStatus` is described in [DC-13](#dc-13).

---

<a id="dc-73"></a>
### DC-73: Process details cached by `(PID, creation time)` with a domain `ProcessIdentity` key

**Decision:** `domain/process_identity.h` defines `ProcessIdentity` (`pid`, `creationTime`) with `ProcessIdentityHash`, and `ProcessInfo::identity()` returns it. `ProcessIdentityCache` (`platform/windows`, no Windows types) maps identities to `ProcessDetails`. `ProcessCollector` passes it every new list. For each process, the cache uses its entry if it has one and otherwise calls the injected `ProcessDetailsReader` once, caching the result whatever its status. It then erases every entry the list did not mention, using a generation counter instead of rebuilding the map. Without a details reader, every process's details stay `NotQueried`.

**Rationale:** Opening a process and reading its path and command line costs a few system calls per process, and the details cannot change while the process lives, so [architecture.md](architecture.md#windows-api-choices) asks for them to be read lazily and cached by `(PID, creation time)`. Keying by PID alone would show a previous process's path and command line for a new process that reused its PID. Erasing on every refresh bounds the cache by the process list and removes an exited process's entry before its PID can come back. Caching failures too means a process that denies access is not opened again every refresh; the cost is that a rare transient failure lasts until the process exits. The identity type is in `domain` because the CPU usage baselines and the Phase 4 process table need the same key. The cache takes the reader as a function, as the other collectors take their readers, so its tests cover PID reuse and eviction without Windows.

**History:** Phase 4, process collector.

---

<a id="dc-74"></a>
### DC-74: Merged into DC-13

The processes the monitor never opens are described in [DC-13](#dc-13).

---

<a id="dc-75"></a>
### DC-75: Process details read with a limited handle, checked against the identity, and logged at debug

**Decision:** `makeWindowsProcessDetailsReader()` returns the production `ProcessDetailsReader`, which `ProcessCollector`'s default constructor uses. For each new process it:

1. Refuses `lsass.exe` and `csrss.exe` by name ([DC-13](#dc-13)).
2. Opens the process with `PROCESS_QUERY_LIMITED_INFORMATION` only, held in a `std::unique_ptr` that calls `CloseHandle`.
3. Compares the creation time from `GetProcessTimes` with the identity's. On a mismatch, the details are `Unavailable`.
4. Reads the image path with `QueryFullProcessImageNameW`, starting at `MAX_PATH` and doubling up to 32,768 characters.
5. Reads the command line with `NtQueryInformationProcess(ProcessCommandLineInformation)` (class 60, which `winternl.h` does not name), resolved from `ntdll.dll` once when the reader is made. The buffer starts at 1 KiB and grows to the reported size, for up to three calls.

`ERROR_ACCESS_DENIED` and `STATUS_ACCESS_DENIED` map to `AccessDenied`; every other failure maps to `Unavailable`. Each detail keeps its own value, and the combined status is `AccessDenied` if either was denied ([DC-13](#dc-13)). Failures are logged at `debug`, and the architecture's logging table now lists them there.

On the development machine (Release, unelevated, 413 processes) the first `collect()`, which opens every process, took about 18 ms. Later ones took 3.5–6 ms: `NtQuerySystemInformation` itself 2.3–3.8 ms for a 1 MB list, parsing 0.1 ms, and the cache filling in the details about 1 ms. 254 processes returned both details, 155 denied access, 3 were never opened (`lsass.exe` and two `csrss.exe`), and 1 had none (the Idle process).

**Rationale:** `PROCESS_QUERY_LIMITED_INFORMATION` is the only access the security rules allow, and both queries need nothing more. `ProcessCommandLineInformation` reads the command line without `PROCESS_VM_READ`, which the rules forbid and which reading the PEB directly would need. Between reading the list and calling `OpenProcess`, a process can exit and its PID be reused. Without the creation-time check, the new process's path and command line would be cached under the old identity, the PID-alone mistake the identity rule exists to prevent. `GetProcessTimes` needs no access beyond what the handle already has. An unelevated monitor is refused by about a third of the processes on a desktop, every time it starts, so a warning per process would add about 150 warnings, and a log flush for each, to every start. The UI shows the status per process, and `debug` keeps the detail for diagnosis. The measured enumeration cost is higher than the 0.5–1.5 ms the architecture estimated, which confirms that it belongs on the slow-collector thread rather than the 1 Hz tick.

**History:** Phase 4, process collector.

---

<a id="dc-76"></a>
### DC-76: Per-process CPU usage and I/O rates computed in the collector, CPU as a share of all logical processors

**Decision:** `ProcessInfo::cpuUsagePercent` (`std::optional<float>`), `ioReadBytesPerSec` and `ioWriteBytesPerSec` (`std::optional<uint64_t>`) are set by `ProcessCollector` with the pure function `calculateProcessRates()`. The CPU usage is the change in user + kernel time since the process's baseline, divided by the elapsed `steady_clock` time times the number of active logical processors in all processor groups, clamped to 0–100. The I/O rates are the changes in the read and write transfer counts over the same time, from `calculateByteRate()` (`platform/windows/byte_rate.h`), which the network collector also uses. A baseline (`ProcessRateBaseline`) holds the CPU time, both I/O byte counters and the time they were read. Baselines are keyed by `ProcessIdentity` and replaced on every successful read, so exited processes drop out and a reused PID starts with no value. Every rate is `std::nullopt` the first time a process is seen and when no time has elapsed; each is also `std::nullopt` when its own counter went backwards, and the CPU usage when the processor count is unknown. The timestamp is taken right after the list is read, before the details are read for new processes. A failed read keeps the baselines, so the next success measures over the longer interval.

**Rationale:** The roadmap puts the calculation in the collector, which owns the previous sample. A rate kept beside the cumulative times ([DC-14](#dc-14)) spares the UI the bookkeeping and keeps it identical for every consumer. The UI could not compute the rates correctly anyway: the list is read every 2 seconds but reaches it with every 1 Hz snapshot, so a rate between snapshots would alternate between zero and double ([DC-79](#dc-79)). Dividing by every logical processor puts each process on the scale of the total CPU usage, as Task Manager's Processes tab and this monitor's CPU card do. A single-threaded process on 16 processors therefore peaks at 6.25%. The column adds up to slightly less than the total, not to the total. Time spent on interrupts and deferred procedure calls is charged to no process (Task Manager lists it as "System interrupts"), and a process that exited during the interval is no longer listed, so its last share is lost. In the Phase 4 code review, the column added up to 98.7% over a 2-second interval, including the Idle process's 73.8%. Keying by identity keeps a new process from inheriting a previous process's baseline, which with PID alone could produce a huge or negative delta. `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)` is used because process CPU times cover every group, unlike `GetSystemTimes` ([DC-29](#dc-29)). Taking the timestamp before the details are read keeps the up to 18 ms that the first details pass takes out of the interval. Windows credits CPU time at clock-tick granularity (about 15.6 ms), so a busy process can briefly exceed its share of the interval, hence the clamp. Millisecond CPU times lose nothing at that granularity.

**History:** Phase 4, process collector; Phase 4, process explorer view (I/O rates, for [DC-79](#dc-79)); code review F-33 (what the CPU column adds up to).

---

<a id="dc-94"></a>
### DC-94: Merged into DC-13

Processes without an image name are described in [DC-13](#dc-13).

---

## Network adapter collector

Each adapter's counters, identity, addresses and DNS servers.

<a id="dc-34"></a>
### DC-34: Network adapter traffic & throughput: `GetIfTable2` with RAII `FreeMibTable` and delta rates

**Decision:** Use `GetIfTable2` to read 64-bit cumulative byte counters (`InOctets`, `OutOctets`), negotiated link speed, and operational status. Wrap allocated `MIB_IF_TABLE2` memory in a custom RAII deleter calling `FreeMibTable`. Compute instantaneous throughput rates (`inBytesPerSec`, `outBytesPerSec`) from counter deltas divided by elapsed monotonic time (`std::chrono::steady_clock`).

**Rationale:** `GetIfTable2` is the modern Windows IP Helper interface that returns 64-bit octet counters without registry or PDH dependencies. Unlike legacy 32-bit `GetIfTable`, 64-bit counters will not roll over under modern gigabit and 10 GbE traffic within days. RAII wrapping via `std::unique_ptr<MIB_IF_TABLE2, MibTableDeleter>` guarantees memory is safely released on all return paths. Throughput rates are computed per adapter baseline over measured monotonic durations. A rate is `std::nullopt`, never 0, when it cannot be computed: the first sample for an adapter, zero elapsed time, or a counter that went backwards (e.g. an interface reset), where the delta across the reset is unknown. Each direction is evaluated independently, so a reset of one counter does not hide the other direction's rate. A genuinely idle link still reports 0. See [known deviation D-6](known_deviations.md#d-6-network-throughput-reports-zero-when-no-baseline-exists).

**History:** Phase 2, network collector; Phase 2, known deviation D-6 (no rate rather than 0).

---

<a id="dc-35"></a>
### DC-35: Adapter identity and details: `GetAdaptersAddresses` matched to `GetIfTable2` by LUID, cached, and reduced to what can be used

**Decision:**

- **Two APIs:** `GetIfTable2` gives each interface's counters, link speed and operational status on every tick ([DC-34](#dc-34)). `GetAdaptersAddresses` (with `AF_UNSPEC` and `GAA_FLAG_INCLUDE_PREFIX`) gives friendly names, device descriptions, IPv4 and IPv6 addresses, and DNS servers. Its adapters are matched with `GetIfTable2`'s rows by `NET_LUID::Value`, falling back to `IfIndex`.
- **Rows dropped:** loopback interfaces (`IF_TYPE_SOFTWARE_LOOPBACK`) and NDIS filter interfaces (`InterfaceAndOperStatusFlags.FilterInterface`) are dropped at the platform boundary.
- **Identity:** `NetworkSample::interfaceId` is the key of the collector's rate baselines: the interface LUID, or the interface index when Windows reports no LUID, or a hash of the name when it reports neither. Both tabs key each adapter's history, chart and card by it, and use the names only for display ([DC-53](#dc-53), [DC-96](#dc-96)). A renamed adapter keeps its chart and history, and its title changes.
- **Flags:** `isHardwareInterface` carries `InterfaceAndOperStatusFlags.HardwareInterface`. `hasIpInterface` is set when `GetAdaptersAddresses` lists the adapter, which it does only for adapters bound to IP. The Performance tab charts an Up adapter only if it has one of the two flags, and the Network tab lists such adapters by default, unless Windows hides them (`isHidden`, [DC-96](#dc-96)). Virtual adapters without an IP interface stay in the snapshots, and their history is still recorded.
- **Cache:** `NetworkCollector` reads `GetIfTable2` on every tick but keeps the `GetAdaptersAddresses` results in an `AdapterDetailsCache`. The cache re-reads them when an address or interface change notification (`NotifyUnicastIpAddressChange`, `NotifyIpInterfaceChange`) has marked it stale, when `GetIfTable2` shows an interface that was not present at the last refresh, or when 30 seconds (`kMaxAge`) have passed. A failed re-read clears the details, so every adapter has `hasIpInterface` cleared, and is retried on the next tick. The production reader, `WindowsAdapterReader`, lives in `network_collector.cpp` and is owned by the collector. The refresh policy is a separate class with no Windows types, so `tests/unit/` can test it.
- **Usable addresses:** `queryAdapterDetails()` keeps each unicast address's duplicate address detection state (`DadState`), which `toAddressState()` maps to the platform type `AddressState`. The cache and `RawNetworkAdapter` carry each address as a `UnicastAddress`, its text with its state. `calculateNetworkSamples()` keeps Preferred addresses, keeps Deprecated ones only while the adapter is Up, and drops Tentative, Duplicate and Invalid ones. A state the mapping does not know counts as Invalid. The state stays in the platform module.
- **Placeholder DNS servers:** `queryAdapterDetails()` drops the DNS servers `fec0:0:0:ffff::1`, `::2` and `::3` (`isPlaceholderDnsServer()`), whatever the adapter's status. Both functions are in the internal header `ip_address_mapping.h`, which `tests/platform/` tests.
- **Address flags and zone indexes:** `NetworkSample::ipAddresses` is a list of `domain::IpAddress`: the address's text, `isTemporary` and `isLinkLocal`, which the platform module sets. `isTemporaryAddress()` maps a unicast address's suffix origin: random means temporary. `isLinkLocalAddress()` is true for IPv6 fe80::/10 and IPv4 169.254.0.0/16. `formatIpAddress()` appends a link-local IPv6 address's scope ID as its zone index, e.g. "fe80::d347:c091:2487:3b32%16", for unicast addresses and DNS servers alike. The DNS servers are a list of strings. Both tabs mark each address from its flags ([DC-96](#dc-96)).

**Rationale:**

- **Two APIs matched by LUID:** `GetIfTable2` provides high-precision counters and link state but no network addresses or DNS configuration, which `GetAdaptersAddresses` provides. The 64-bit `InterfaceLuid.Value` pairs the two exactly and without collisions across physical, virtual, and Wi-Fi adapters.
- **Rows dropped:** loopback is a local pseudo-interface (`127.0.0.1`, `::1`). `GetIfTable2` also returns one row per NDIS lightweight filter bound to an adapter (for example "Ethernet-WFP Native MAC Layer LightWeight Filter-0000" and "Ethernet-QoS Packet Scheduler-0000"). These rows are Up and carry the same counters as the adapter, so a sum over all Up rows counted the physical traffic several times ([code review F-9](code_reviews/phase_2.md#f-9-network-throughput-total-counts-the-same-traffic-several-times)). They mean nothing to a user and Task Manager does not show them. Virtual adapters such as VPN tunnels and Hyper-V vEthernet are kept, because the Network tab lists them, but they are marked non-hardware: their traffic also crosses a physical adapter, so totals must not include them ([DC-46](#dc-46)).
- **Identity by LUID:** names are not unique. Descriptions repeat ("WAN Miniport (IP)", "Microsoft Wi-Fi Direct Virtual Adapter"), and when the Performance tab keyed adapters by `adapterName`, two adapters with the same name pushed into one history, which then held two samples per tick and drew both adapters as one line. Renaming a connection also restarted its history. The LUID is unique per interface and does not change on rename, and the collector already computed it for its baselines. It is an opaque `uint64_t` in the domain, so no Windows type crosses the boundary.
- **`hasIpInterface`:** Windows keeps several hidden virtual adapters Up permanently, such as WAN Miniport (IP), (IPv6) and (Network Monitor). They have no IP interface and never carry traffic, yet on the review machine they took three of the Performance tab's five network charts. `MIB_IF_ROW2` has no "hidden" flag, and `ConnectorPresent` is false for the WAN Miniports and for VPN tunnels alike, so `GetIfTable2` cannot tell them apart. IP binding can. Testing `ipAddresses` for emptiness gives the same result in practice, because link-local addresses are kept, but an explicit flag states the reason and does not depend on an adapter having an address yet. Hardware adapters are charted without an IP interface, because their link can still carry traffic. While `GetAdaptersAddresses` fails, virtual adapters leave the charts until the next successful read and hardware adapters stay, which is acceptable for a chart list.
- **Cached details:** on the review machine `GetAdaptersAddresses` took about 2 ms per call, compared with about 0.7 ms for `GetIfTable2`. That alone exceeded the fast-collector budget, and it was spent re-reading data that changes only when the network configuration does ([code review F-10](code_reviews/phase_2.md#f-10-getadaptersaddresses-runs-on-every-scheduler-tick)). The notifications are an event-driven source under the architecture's rules: the callbacks only set the stale mark, the registrations are RAII members declared after the cache so they are cancelled first, and the cache starts stale so the first sample is complete. The stale mark is cleared before re-reading, so a change reported during a re-read triggers another one. No IP Helper notification reports DNS server changes, so the 30-second maximum age catches those, and it keeps details current if registration fails. Refreshing only when the set of interfaces changes was rejected: it misses address changes on an existing adapter, for example a DHCP renewal with a new lease.
- **Usable addresses:** Windows keeps addresses on an adapter whose media is disconnected, in states that cannot be used, and `GetAdaptersAddresses` lists them all. On the development machine, each of the four disconnected adapters that the Network tab listed by default showed two addresses that `ipconfig /all` hides, because it reports these adapters as "Media disconnected". The Wi-Fi adapter showed the 192.168.1.16 lease from its last connection, in the Ethernet adapter's subnet, so the machine appeared to be on the LAN twice. The Bluetooth PAN and Wi-Fi Direct adapters showed Tentative 169.254 addresses that no other host can reach. Every one also showed a Deprecated link-local address. With the filter, the collector reports no addresses for those adapters, and every address of the Ethernet and Mullvad adapters, which are all Preferred.
- **The state at the boundary, the status on each tick:** a Deprecated address can still be used on a link that is Up: an expired temporary IPv6 address keeps the connections that already use it. Whether to keep one therefore depends on the operational status, which `GetIfTable2` reports on every tick, while the addresses are cached. Applying the rule in `calculateNetworkSamples()` uses the current status, and keeps the rule in a pure function that `tests/unit/` covers. Filtering in the collector, not only on the tab, also corrects the data for any later consumer, such as export.
- **Placeholder DNS servers:** Windows lists `fec0:0:0:ffff::1` to `::3`, site-local addresses from an expired draft for finding DNS servers over IPv6, as the DNS servers of an adapter that has none configured. On the development machine every adapter without a configured server listed them, loopback included, which is Up. An Up adapter with no DNS server, such as a direct link with an automatic private address, would show them too, so hiding them only while the adapter is not Up would not be enough. RFC 3879 deprecated site-local addresses in 2004, so no network configures them. The check compares the address bytes and ignores the scope ID, so it does not depend on how the address is formatted. Such an adapter reads "None", which is true.
- **Temporary by the suffix origin:** with identifier randomization on, as it is by default, Windows still reports a stable address's suffix origin as the link-layer address, and only a temporary address's as random. `netsh interface ipv6 show addresses` shows the two as Public and Temporary. On the development machine both fd44:: addresses on Ethernet matched ipconfig's names.
- **Flags in the domain, the state not:** two flags carry what the address marks need without a Windows type, the link-local test runs on the address bytes rather than on text in the UI, and a later consumer, such as export, gets the same facts. The address state stays in the platform module: with the unusable addresses dropped, only an Up adapter can have a Deprecated address, which is rare.
- **The zone index:** a link-local address cannot be used without it, because two adapters can have the same fe80:: address. ipconfig shows it too.

**History:** Phase 2, network collector; code review F-9 (filter interfaces dropped, virtual adapters marked); code review F-10 (the details cached); code review F-21 (`hasIpInterface`); code review F-23 (`interfaceId`); code review F-36 (unusable addresses and placeholder DNS servers dropped); code review F-44 (temporary and link-local addresses, and zone indexes); maintenance (DC-36, DC-59, DC-64, DC-103 and DC-108 merged in, the Network tab's parts of the last two into [DC-96](#dc-96)).

---

<a id="dc-36"></a>
### DC-36: Merged into DC-35

The adapter details cache is described in [DC-35](#dc-35).

---

<a id="dc-59"></a>
### DC-59: Merged into DC-35

`NetworkSample::hasIpInterface` is described in [DC-35](#dc-35).

---

<a id="dc-64"></a>
### DC-64: Merged into DC-35

`NetworkSample::interfaceId` is described in [DC-35](#dc-35).

---

<a id="dc-103"></a>
### DC-103: Merged into DC-35 and DC-96

The addresses and DNS servers that are dropped are described in [DC-35](#dc-35), and "Not connected" in [DC-96](#dc-96).

---

<a id="dc-108"></a>
### DC-108: Merged into DC-35 and DC-96

The address flags and zone indexes are described in [DC-35](#dc-35), and the address marks and the link speed's tooltip in [DC-96](#dc-96).

---

## Connectivity and network names

Connectivity level, cost and roaming, and the Network List Manager's network names, from the collector to the Network tab.

<a id="dc-39"></a>
### DC-39: Connectivity: a cache fed by `NotifyNetworkConnectivityHintChange`, an explicit Unknown, and a section on the Network tab

**Decision:**

- **Cache:** `ConnectivityCollector` seeds a mutex-protected `ConnectivityStatus` cache with `GetNetworkConnectivityHint` in its constructor, then registers `NotifyNetworkConnectivityHintChange` (with `InitialNotification = TRUE`). The notification callback only stores the value, and `collect()` returns the cached value. The scheduler collects it on the slow-collector thread, and a notification that changed the cached status wakes that thread, so the change reaches the next snapshot ([DC-77](#dc-77)). Registration goes through an injected `ConnectivitySubscriber` that returns an RAII `ConnectivitySubscription`. The production `WindowsConnectivitySubscription`, defined in the `.cpp`, calls `CancelMibChangeNotify2` in its destructor. The subscription is the collector's last member, so it is destroyed before the mutex and cache.
- **Fallback:** if registration fails at run time, `collect()` polls `GetNetworkConnectivityHint` instead.
- **Logging:** `collect()` logs each change of status once at `info`, roaming included.
- **Unknown:** `ConnectivityLevel::Unknown` is the default level, and `isMetered` and `isRoaming` are `std::optional<bool>`. The collector reports `Unknown` and `std::nullopt` before the first successful read and after a failed read, which does not keep the previous value. The mapping ([DC-41](#dc-41)) turns `NetworkConnectivityLevelHintUnknown` and `NetworkConnectivityLevelHintHidden` into `Unknown`, and `NetworkConnectivityCostHintUnknown` into an unknown cost. `toIsRoaming()` maps `NL_NETWORK_CONNECTIVITY_HINT::Roaming`, and returns `std::nullopt` whenever `toIsMetered()` does, which also covers a cost value that a later SDK might add.
- **On the Network tab:** above the adapters, a "Connectivity" section has four readouts from `connectivityReadouts()`: the level ("Internet access" … "No connectivity", or "Unknown"), the cost ("Metered", "Unmetered" or "Unknown"), roaming ("Yes", "No" or "Unknown"), and the connected networks ([DC-100](#dc-100)). The level words come from `connectivityLevelText()`, which the dashboard's `connectivityText()` shares. That one-line summary also appends ", roaming" when the connection is known to be roaming.

**Rationale:**

- **Notifications and a cache:** connectivity changes rarely, so event-driven updates avoid a kernel round-trip on every refresh while keeping `collect()` cheap. Seeding before registration makes the first `collect()` accurate without waiting for the asynchronous initial callback, and ordering it before registration means a newer callback value can never be overwritten by the seed; the initial notification still covers a change between the two. `CancelMibChangeNotify2` waits for in-flight callbacks, so destroying the handle first guarantees the callback never touches a destroyed cache. The Windows subscription class lives in the `.cpp` so no Windows types appear in `connectivity_collector.h`. Injecting the subscriber, like the reader, lets unit tests deliver notifications and verify seeding order, cancellation on destruction, and the polling fallback ([known deviation D-5](known_deviations.md#d-5-notification-callback-wiring-has-no-unit-test)).
- **Off the tick:** the whole collector runs on the slow-collector thread, polling fallback included, so the tick makes no connectivity call ([known deviation D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)).
- **Logging in `collect()`:** logging only on change follows the architecture's `info` level for connectivity state changes without repeating the same state on every refresh. It happens in `collect()` rather than in the callback, because event-driven collector rule 1 allows the callback only to store: a file-sink write is blocking I/O on an OS thread-pool thread ([code review F-14](code_reviews/phase_2.md#f-14-connectivity-callback-writes-to-the-log-on-the-os-thread)). A change that reverts before the next `collect()` is therefore not logged, which is acceptable for a status log.
- **Unknown rather than offline:** the guidelines forbid substituting a default for missing data ([DC-12](#dc-12)). With only `None`, "we could not determine connectivity" was indistinguishable from "offline", and a retained previous value would present stale data as current. `Unknown` is an enum value rather than wrapping the whole status in `std::optional`, so the scheduler, snapshot, and UI keep a single value type and the UI can render the state directly.
- **Roaming unknown with the cost:** in the hint, roaming is a plain flag, so it reads false whenever Windows does not know the cost, for example at startup while the level is still unknown. Windows treats roaming as one of the connection's cost flags (`NLM_CONNECTION_COST_ROAMING` in the Network List Manager), so with an unknown cost, "not roaming" would be a guess.
- **A readout per field:** the roadmap asks for unknown states to be shown as unknown, not as offline or unmetered. Giving each field its own readout, each with its own "Unknown", does that without a sentence that combines three states. The dashboard's one line still mentions only what is known to be true. Until the first slow-collector refresh, every snapshot carries `ConnectivityLevel::Unknown` ([DC-77](#dc-77)), and the section shows "Unknown" for the level, the cost and roaming.

**History:** Phase 2, network collector; Phase 2, known deviation D-5 (an injected subscription); code review F-14 (the callback only stores, and `collect()` logs); Phase 4, process collector (collected on the slow-collector thread, for [DC-77](#dc-77)); Phase 5, network view (roaming and the Network tab's section, and a notified change wakes the slow-collector thread, for [DC-77](#dc-77)); code review F-44 (roaming unknown whenever the cost is); maintenance (DC-40 and DC-99 merged in).

---

<a id="dc-40"></a>
### DC-40: Merged into DC-39

`ConnectivityLevel::Unknown` and the optional cost are described in [DC-39](#dc-39).

---

<a id="dc-41"></a>
### DC-41: Windows-to-domain connectivity mapping in a separate, testable header

**Decision:** The pure conversions `toConnectivityLevel`, `toIsMetered`, and `toConnectivityStatus` live in `platform/windows/connectivity_hint_mapping.h/.cpp`, which exposes Windows SDK types, and are unit tested directly.

**Rationale:** The mapping is the part most likely to be wrong and is impossible to exercise from a host integration test, because the host's connectivity state cannot be forced. Keeping it separate keeps `connectivity_collector.h` free of Windows types. The tests live in the separate `tests/platform/` target (`system_monitor_platform_tests`), which may include Windows SDK headers but makes no API calls, so `tests/unit/` stays free of Windows headers (resolving [known deviation D-4](known_deviations.md#d-4-a-unit-test-includes-windows-sdk-headers)). A separate target was chosen over mirroring SDK constants as plain integers, which would lose the compile-time link to the real SDK values. Note that `netioapi.h` must be reached through `<iphlpapi.h>`, and `CancelMibChangeNotify2` is only declared when `<ws2tcpip.h>` is included first.

**History:** Phase 2, network collector; Phase 2, known deviation D-4 (the tests moved to `tests/platform/`).

---

<a id="dc-99"></a>
### DC-99: Merged into DC-39

Roaming and the Network tab's connectivity section are described in [DC-39](#dc-39).

---

<a id="dc-100"></a>
### DC-100: Network names from `INetworkListManager`, read on the collector's own thread at most once a second, and matched to adapters by LUID

**Decision:**

- **What is read:** `NetworkProfileCollector`, a slow collector, reports the connected networks in `SystemSnapshot::networkProfiles` as `domain::NetworkProfile`s: each network's name, its category (`std::nullopt` if it cannot be read), and the interface IDs of its adapters. Its production reader creates an `INetworkListManager`, lists `GetNetworks(NLM_ENUM_NETWORK_CONNECTED)`, and reads each network's name, category and connections. `ConvertInterfaceGuidToLuid` turns each connection's adapter GUID into the interface LUID, which is the `NetworkSample::interfaceId` the network collector reports ([DC-35](#dc-35)). COM pointers and the name's `BSTR` are held in `std::unique_ptr`s whose deleters release them.
- **A worker of its own:** the collector owns a `std::jthread`, its worker, which makes every Network List Manager query. The worker reads at once when it starts, again when a connectivity notification has made the names stale, and again once they are `kMaximumAge` (a minute) old, but never sooner than `kMinimumReadInterval` (a second) after its last read began. Both are `ReadIntervals`, which tests shorten. The notification is a second `NotifyNetworkConnectivityHintChange` registration, made through `subscribeToConnectivityChanges()`, and its callback only marks the names stale and wakes the worker. A failed read is stored as `std::nullopt`, so it is retried at the next of these points.
- **Results:** the worker stores each result and, if it differs from the previous one, calls the change listener with its mutex held, which wakes the scheduler ([DC-77](#dc-77)). `collect()`, which the scheduler calls on the slow-collector thread, only copies the latest result, and returns `std::nullopt` before the first read has finished. `readCount()` tells tests when a read has finished.
- **The COM apartment:** the production reader is made by a `NetworkProfileReaderFactory` that the worker calls, so the reader's `ScopedComApartment`, a multithreaded apartment, is created on the worker, kept for the worker's lifetime, and destroyed on the worker when the reader is. Within a read, `ScopedComApartment` is declared before the COM objects, so they are released first.
- **Shutdown:** members are declared so that the subscription is cancelled first, the worker is stopped and joined next, and the state they share goes last.
- **On the Network tab:** each card's "Network" readout names the adapter's networks, e.g. "Normandy (Private)", and the connectivity section's "Networks" readout lists every connected network ([DC-39](#dc-39)). "None" means there are none; "N/A" means the names could not be read. The names are shown as plain text, because an access point chooses the name of its Wi-Fi network ([coding guidelines](coding_guidelines.md#text-from-outside-the-program)).

**Rationale:**

- **Matching by LUID:** a network's name is most useful next to the adapter connected to it, and the LUID is the adapter's identity everywhere else ([DC-35](#dc-35)). `INetworkConnection::GetAdapterId` returns the interface GUID. Converting it at the platform boundary keeps GUIDs out of the domain. On the development machine, the two connected networks matched the Ethernet and Mullvad adapters.
- **When to read:** the query is a COM call into the Network List Service, which [DC-06](#dc-06) keeps off the tick. Windows sends a connectivity change when the system joins or leaves a network, and the platform module already had a subscriber for it. Renaming a network or changing its category in Settings changes no connectivity, though, and no C-style notification reports it. The Network List Manager's own events would need a COM event sink. A one-minute maximum age picks those changes up at the cost of one query a minute. Failures are retried at the same points, not on every slow refresh, so a host with the service disabled does not get a COM call every 2 seconds.
- **No seed:** the event-driven rules seed the cache in the constructor, but here that would run COM on the UI thread at startup, where Qt has made the apartment single-threaded. The worker's first read takes the seed's place, and the names are missing until it finishes. [architecture.md](architecture.md#event-driven-collectors-os-callback-threads) describes this stale-mark form, which the adapter details also use.
- **Its own thread:** if the service is slow to answer or stops responding, the query waits, and nothing bounds how long. On the shared slow-collector thread, the process list would stop refreshing and connectivity changes would not be collected. On the collector's own thread, only the names wait. A third scheduler thread for external queries was the alternative, but the scheduler would then have to know which collectors make such calls. The collector knows, and the worker also lets it keep its apartment.
- **Shutdown still waits for a read in progress, and that risk is accepted:** `NetworkListManager` is an in-process COM object (`netprofm.dll`, threading model Both) that calls the service over its own RPC, so `CoCancelCall` cannot interrupt the query, and a thread cannot be stopped safely from outside. A detached thread could still be reading when the collector's state is destroyed. If the service hangs, closing the window therefore leaves the process waiting for the read before it exits.
- **At most once a second:** the review's probe, with a subscription that kept notifying and a 5 ms reader, made 130 reads in 2 seconds. Notifications come in bursts while an adapter reconnects, and each tick copies only the latest result. A second between reads bounds a burst to one read a second, and a change still reaches the UI within about a second, the next tick.
- **The listener after the read:** the worker reads, stores, and only then wakes the scheduler, which copies what it stored. This is the "store, then wake" rule that connectivity follows, and a read that finds the same names wakes nobody.
- **The apartment for the worker's lifetime:** on the shared slow-collector thread, an apartment kept for the thread's lifetime would have outlived the collector, so COM had to be initialized for each read. The worker belongs to the collector, and every COM object is still released within one read. Creating the reader on the worker keeps `CoInitializeEx` and `CoUninitialize` on the same thread, as COM requires. The apartment of the thread that creates the collector does not matter: an integration test creates one on a thread with a single-threaded apartment.
- **Deleters, not WRL:** WRL's `ComPtr` is the usual choice, but `<wrl/client.h>` would be the module's only SDK header with a path, which `.clang-format`'s include categories do not cover. A deleter matches the RAII wrappers the module already uses for MIB tables.
- **Category:** `GetCategory` comes with the same object and decides which firewall profile applies, which explains why one network behaves differently from another. A network whose category cannot be read keeps its name.
- **The cost of a read:** in a Release build on the development machine, the first read in the process took about 11 ms, and later reads 4.4–6.1 ms. That is close to what the review measured with an apartment for each read, so the cost is the query, not COM's start-up. [architecture.md](architecture.md#threading-model) records the figure with the worker.

**History:** Phase 5, network view; code review F-35 (names shown as plain text); code review F-38 (read on the collector's own thread, at most once a second); maintenance (DC-107 merged in).

---

<a id="dc-107"></a>
### DC-107: Merged into DC-100

The network-name collector's own thread is described in [DC-100](#dc-100).

---

## Charts

`SparklineWidget` (`src/ui/charts/`): geometry, series, decorations, colours and painting cost.

<a id="dc-48"></a>
### DC-48: `SparklineWidget`: a caller-owned history, series on one Y range, optional decorations, and cheap dense charts

**Decision:** `SparklineWidget` (`src/ui/charts/`, its own `sysmon_ui_charts` library, linked by `sysmon_ui`) draws every chart in the app, from the dashboard's bare sparklines to the Performance tab's labelled charts.

- **Data:** the caller owns the history and passes a view of it with `setSamples()` each tick. The dashboard keeps a one-minute `domain::RingBuffer<std::optional<float>>` per card, and its network sparkline charts receive plus send. Samples are `std::optional<float>`, and a missing sample breaks the line (a visible gap) instead of plotting zero. The newest sample sits at the right edge, so a partial history grows in from the right. A pure `sparklineSegments()` function maps samples to polyline segments and is unit tested without painting.
- **Y range:** fixed (`kPercentRange`, 0–100%, for CPU, memory and disk) or auto-scaled to the data with 10% headroom (network).
- **Series:** a chart holds one or more series (`setSeriesCount()`, `setSeriesSamples()`), each with its own colour, optional legend label, and fill flag. All series share the capacity, the time axis and the Y range; the auto range covers every series. Series 0 is the *primary* series: `setSamples()` and `samples()` are shortcuts for it, only it is filled by default, it is painted on top, and the minimum and maximum annotations describe it.
- **Grid:** `setSampleIndex()` takes the running index of the newest sample (e.g. the number of samples recorded so far), and the pure function `verticalGridLines()` places vertical grid lines on every `capacity / 10`th sample of that sequence. Each new sample moves every line one slot to the left, so the grid scrolls with the data as in Task Manager. Horizontal lines are fixed at quarters of the Y range.
- **Axis labels:** the Y range's minimum and maximum in a left gutter, and the history window (e.g. "1 min", `formatHistoryWindow()`) and "now" below the plot. `setAxisLabelTemplates()` takes texts as wide as the widest Y-axis labels expected, and the gutter is the widest of these templates and the current range's labels, so a label is never clipped. Network charts get `chartByteRateAxisTemplates()`: the widest output of `formatChartByteRate()` in each unit up to GiB/s, produced by the formatter itself at one byte below each unit boundary (for example "1024.0 MiB/s"). Percentage charts need no templates, because their fixed range already gives constant labels.
- **Annotations:** a line above the plot with the current value and the primary series' minimum and maximum, and dot markers at those points on its line; on ties, the newest occurrence is marked. With two or more series and at least one label, the current-value area becomes a legend: a short line in each series colour, then its label and current value in the text colour. When a chart has a legend and its primary series has a label, the extremes name it: "Receive min 1.0 KiB/s  max 4.0 KiB/s". Otherwise they read "Min …  Max …". `extremesText()` exposes the text for tests.
- **Formatters:** values are formatted by caller-supplied `ValueFormatter`s, so the charts library stays independent of `ui`'s formatting helpers. `setAnnotationFormatter()` formats the annotations separately from the axis labels, and defaults to the value formatter. `PerformancePage::createHistoryChart()` takes both: percentage charts label the axis in whole percent ("8%", `formatChartPercent()`) and annotate with one decimal ("7.5%", `formatPercent()`), as the readouts do. A missing current value shows "N/A". `plotArea()` exposes the rectangle left for the series, and pure `summarizeSeries()` carries the annotations' logic for unit tests.
- **Off by default:** the grid, the axis labels and the annotations are each off by default.
- **Colours:** unless the caller overrides a series colour, series 0–4 take fixed hues in this order: blue, orange, aqua, violet, magenta (`defaultSeriesColor()`), with separate light and dark steps. The widget picks the dark steps when its background colour is dark, at paint time, so it follows theme changes. Series 5 and later share a muted grey instead of generated or repeated hues. `setSeriesColorSlot()` gives a series the default colour of another palette slot, still following the light or dark surface. `DiskPerformancePage` gives each volume the lowest free slot when it first appears and keeps it while the volume has history; its series stay in volume-name order. Text is drawn in the palette's text colour, dimmed for secondary labels, never in a series colour. The grid, frame, and secondary text use the text colour at reduced opacity.
- **Dense charts:** a chart is *dense* when it has more slots (its capacity) than its plot has pixel columns, for example a 30-minute history on an 800-pixel chart or on a small per-core chart. For a dense chart, `sparklineSegments()` keeps only each pixel column's lowest and highest point, in time order, so a segment has at most two points per column, and gaps still split segments. The line is drawn with `drawPolyline()` and a 1-pixel cosmetic pen instead of the 1.5-pixel pen with round joins. The fill follows `upperEnvelope()`, one point per column at the column's highest value, instead of the zigzag line. Charts that are not dense are drawn without these steps.

**Rationale:**

- **Built early, extended:** the roadmap's "each card shows a mini sparkline" belonged to Phase 2, so a minimal single-series widget came first, and Phase 3 extended it (grid, multiple series, axis labels, annotations) rather than replacing it. Rendering from a caller-supplied view keeps the widget reusable for the Performance tab's longer histories, and copying 60 samples per chart per tick is negligible. Gaps follow the project rule against representing missing data as zero ([DC-12](#dc-12)). With its decorations off, the same widget stays a bare 36-pixel dashboard sparkline.
- **A caller-supplied index for the grid:** once the history is full, the samples span passed to the widget keeps the same length, so the widget cannot tell from the samples alone that time has moved on. Counting `setSamples()` calls inside the widget would break as soon as a series is set twice per tick or the widget is refreshed for other reasons, so the caller passes the index explicitly. Deriving the spacing from the capacity keeps about ten columns whether the chart shows 60, 300, or 1800 samples. Lines are computed with modular arithmetic on the running index, so a short history (index smaller than the capacity) does not underflow. `RingBuffer` was left unchanged; a caller that needs the index keeps its own counter.
- **One Y range:** the roadmap asks for several series on one chart (per-core CPU, and network receive and send). One Y scale keeps the chart honest: series of different units or scales belong on separate charts, not a second axis. Filling only the primary series avoids stacked translucent fills turning into a muddy block. Minimum and maximum markers for every series would clutter a per-core chart, so they follow the one series the chart is about. With a legend nothing said which series that was, so the extremes name it, which keeps them on the multi-series charts that need them, such as the network charts' Receive series.
- **Text outside the plot:** text drawn over the plot would collide with the lines, so labels get their own bands and the plot shrinks instead. Text stays in the palette's text colour, so it is readable on any theme, and a series is identified by its colour key plus its label, never by colour alone. Marking the newest of tied extremes keeps the marker close to the live edge on flat data.
- **Two formatters:** the annotation and the readout below it showed the same value at different precision ("8%" and "7.5%"), which looks like a disagreement. Whole percentages still suit the axis, where they are scale marks rather than readings, so the two get separate formatters instead of one shared precision. Byte rates already have one decimal and use the same formatter for both.
- **Templates for the gutter:** sized from the current range's labels alone, the gutter of an auto-ranged chart changed with traffic. The review measured the plot's left edge moving by up to 19 pixels as a burst came and went, which reads as jitter on a chart that otherwise scrolls smoothly, and stacked network charts did not line up. Several templates are needed rather than one, because unit letters differ in width ("KiB/s", "MiB/s", "GiB/s"). Producing them from the formatter keeps them correct if the formatting changes. Measuring a few strings costs little next to painting the series. A formatter that reports its own widest output was rejected because `ValueFormatter` is a plain function, and the charts library would need a richer interface for a single caller. Units above GiB/s are left out: they would widen every network chart for rates above 1024 GiB/s, which no current adapter reaches, and such a label would still widen its gutter instead of being clipped.
- **The palette:** the hues and steps come from a categorical palette whose order was checked with a colour-vision-deficiency validator: every adjacent pair stays separable under deuteranopia, protanopia, and tritanopia, and has enough contrast against the chart background, in both light and dark mode. Red, amber, and green were removed from that palette because they are the dashboard's Critical, Warning, and Healthy status colours, and a series in those hues would read as a status. The remaining five still pass after re-validation. In light mode, aqua and magenta fall below 3:1 contrast against the background, which is why labelled series always appear in the legend with their values. Repeating or generating hues past the fifth series would make series look alike, so more series (e.g. per-core CPU on a 16-core machine) get a shared de-emphasised grey. The caller can colour them explicitly, highlight a total in the primary colour, or split them across charts, as the Performance tab does for cores ([DC-53](#dc-53)). The palette is used rather than the system accent colour, because on systems with a red accent the accent would read as Critical.
- **Colour slots:** volume colours used to follow sort position, so a new volume changed the colour of every volume after it. A fixed `setSeriesColor()` override would lose the light and dark steps, so the widget takes a palette slot instead. Reusing the lowest free slot keeps colours within the five hues while there are at most five volumes.
- **Dense charts:** with 30 minutes of history, a Release build took about 210 ms per tick on the CPU page (16 cores) and 145 ms on the network page. That was measured with synthetic worst-case data that swings across most of the range every 20 samples, and it is far too long for a 1 Hz UI thread. Decimation alone barely helped, because the cost was not the number of points. It was Qt's general stroker, which builds a round-joined outline for every vertex of a pen wider than one pixel, and the scanline cost of filling a polygon whose top edge zigzags. Qt's raster engine has a fast path for cosmetic pens up to one pixel wide, which cut the worst page to about 60 ms. The envelope fill, which covers the same pixels as filling under the zigzag because the line is drawn over each column's span, brought it to about 27 ms (see the measurements in [DC-61](#dc-61)). Min/max decimation is kept because it bounds the work by the chart's width, and unlike averaging it keeps one-sample spikes visible. A one-pixel line also suits a dense chart better, because a thicker one would merge neighbouring columns.

**History:** Phase 2, dashboard view; Phase 3, custom sparkline widget (the grid, several series, axis labels and annotations, and the series palette); Phase 3, Performance tab (dense charts); code review F-24 (the gutter sized from templates); code review F-26 (a separate annotation formatter, named extremes, and colour slots); maintenance (DC-49, DC-50, DC-51, DC-52, DC-60, DC-65 and DC-67 merged in, DC-67's link speeds into [DC-57](#dc-57)).

---

<a id="dc-49"></a>
### DC-49: Merged into DC-48

The scrolling grid is described in [DC-48](#dc-48).

---

<a id="dc-50"></a>
### DC-50: Merged into DC-48

Multiple series on one chart are described in [DC-48](#dc-48).

---

<a id="dc-51"></a>
### DC-51: Merged into DC-48

The axis labels and annotations are described in [DC-48](#dc-48).

---

<a id="dc-52"></a>
### DC-52: Merged into DC-48

The series colours are described in [DC-48](#dc-48).

---

<a id="dc-60"></a>
### DC-60: Merged into DC-48

Dense charts are described in [DC-48](#dc-48).

---

<a id="dc-65"></a>
### DC-65: Merged into DC-48

The axis label templates are described in [DC-48](#dc-48).

---

<a id="dc-67"></a>
### DC-67: Merged into DC-48 and DC-57

The annotation formatter, the named extremes and the colour slots are described in [DC-48](#dc-48), and the rounding of link speeds in [DC-57](#dc-57).

---

## Main window and shared UI

How snapshots reach the tabs. The rules for the text that the tabs show, its format and its colours, are in the [coding guidelines](coding_guidelines.md#text-from-outside-the-program).

<a id="dc-54"></a>
### DC-54: Snapshot forwarding: every view records every snapshot, and hidden views skip repainting (code review F-4)

**Decision:** `MainWindow::onSnapshotReady` forwards every snapshot to the dashboard and the performance view, whichever tab is current. Recording and display are separate steps. `PerformanceView::updateSnapshot` always appends to each page's history. It refreshes widgets only while the view is visible, and then only for the selected page and the sidebar. `showEvent` and a page change refresh from the recorded history. The Processes and Network views will follow the same pattern when they are implemented.

**Rationale:** Code review F-4 proposed updating only the visible tab. For views with history, that would leave gaps covering the time another tab was open, so the recording step must always run. Recording is only a few `RingBuffer` pushes per tick. The costs worth skipping are copying up to thousands of samples per chart into the widgets and painting them, and those are skipped while the view is hidden. Deciding visibility inside each view, rather than in `MainWindow`, keeps the rule next to the state it protects and also covers pages that are not selected within the view.

**History:** Code review F-4, deferred to Phase 3, Performance tab.

---

<a id="dc-68"></a>
### DC-68: Retired, the headers say what they hold

A UI constant or helper that several files use is declared once, in a header they already include, with a comment that says what it is for; the adapter order is in [DC-96](#dc-96).

---

<a id="dc-102"></a>
### DC-102: Retired, the rule is in the coding guidelines

The rule and its reasons are in [coding_guidelines.md — Text from outside the program](coding_guidelines.md#text-from-outside-the-program).

---

<a id="dc-105"></a>
### DC-105: Retired, the rule is in the coding guidelines

The rule and its reasons are in [coding_guidelines.md — Text colours](coding_guidelines.md#text-colours), and each tone's shades in `text_tone.cpp`.

---

## Dashboard tab

The resource cards and the health evaluation they show.

<a id="dc-45"></a>
### DC-45: Health evaluation: domain types, a pure evaluator in `monitoring`, and the result carried in the snapshot

**Decision:** `domain/health_status.h` defines `HealthLevel` (Unknown, Healthy, Warning, Critical) and `SystemHealth` (cpu, memory, disk, network, overall). `monitoring/health_evaluator.h` provides the pure function `evaluateHealth(snapshot, HealthThresholds)`, and `SamplingScheduler` calls it at the end of each `sampleOnce()` to fill `SystemSnapshot::health`. Thresholds are strictly "above": warning above 90% and critical above 95% for memory and disk. CPU is critical only above 98%, because short full-load bursts are normal. Disk health is the worst fixed volume. Network health comes from connectivity: InternetAccess is Healthy, any lesser level is Warning. Overall is Critical if any component is Critical, else Warning if any is Warning, else Unknown if any is Unknown, else Healthy. Thresholds are set on the scheduler (`setHealthThresholds`, stopped-only like collector registration) so the Phase 6 settings file can supply them later.

**Rationale:** The architecture places the "health/connectivity evaluator" in `monitoring`, and `domain/AGENTS.md` lists health states as domain types. The domain target is header-only, so it holds only the types. The UI may not depend on `monitoring`, so it cannot call the evaluator. Carrying the result in the snapshot keeps the UI a pure renderer and makes health available to future consumers (tray icon, alerts) without duplicating logic. Unknown is a first-class level so missing data is never reported as Healthy, but a known problem still surfaces when other components lack data. Health is evaluated from each instantaneous sample with no smoothing or hysteresis, so CPU can flicker between levels under bursty load. Sustained-threshold rules belong with the planned alert rules (see the roadmap's alert-rules section).

**History:** Phase 2, dashboard view.

---

<a id="dc-46"></a>
### DC-46: Dashboard cards: `ResourceCard` widgets fed by pure formatting helpers

**Decision:** The dashboard is an overall "System status" line plus five `ResourceCard` widgets (CPU, memory, disk, network, uptime) in a three-column grid with equal column stretch. Each card has a title, a large value, a detail line, a health status label (hidden for uptime, which has no health dimension), and at the bottom a fixed-height slot that holds the card's one-minute sparkline ([DC-48](#dc-48)); the uptime card's slot stays empty. The status label is a `TonedLabel` ([coding guidelines](coding_guidelines.md#text-colours)): Healthy, Warning and Critical in bold in their tones' colours, and Unknown dimmed and in italics, because it is not a state. Each tone has a shade for light and one for dark backgrounds, so the status reaches 4.5:1 contrast in both colour schemes and follows a change of scheme. All text comes from pure functions in `ui/dashboard_formatting.h`, which are unit tested without widgets. Before the first snapshot a card shows "--". When a snapshot lacks the data it shows "N/A", never a zero. Specific rules:
- Byte quantities use binary (IEC) units (KiB, MiB, GiB).
- Disk shows the fullest volume.
- Network shows total throughput over hardware adapters that are Up. Virtual adapters (VPN tunnels, virtual switches) are left out because their traffic also crosses a hardware adapter. It shows "N/A" if any active hardware adapter has no rate yet, because a partial sum would understate traffic, and "No active adapter" when no hardware adapter is Up. Connectivity is the detail line.
- String literals are ASCII only (no arrows or middle dots). Targets that link Qt compile with `/utf-8`, which Qt adds to its usage requirements, but `domain` and `platform/windows` do not link Qt, so a non-ASCII literal would be read in the system code page there. Non-ASCII text in code is written as escapes or code units.

**Rationale:** A reusable card keeps the five resources visually consistent. The chart slot has the same height in every card, so the uptime card, which has no chart, lines up with the others. Keeping formatting and aggregation in pure functions keeps `DashboardView` thin and makes edge cases (unit boundaries, missing rates, no volumes) testable. Health comes precomputed in `SystemSnapshot::health`, so the UI only renders it, in line with `src/ui/AGENTS.md`. The status colours are tones rather than fixed colours because no single colour reaches WCAG's 4.5:1 on both a light and a dark background. Binary units match how Windows reports memory and disk sizes.

**History:** Phase 2, dashboard view; code review F-9 (only hardware adapters in the network total); code review F-19 (why literals are ASCII); code review F-37 (status colours that reach 4.5:1 in both colour schemes).

---

## Performance tab

The resource pages, their histories and their readouts.

<a id="dc-53"></a>
### DC-53: Performance view: a sidebar of resource pages that each own their history

**Decision:** `PerformanceView` is a sidebar (`QListWidget`) beside a `QStackedWidget` of four pages: CPU, memory, disk, and network. Each sidebar entry shows the page title and a short summary of the latest values. Each page derives from `PerformancePage` and owns the `RingBuffer` histories for its charts:
- **CPU:** a total utilization chart above a near-square grid of small per-core charts, one per logical processor, which scrolls vertically when the page is too small for it. When the number of cores in the samples changes, the grid is rebuilt and the per-core history restarts, because the core indices no longer line up.
- **Memory:** physical memory usage and commit charge as percentage charts. Commit charge is a percentage of the commit limit, and the limit can grow with the page file.
- **Disk:** one space-used chart with a labelled series per volume, in volume-name order.
- **Network:** one chart per adapter that is Up and can carry traffic: a hardware adapter, or a virtual adapter bound to IP (see [DC-35](#dc-35)). Each chart has receive and send series on an auto-scaled byte-rate axis. Hardware adapters come first and are shown in a scroll area.

Disk volumes are keyed by volume name, and network adapters by `NetworkSample::interfaceId` (see [DC-35](#dc-35)). A key that leaves the snapshots records gaps, and is dropped once its history holds no samples. An adapter still in the snapshot is kept even when it has no values, since a new adapter has no rate until its second sample. Network history is recorded for every adapter, including adapters that are down, so an adapter that comes up shows its history.

**Rationale:** Task Manager's layout (a resource list with one detail page at a time) gives each chart room. It also lets per-core charts use their own small multiples instead of 16 or more overlapping series, which would run out of distinguishable colours after five (see [DC-48](#dc-48)). The histories stay in the UI, as the dashboard's do, because the UI may not depend on `monitoring`. Keeping each history next to the page that draws it keeps `SystemSnapshot` a per-tick value. Keying by name or ID rather than position keeps a volume's or adapter's line continuous when others appear or disappear.

**History:** Phase 3, Performance tab; code review F-20 (the per-core grid scrolls, for [DC-58](#dc-58)); code review F-21 (only adapters that can carry traffic, for [DC-35](#dc-35)); code review F-23 (adapters keyed by interface ID, for [DC-35](#dc-35)).

---

<a id="dc-57"></a>
### DC-57: Performance readouts: pure builders with stable names, shown in a `ReadoutGrid`

**Decision:** Each page shows numeric readouts next to its charts, in a `ReadoutGrid` with a dimmed name above each value:
- **CPU:** utilization, base speed, logical processors, processes, threads, handles, and up time.
- **Memory:** used, available, total, committed, system cache, paged pool, and non-paged pool (see [DC-63](#dc-63)).
- **Disk:** used, total, and free space, one readout per volume.
- **Network:** under each adapter's chart, receive, send, link speed, type (hardware or virtual), adapter description, IPv4 and IPv6 addresses, DNS servers, and bytes received and sent.

The text comes from pure functions in `ui/performance_readouts.h` (`cpuReadouts()`, `memoryReadouts()`, `diskReadouts()`, `adapterReadouts()`). They return the same names in the same order whether or not the data is available, and "N/A" for missing values, including a link speed of 0. Readouts show "--" until the first snapshot, as the dashboard does. `ReadoutGrid::setReadouts()` updates the value labels in place while the names are unchanged, and rebuilds its cells only when they change (for example, a volume is added). Counts use comma thousands separators, clock speeds are in GHz, and link speeds are in decimal bits per second. `formatLinkSpeed()` rounds a link speed to three significant digits before it chooses the unit, up to Tbps, so 1000 Mbps reads "1 Gbps".

**Rationale:** Keeping the names fixed means a missing value does not change the layout, and the missing value is visible rather than quietly removed. Pure builders keep formatting and edge cases testable without widgets, as `dashboard_formatting.h` does for the dashboard. Updating labels in place avoids rebuilding widgets every second. Counts are grouped with a fixed separator rather than with `QLocale`, so tests do not depend on the machine's locale. Localized number formatting can come with the Phase 6 display-unit settings. Link speeds use decimal units because network rates are conventionally quoted that way, while byte quantities keep the binary units used elsewhere. `QString::number(…, 'g', 3)` switches to an exponent once rounding reaches 1000, which showed "1e+03 Mbps", and choosing the unit after rounding avoids that at every boundary.

**History:** Phase 3, Performance tab; code review F-22 ("Used" and "System cache", for [DC-63](#dc-63)); code review F-26 (link speeds rounded before the unit is chosen); maintenance (that rule moved here from DC-67).

---

<a id="dc-58"></a>
### DC-58: Performance pages fit an 800×600 window: re-flowing readouts and a scrolling per-core grid (code review F-20)

**Decision:**
- **Readouts:** `ReadoutGrid` lays out its labels with its own `QLayout` (`ReadoutLayout`, in `readout_grid.cpp`). The constructor's column count is a maximum: the layout uses as many columns as fit the width, reports a height for width, and has the widest readout as its minimum width. Spare width is shared so that columns are equally wide where their content allows. Each cell remembers the widest it has been since the names last changed, so a value that shrinks again does not bring a column back.
- **Scrolling:** the network page's scroll area, and a new scroll area around the CPU page's per-core grid, scroll vertically only. Per-core charts have a 12×12 minimum, so the 16 columns of a 256-core grid fit the page width.
- **Window size:** `MainWindow` opens at 1024×720, limited to 90% of the screen's available area. 800×600 is still the size the content must fit, and widget tests check it with 256 cores and two adapters that have long addresses and descriptions.

**Rationale:** A `QLabel` without word wrap cannot be narrower than its text. A fixed five-column grid of IPv6 addresses and adapter descriptions was 875 px wide on a 535 px page, so every adapter's readouts needed sideways scrolling. Doing the re-flow in a layout, rather than in `resizeEvent()` as the review suggested, means Qt's height-for-width handling sizes the scroll area's content and the pages' box layouts correctly. The labels are only moved, never recreated, so values are still updated in place. Wrapping or eliding values was rejected, because addresses must stay whole to be read and copied. Giving long values their own full-width rows was also rejected: it would be a special case inside a generic grid, while re-flow already gives long values fewer columns and short readouts more when there is room. Without the remembered widths, a transfer rate near a column boundary would change the column count every second.

The per-core grid scrolls instead of shrinking its charts further, because a chart only a few pixels tall shows nothing. Its minimum width still matters: a scroll area with horizontal scrolling turned off squeezes content that is too wide rather than widening the window, and it shows no scroll range, so the tests compare the content's minimum width with the viewport. At the larger default size, the network readouts usually get two or more columns instead of one. Limiting it to the screen keeps the window on screen on a 1366×768 laptop.

**History:** Code review F-20.

---

<a id="dc-61"></a>
### DC-61: 30 minutes of performance history and a chart-window selector

**Decision:** Each Performance page keeps 30 minutes of history (`PerformancePage::kHistoryCapacity` = 1800 samples at 1 Hz). A "Chart history" selector below the sidebar sets how much of it the charts show: 1 minute (the default, as in Task Manager), 5 minutes, or 30 minutes. `showHistory()` passes the chart only the newest window of samples and sets the chart's capacity to match. The selection is not saved yet, because persisting it belongs with the Phase 6 settings file. Windows are counted in samples, which equals seconds at the fixed 1 Hz sampling rate. A configurable refresh interval will need to convert them.

**Rationale:** The roadmap asks for smooth 1 Hz updates with 5 to 30 minutes of history. Measured with `build.ps1 -Benchmark` ([DC-66](#dc-66)), a page costs under 20 ms per second at the default window, 2% of the UI thread, and even the synthetic worst case at 30 minutes averages under 50 ms, one short frame per second ([performance.md](performance.md)). Real data rarely swings that far on every sample. Reaching this needed the dense-chart painting in [DC-48](#dc-48). Before it, the 30-minute CPU page took about 210 ms per tick, and the Debug build was about three times slower still. The history always holds 30 minutes, so switching windows shows data at once rather than starting empty. It costs about 29 KB per series (the `RingBuffer` stores each sample twice), about 2 MB for a 64-core CPU page. Passing only the visible window keeps an auto-scaled axis, such as network throughput, fitted to what is on screen rather than to a spike that has scrolled out of view. A known side effect is that a volume or adapter that disappears stays in the chart's legend, with gaps, until its 30-minute history is empty.

**History:** Phase 3, Performance tab; code review F-25 (the figures replaced by the committed benchmark's, for [DC-66](#dc-66)); maintenance (the figures moved to performance.md).

---

<a id="dc-63"></a>
### DC-63: Memory readouts name what `GetPerformanceInfo` reports, not Task Manager's figures (code review F-22)

**Decision:** The memory page's "Cached" readout is renamed "System cache", and `MemorySample::cachedBytes` is renamed `systemCacheBytes`. It still shows `PERFORMANCE_INFORMATION::SystemCache`, and the domain comment now says what that is without claiming it matches Task Manager. "In use" is renamed "Used". It is still total minus available, so it includes the modified page list. The dashboard ("… of … used") and the sidebar summary already used that wording or no label, and are unchanged.

**Rationale:** Task Manager's "Cached" is the standby list plus the modified list, and its "In use" leaves the modified list out. `SystemCache` is neither. Microsoft documents it as the standby list plus the system working set, but it does not measure as that either. Two readings on the development machine, taken from the memory-list performance counters for comparison only:

| Reading | `SystemCache` | Standby | Standby + modified (Task Manager's "Cached") | Standby + system working set |
| --- | --- | --- | --- | --- |
| Code review F-22 | 30.59 GiB | 33.83 GiB | 34.37 GiB | 35.09 GiB |
| This fix | 34.84 GiB | 34.25 GiB | 34.42 GiB | 35.31 GiB |

The difference from Task Manager was 3.8 GiB in the first reading and 0.4 GiB in the second, and in different directions, so no correction can be applied to it. Reproducing Task Manager's figures needs the exact list sizes. `NtQuerySystemInformation(SystemMemoryListInformation)` provides them but requires `SeProfileSingleProcessPrivilege`, which unelevated users do not have. The `\Memory\Standby Cache*` and `\Memory\Modified Page List Bytes` counters would need PDH, which is not used for core metrics ([DC-02](#dc-02)). The readout is kept rather than dropped because the size of the system cache is still useful, and the new name no longer promises a figure the tool cannot provide. "Used" is the plain description of total minus available. If elevated access is added (see "Elevated process termination" in the post-MVP roadmap), an elevated component could read `SystemMemoryListInformation` and provide Task Manager's breakdown.

**History:** Code review F-22.

---

## Processes tab

The process table: its model, columns, sorting, tree, highlights and access indicator.

<a id="dc-79"></a>
### DC-79: Process table model: one row per `ProcessIdentity`, its columns, and updates that signal only the cells that changed

**Decision:**

- **Rows:** the Processes tab shows a `QTreeView` over `ProcessModel`, a `QAbstractItemModel` subclass. Each row is a node owned by a map keyed by `ProcessIdentity`. An index's internal pointer is its node, and each node keeps its row number, so `indexOf()` needs no search. `updateProcesses()` gives the kept rows their latest values, inserts the new processes, and removes the row of a process that left the list once its exited highlight has ended ([DC-82](#dc-82)). It never resets the model. A duplicate identity in a list adds one row.
- **Every snapshot:** `MainWindow` forwards every snapshot to `ProcessesView`, which applies the list whether or not the tab is shown. A snapshot without a list keeps the last rows, and the summary line above the table (`processListSummary()`) then says that it shows the last list read. The Idle process (PID 0) has no image name in the list ([DC-71](#dc-71)), and the table names it "System Idle Process".
- **Columns:** Name, PID, CPU, Memory, I/O, Network, Threads, and Handles.
  - **CPU** is `cpuUsagePercent` with one decimal ([DC-76](#dc-76)).
  - **Memory** is `privateWorkingSetBytes`, the part of the working set that no other process shares, and the column sorts by it. The parser reads it from `WorkingSetPrivateSize` ([DC-71](#dc-71)), and the comments on the three memory fields say what each one counts. The header is "Memory", and its tooltip (`headerToolTip()` in `process_model.cpp`) says that the column shows the private working set, as Task Manager's Memory column does. `processMemoryToolTip()` gives each cell's tooltip: the private working set, the working set and the commit size (`privateBytes`), each with what it counts.
  - **I/O** is read plus write bytes per second, from `ioReadBytesPerSec` and `ioWriteBytesPerSec`, which `ProcessCollector` computes with the CPU usage ([DC-76](#dc-76)). The cell shows the sum, or "N/A" when either rate is missing, and its tooltip shows each rate. The column is headed "I/O" rather than the roadmap's "Disk I/O", and its header tooltip says it counts files, pipes, and devices, not only disk.
  - **Network** shows "N/A" until per-process network usage is collected with ETW, and its header tooltip says so.
  - **Threads** and **Handles** are counts with thousands separators, from `formatCount()`, as in the readouts ([DC-57](#dc-57)).
  - A missing value shows "N/A" in the dimmed text colour ([coding guidelines](coding_guidelines.md#text-colours)). A measured zero shows "0.0%" or "0 B/s" in the normal colour.
  - Numbers and their headers are right-aligned. Every header has a tooltip. Each value column is sized once to fit a template of its widest expected text, such as "1023.9 MiB/s", and the Name column takes the remaining width.
- **Changed cells only:** `updateProcesses()` compares each kept process's raw values with its previous ones, column by column, and signals only what changed: one `dataChanged()` per parent, covering the smallest block of rows and columns that changed under it. A list that repeats the previous one signals nothing. A change in any of the three memory figures marks the Memory cell as changed, because its tooltip shows all of them. Name and PID never change for an identity.
- **Named roles:** the value signals name the display, tooltip, foreground, and decoration roles. The proxy's filter role is `Qt::UserRole`, which no signal names, because the filter matches only names and PIDs ([DC-81](#dc-81)), and those never change for a row.
- **Batches:** `ProcessModel` emits `valueChangesStarted(firstColumn, lastColumn, signalCount)` before a refresh's value signals and `valueChangesEnded()` after them, so that the proxy can sort once per refresh ([DC-81](#dc-81)).
- **Measured:** `build.ps1 -Benchmark` times the tab with up to 2,000 processes ([DC-66](#dc-66)), and [performance.md](performance.md) records the figures.

**Rationale:**

- **`QAbstractItemModel`, rows by identity, changed in place:** the model derives from `QAbstractItemModel` rather than `QAbstractTableModel` because the process tree needs parent and child rows in the same model, and a `QTreeView` shows a flat model as a table. Keying rows by identity rather than PID means a process that reuses a PID gets a new row, rather than the old process's row with new values. Changing rows in place keeps the selection, the scroll position, and every `QPersistentModelIndex` a view or proxy holds. A reset on every refresh would lose them, which [ui/AGENTS.md](../src/ui/AGENTS.md) forbids. The tab applies every list even while hidden, as every view records every snapshot ([DC-54](#dc-54)): the model is the tab's record of the process list, and a hidden `QTreeView` does not repaint. A failed read keeps the last rows rather than emptying the table, because treating it as an empty list would remove every row and add them all back on the next successful read. The summary line says that the rows are stale.
- **Rates from the collector:** the process list reaches the UI with every 1 Hz snapshot but is refreshed only every 2 seconds ([DC-77](#dc-77)), so consecutive snapshots often carry the same counters. The snapshot's timestamp is also the tick's, not the time the list was read. A rate computed in the UI would therefore alternate between zero and double the real value. The collector knows when each list was read, so it computes the I/O rates there, from the same baselines and timestamp as the CPU usage. Keeping read and write separate in the domain leaves both available to other consumers, while the table shows their sum, as the roadmap asks. Summing only when both rates are known avoids understating the total, the rule `sumActiveThroughput()` follows for adapters. The counters are `ReadTransferCount` and `WriteTransferCount`, which include pipe and device I/O ([DC-71](#dc-71)), so "Disk I/O" would mislabel them.
- **The private working set:** a working set counts every page a process has in physical memory, including shared ones: DLL code, and memory that a browser's processes share. Each shared page is counted again for every process that maps it. In the code review, the working sets added up to 33.2 GiB on a machine with 30.5 GiB in use, and browser processes showed up to 1.8 times their private working set. Users who add up a browser's processes, or compare one with Task Manager, got a figure that was too high, and nothing explained why. No page is in two processes' private working sets, and the same 399 processes' private working sets added up to 17.2 GiB. Each value can therefore be read as the process's own share. It is also what Task Manager's Memory column shows, so the two tools agree, and sorting by Memory finds the process that holds the most memory on its own. The roadmap asked for "Memory (working set)", which the column first showed without a comparison with Task Manager.
- **Not the commit size:** the commit size was the other candidate. It also counts private memory that has been paged out or never touched, so it does not measure physical memory, and Task Manager shows it only as an optional column on its Details tab. The working set stays in the tooltip because shared memory still occupies physical memory, and a process that maps large files or many DLLs shows it only there. A tooltip, rather than two more columns, keeps the roadmap's columns and leaves the width to the Name column. Marking the cell for a change that only the tooltip shows costs little, because the three figures usually change together.
- **N/A, dimmed, and fixed widths:** the Network column stays so the layout does not change when ETW arrives. Its cells say "N/A" rather than zero because no value is known, and they are dimmed so a column of placeholders does not draw the eye. Fixed widths from templates keep the columns still as values change, and avoid `QHeaderView::ResizeToContents`, which measures every row whenever the data changes.
- **Changed cells and named roles:** when every refresh signalled every kept row, one block per parent, naming no roles, a repeat of the same list still cost up to 6.8 ms. In a tree with recursive filtering, Qt re-filters every ancestor of a block whose signal names no roles or names the filter role, and treats each ancestor as a changed range of its own. Naming the roles and moving the filter role away from them removes that walk, and comparing values makes a repeat signal nothing at all. Raw values are compared rather than display text because a change too small to show can still reorder a sorted view. A block per parent, rather than a signal per changed row, keeps the number of signals to one per parent, and every signal that covers the sorted column can cost a re-sort. Re-checking the unchanged rows inside a block is cheaper.

**History:** Phase 4, process explorer view; Phase 4, process explorer view (exited rows kept for three seconds, for [DC-82](#dc-82)); code review F-29 (Memory shows the private working set); code review F-33 (re-measured, for [DC-95](#dc-95)); code review F-37 (N/A in the dimmed text colour); maintenance (DC-80, DC-83 and DC-91 merged in, DC-83's single sort per refresh into [DC-81](#dc-81)); maintenance (the benchmark moved to [DC-66](#dc-66), and its figures to performance.md).

---

<a id="dc-80"></a>
### DC-80: Merged into DC-79

The process table's columns are described in [DC-79](#dc-79).

---

<a id="dc-81"></a>
### DC-81: Sorting and filtering: a proxy that compares domain values, with missing values lowest, and sorts once per refresh

**Decision:** `ProcessSortFilterProxyModel`, a `QSortFilterProxyModel` subclass, sits between `ProcessModel` and the view.

- `lessThan()` compares the `ProcessInfo` values behind the two indexes, read with `ProcessModel::process()`, rather than display text or `QVariant`s. Through `std::optional`'s ordering, a missing value (a CPU or I/O rate not measured yet) sorts below every value.
- Rows that compare equal fall back to the name, then the PID, then the creation time. Names are compared on their UTF-8 bytes with ASCII case folded, and the Idle process sorts as "System Idle Process".
- A filter box ("Filter by name or PID") passes its text to `setFilterFixedString()`, and the proxy ignores case. `filterAcceptsRow()` keeps a row whose display name or decimal PID contains the text.
- Headers give `Qt::InitialSortOrderRole`, so the first click on Name, PID or Network sorts ascending and the first click on a measured value sorts largest first.
- The table starts sorted by name. Dynamic sorting stays on, so rows move as their values change. The summary line keeps counting the whole list, not the rows the filter shows.
- **One sort per refresh:** when the value signals of a refresh, which `ProcessModel` announces as a batch ([DC-79](#dc-79)), cover the sorted column under more than one parent, the proxy switches dynamic sorting off for the batch and back on afterwards, which sorts every level once. Removals and insertions keep dynamic sorting, so a new row is still placed by binary search.

**Rationale:** Display text sorts wrongly: "9.0%" comes after "10.0%", and "900 KiB" after "1.2 MiB". A sort role of numeric `QVariant`s would fix that, but Qt's default comparison never orders an invalid `QVariant` before a valid one, so "N/A" rows would come first in a largest-first sort, the common case. Comparing the domain values is type-checked, puts missing values last in that sort, and creates no `QVariant`s. Most processes show "0.0%" CPU at any moment. Without a tie-breaker their relative order depends on the order the proxy happened to insert and re-sort them, so equal rows could swap on a refresh. Breaking ties by name and PID keeps them in an order a reader can scan. Dynamic sorting re-sorts whenever the sorted column changes, and ties fall back to names whatever the column. Folding ASCII case on the raw bytes compares names without allocating, unlike building a `QString` for each comparison, and image names are almost always ASCII. Non-ASCII bytes still compare in a consistent order. A literal filter means the dots in "a.exe" are not wildcards. Matching the PID as a substring lets the same box find a process by ID. `Qt::InitialSortOrderRole` is how `QHeaderView` chooses a new column's first sort order, so the view needs no header handling of its own. Network has no values yet, so every row compares equal and the names decide; a descending sort reverses the whole comparison, names included, so a largest-first click would list them Z to A. Starting sorted by name gives a steady first view; sorting by CPU is one click away.

Re-sorting once per parent was quadratic. Each re-sort that moves a row is a layout change in which the proxy remaps every persistent index, and a tree view holds one for each expanded row. In a 2,000-process tree almost every parent re-sorted, so the CPU-sorted tree cost grew from 1.1 to 6.1 to 24.7 ms as the list grew from 400 to 1,000 to 2,000 processes. One sort per refresh needs one remap pass. Only the value signals are batched. With dynamic sorting off for a whole refresh, Qt appends new rows unsorted, so every refresh that starts a process would need a full sort where a binary insert suffices. A flat list sends a single block, and Qt's dynamic re-sort of one block only moves the rows whose order changed. A general proxy cannot tell where one refresh's signals end, so the model announces the batch and the proxy decides whether to defer.

**History:** Phase 4, process explorer view; code review F-34 (the first click on Network sorts ascending); maintenance (the single sort per refresh merged in from DC-83).

---

<a id="dc-82"></a>
### DC-82: Process tree and highlights: parents matched by PID, creation time and listing time, and new and exited rows marked for three seconds

**Decision:**

- **Tree mode:** `ProcessModel::setTreeMode()` switches between a flat list (the default) and a tree, and the "Show as tree" check box calls it.
- **Parent matching:** a process's parent is the latest listed process with its parent PID that was created no later than itself and was not marked exited before the process was first listed. Each row records `listedAt`, the snapshot time of the refresh that first listed its process. Unlike `startedAt`, it is set for the first list too and kept for the life of the row, and `findParent()` skips a candidate whose `exitedAt` is earlier than the child's `listedAt`. Nodes are indexed by PID in a multimap for the lookup. Parent PID 0 (reported by the Idle process and by System) makes no process a parent. A candidate that is the process itself, or one of its descendants, is rejected, which can only happen with processes created in the same clock tick.
- **Fixed links:** a process's parent is found when its row is inserted and does not change while both rows are shown. When an exited parent's row is removed, its children first move to the top level with one `beginMoveRows()`. Exited rows are removed deepest first, so a child that moves is never one that is being removed.
- **Insertion:** a new process's parent is looked up among every row, including the other processes that are new in the same list and the exited rows still on show. Each parent's new children are inserted with one `beginInsertRows()`, starting with the parents already shown, so a new parent is shown before its children whatever the list order. Value changes are signalled once per parent that has children.
- **Toggle:** changing the mode resets the model once and relinks every process, oldest first, through `findParent()`. Both times are kept on the rows, so it makes the same links. This is a user action, never a refresh. The view keeps the selected process selected across the reset.
- **Expansion:** switching to tree mode, or changing the filter in tree mode, expands every branch. A process that gains its first child is expanded; a process that already has children keeps the state it has, including one the user collapsed.
- **Filtering and sorting:** the proxy filters recursively, so a match keeps its ancestors shown, and sorts each process's children among themselves ([DC-81](#dc-81)).
- **Highlight time:** `updateProcesses()` takes the snapshot's `steady_clock` timestamp, and `updateTime()` takes it for a snapshot without a list. Highlights start and end on those times; there are no timers.
- **New processes:** a process that appears after the first list has a translucent green background for `kHighlightDuration` (3 seconds). The first list is not highlighted.
- **Exited processes:** a process missing from the list keeps its row and its last values, with a translucent red background and italic text, for `kHighlightDuration`. Its row is then removed, and its children move to the top level. A process listed again while marked is unmarked. An exited row stays a candidate parent while it is shown, under the rule above, so a child it started just before exiting appears under it.
- **Not by colour alone:** the name cell's tooltip says that the process started in the last few seconds or has exited ([DC-85](#dc-85)), and exited rows are italic.
- **Signals:** a highlight that starts or ends emits `dataChanged()` for the whole row, naming the background, font, and tooltip roles.
- **Counts:** the summary line counts the listed processes, so exited rows on show are not counted.

**Rationale:**

- **Creation time:** Windows records only the parent's PID, and that PID can belong to a younger process by the time the child is listed. A parent can never be younger than its child, so the creation-time check rejects a reused PID, as process viewers such as Process Explorer do. "No later than" rather than "earlier than" because creation times are recorded at clock-tick granularity, and a process that starts a child at once can share its creation time. Same-tick creation is also why the cycle check is needed.
- **Listing time:** a process that first appears in a refresh was not in the previous list, which is a complete list of the running processes, so it was created after that list was read. A parent must be running to create a child. A candidate that was already missing from an earlier list had therefore exited before the child was created, and the child's parent PID belongs to a process that was never listed, such as a launcher that started it and exited between two refreshes. The creation-time check cannot catch this, because the exited process is older than the child, and its row stays on show for three seconds. Windows gave a freed PID to a new process after as few as six launches, about 0.4 seconds, in the code review, so the wrong link was not rare. Comparing refresh times needs no extra data from the collector and no clock other than the snapshot timestamps the model already uses. A candidate found missing in the same refresh as the child is new may have started it just before exiting, which is the launcher case that keeping exited rows is for.
- **Fixed links and orphans:** a link never changes because a running process's parent PID never changes, and the process it names either keeps running or exits; it cannot appear later. Moving orphans to the top level is what Process Explorer does, and moving them rather than removing and reinserting them keeps their selection and expanded state. Removing deepest first keeps each move to the rows that outlive their parent's.
- **Insertion by parent:** it keeps the signals per refresh to one per parent that gained children, and inserting parents first is required, because a row must exist before rows can be inserted under it.
- **A reset on toggle:** it is simpler than moving every row, and it happens only when the user asks.
- **Expansion:** `QTreeView` cannot tell a branch the user collapsed from one never expanded, so the view expands a branch automatically only when it gains its first child, which no user can have collapsed yet.
- **Snapshot time for highlights:** the snapshot timestamp is the monotonic clock the rest of the pipeline uses. Driving highlights from it keeps the model deterministic in tests, needs no timer per row, and ends a highlight on the first tick after it is due, which at 1 Hz is within a second. A snapshot arrives every second even when the process list does not change, so `updateTime()` ends highlights between refreshes, including after a failed read.
- **Three seconds:** it spans at least one process refresh at the default 2-second slow interval, so each highlight is on screen across one or two refreshes: long enough to notice, short enough to call brief.
- **Colours and text style:** translucent colours tint whichever row background the style draws, alternating or not, light or dark, and keep the text readable. Red and green alone are hard to tell apart for many colour-blind users, so the exited state also changes the text style and both states have a tooltip.
- **Exited rows kept:** keeping the exited process's last values, rather than blanking them, shows what it was doing when it exited. Highlighting the first list would mark every process as new. Removing an exited row only after its highlight means a parent that exits at once after starting a child, as launchers do, is shown with that child until the parent leaves the table.

**History:** Phase 4, process explorer view; Phase 4, process explorer view (new and exited processes highlighted, and exited rows kept for three seconds); code review F-30 (exited rows parent only processes listed by their exit); code review F-34 (the random-churn test also changes the filter and the mode); maintenance (DC-84 and DC-92 merged in).

---

<a id="dc-83"></a>
### DC-83: Merged into DC-79 and DC-81

The updates of changed cells are described in [DC-79](#dc-79), the single sort per refresh in [DC-81](#dc-81), and the process table benchmark in [DC-66](#dc-66).

---

<a id="dc-84"></a>
### DC-84: Merged into DC-82

The highlights of new and exited processes are described in [DC-82](#dc-82).

---

<a id="dc-85"></a>
### DC-85: Access indicator: a painted icon per details status, and a tooltip with the details or the reason

**Decision:**

- **Indicator:** the name column's decoration is `detailsStatusIcon(detailsStatus)`: an amber padlock when Windows denied access (`AccessDenied`), a blue shield for a process the monitor never opens (`NotQueried`, [DC-13](#dc-13)), and a grey circle with a bar when there was nothing to read (`Unavailable`). A process whose details were read (`Available`) gets a transparent icon of the same size.
- **Painting:** the icons are drawn with `QPainter` on a 16-pixel grid at 1x, 1.5x, and 2x scale. `ProcessModel` paints the four once, in its constructor, and keeps them as members.
- **Tooltip:** the name cell's tooltip (`processDetailsToolTip()`) is rich text. It shows the path and command line when both were read. Otherwise it says why they are missing, in words that match the icon, and then shows whichever one was read. Details are HTML-escaped and cut off after 2,000 characters. The started or exited note of [DC-82](#dc-82) follows them.
- **Header:** the name header's tooltip explains what the icon means.
- **Changes:** details are read once per process, but if they ever change, the name column is signalled with the value changes, which now name the decoration role too.

On the development machine, unelevated, System, Registry, smss.exe and most services show the padlock, both csrss.exe processes show the shield, and the Idle process shows the barred circle.

**Rationale:** The three reasons details can be missing mean different things ([DC-13](#dc-13)). A denial depends on the monitor's rights and is routine when it runs unelevated: about a third of the processes on a desktop refuse it ([DC-75](#dc-75)). Never opening a process is the monitor's own rule, and Unavailable means there is nothing to read. Different shapes and colours tell them apart at a glance without relying on colour alone, and the tooltip says the reason in words. The icons are painted because the project has no resource files yet and `QStyle`'s standard icons vary by style and include no padlock. Painting each scale keeps the shapes sharp on high-DPI screens. The model keeps the icons as members rather than function-local statics, because a pixmap must not outlive the `QGuiApplication`, which statics would. The default delegate leaves room for a decoration only in rows that have one, so the transparent icon keeps every name in the same place. The colours are mid-tones that show on both light and dark rows. Amber suggests a warning, blue a deliberate policy, and grey an absence. The tooltip is rich text because Qt wraps rich-text tooltips, while plain text wraps only when it is wider than the screen, and command lines can run to thousands of characters. Escaping keeps "<" and "&" in a command line literal, and the cut-off bounds the tooltip's size.

**History:** Phase 4, process explorer view.

---

<a id="dc-91"></a>
### DC-91: Merged into DC-79

The Memory column is described in [DC-79](#dc-79).

---

<a id="dc-92"></a>
### DC-92: Merged into DC-82

The parent rule for exited rows is described in [DC-82](#dc-82).

---

## Process termination

Ending a process: the checks in `platform/windows`, and the End process action and its messages.

<a id="dc-87"></a>
### DC-87: Process termination: identity, owner and elevation checked before `PROCESS_TERMINATE` is requested

**Decision:**

- **Result:** `domain::ProcessTerminationResult` (`domain/process_termination.h`) names the outcomes: `Terminated`, `AlreadyExited`, `NotOwnedByCurrentUser`, `Elevated`, `NeverOpened`, `AccessDenied`, and `Failed`.
- **Function:** `platform::terminateOwnedProcess(const domain::ProcessInfo&)` (`process_terminator.h`) takes the process's `(PID, creation time)` identity and its image name. In order, it:
  1. refuses a process the monitor never opens ([DC-13](#dc-13)) as `NeverOpened` before any `OpenProcess` call, and the System Idle Process (PID 0) as `NotOwnedByCurrentUser`;
  2. opens the process with `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE`, compares its creation time with the identity, and checks that it has not exited;
  3. opens its token once, and compares the token's user SID (`GetTokenInformation(TokenUser)`) with that of the monitor's own token (`GetCurrentProcessToken()`), using `EqualSid`;
  4. refuses a process that runs elevated as `Elevated`. `platform::runsElevated()` (`platform/windows/process_elevation.h`) states the rule: the token is elevated (`GetTokenInformation(TokenElevation)`), or its integrity level (`TokenIntegrityLevel`) is `SECURITY_MANDATORY_HIGH_RID` or above: high, system, or protected;
  5. with the first handle still open, opens the process again with `PROCESS_TERMINATE` added, checks the creation time again, and calls `TerminateProcess` with exit code 1.
- **Exited processes:** the result is `AlreadyExited` when `OpenProcess` fails with `ERROR_INVALID_PARAMETER` (no process has the PID), when the creation time differs (another process has the PID), or when the handle is signalled. When a later call fails, reading the token's elevation or integrity level included, a signalled handle also means `AlreadyExited`; otherwise the result is `AccessDenied` or `Failed`.
- **Always:** the monitor ends only unelevated processes, even when it runs elevated itself. On a machine with UAC turned off, every process an administrator starts is elevated, so the monitor ends none of them there. The [roadmap](../roadmap.md#elevated-process-termination) has an item to investigate letting an elevated monitor end elevated processes.
- **System processes:** `lsass.exe` and `csrss.exe` are refused by name. SYSTEM, LOCAL SERVICE, NETWORK SERVICE, and virtual accounts such as `DWM-1` are never the current user, so the owner check refuses them. An unelevated monitor is not even allowed to open them. If the monitor runs as SYSTEM, the integrity check refuses them, since they run at system integrity.
- **When:** the Processes tab calls the function once the user has confirmed End process ([DC-88](#dc-88)), so the checks run then. The snapshot does not say which processes are elevated, so the action cannot be disabled for them in advance.
- **No waiting:** it returns once Windows accepts the request and keeps no state.
- **Logging:** `info` for `Terminated` and `AlreadyExited`, `warn` for a refusal or a denial, and `error` for any other failure, each with the image name, PID, and the failing call's error code. An `Elevated` refusal logs both values, e.g. "it runs elevated (elevated token: false, integrity level 0x3000)".
- **Rules:** the access-rights rule in AGENTS.md (and its copies), `platform/windows/AGENTS.md`, and [architecture.md](architecture.md#unelevated-process-termination) name this function as its one exception.
- **Shared code:** `platform/windows/process_handle.h` holds `ScopedHandle` and `processCreationTime()`, which the details reader ([DC-75](#dc-75)) and the terminator share.
- **Integration tests:** the `ProcessTerminatorIntegration` tests end only suspended `cmd.exe` children that they start ([DC-30](#dc-30)). They start the children they expect to end with `SuspendedChildProcess(UnelevatedToken{})`, which copies the test's own token with `CreateRestrictedToken(LUA_TOKEN | DISABLE_MAX_PRIVILEGE)`, sets it to medium integrity, and starts the child with `CreateProcessAsUserW`, so the child is unelevated even when the tests run elevated. The tests of an elevated child run only when the tests run elevated.

**Rationale:**

- **Identity first:** ending the wrong process is the worst mistake a process explorer can make, so the identity check comes first, as it does for reading details. An open process handle keeps Windows from reusing its PID, so holding the first handle while opening the second means the second reaches the process whose owner was checked, and its creation-time check guards against the same mistake again.
- **Owner and elevation before `PROCESS_TERMINATE`:** checking both before asking for `PROCESS_TERMINATE` means the monitor never asks for termination rights it is not meant to use, which keeps to the reason for the access-rights rule: request only the access that is needed. The owner is the token's user, the account the process runs as and the user name Task Manager shows. An elevated monitor can open processes the user does not own, which the owner check refuses. The owner is checked first, so another user's process, including SYSTEM's, keeps its more specific message, and both checks read the same token handle.
- **Only unelevated processes:** the MVP ends only unelevated processes ([architecture.md](architecture.md#unelevated-process-termination)). Ending elevated ones is a later feature, through a UAC-prompted helper. Ownership alone does not mean Windows allows the termination: without the elevation check, an unelevated monitor would ask for `PROCESS_TERMINATE` on the user's own elevated processes and be refused (`AccessDenied`), while an elevated monitor would end them. Refusing them up front lets the message give the reason instead of reporting a denial.
- **Token elevation or high integrity:** `TokenElevation` is what Task Manager's Elevated column shows, so it matches what the user knows about a program run as administrator. On its own it misses processes that run with more than standard rights without an elevated token. A UIAccess program such as `ctfmon.exe` runs at high integrity for an administrator, and a vendor's service can start a helper in the user's session at high integrity. Windows does not let a medium-integrity caller write to a higher-integrity process, so an unelevated monitor's request to end them would also fail. The threshold is high integrity rather than anything above medium, because medium-plus is not an administrator level; Windows denies a medium monitor's request to end such a process (`AccessDenied`), which is rare. An elevated process that lowered its integrity level still counts as elevated.
- **What the development machine showed:** an unelevated process could read the token of every process it could open, including the user's elevated ones. Of about 400 processes, 151 refused `OpenProcess` (processes that do not run as the user, such as System and the services, which get `AccessDenied`), 4 refused `OpenProcessToken`, and the rest could be read: 2 elevated processes, 2 that were not elevated but ran at high integrity, and the rest unelevated at medium integrity or below.
- **Exited or denied:** `TerminateProcess` fails with `ERROR_ACCESS_DENIED` for a process that has already exited, so without another check an exited process would be reported as denied. A zero-timeout wait tells them apart. It needs `SYNCHRONIZE`, which grants nothing beyond waiting. `GetExitCodeProcess` would need no extra right, but it cannot tell a process that is still running from one that exited with code 259 (`STILL_ACTIVE`). PID 0 is handled by itself because `OpenProcess` rejects it with the same error as an unused PID, which would report the Idle process as already exited.
- **Exit code 1:** a non-zero exit code tells the parent, and anything waiting on the process, that it did not finish normally; 0 would look like success.
- **No waiting:** all the calls return quickly and the next process refresh shows the process leaving, with the exited-row highlight ([DC-82](#dc-82)), so there is nothing to wait for.
- **The result in `domain`:** the UI, which may depend only on domain types ([ui/AGENTS.md](../src/ui/AGENTS.md)), shows a message for each result. It cannot include platform headers, so `main.cpp` passes the function to it, as it passes the collectors to the scheduler.
- **Testing every path:** a test cannot create another user's process, so the owner-mismatch branch is reached only for the System process when the tests run elevated. System is a protected process that Windows does not let an unprotected caller open for termination, so it is the one process outside the test's own children that the tests ask to terminate. `LUA_TOKEN` filters a token as UAC filters an administrator's: the Administrators group becomes deny-only. `DISABLE_MAX_PRIVILEGE` removes every privilege except `SeChangeNotifyPrivilege`. The result is not elevated. Setting its integrity level to medium (or high, for the UIAccess-like case) keeps or lowers it, which needs no privilege. `CreateProcessAsUserW` can start a process with a filtered copy of the caller's own token without `SeAssignPrimaryTokenPrivilege`. The same code therefore runs unelevated, where it changes nothing that matters, and elevated, as on CI runners. Without it, CI would not test the termination path, because every process the tests start there is elevated.

**History:** Phase 4, process termination; Phase 4, process termination (elevated processes refused); code review F-32 (a process without an image name refused, for [DC-13](#dc-13)); maintenance (DC-90 merged in).

---

<a id="dc-88"></a>
### DC-88: End process: one action, a confirmation with Cancel as default, a message for each outcome but success, and an offer to close the monitor on its own row

**Decision:**

- **Action:** `ProcessesView` has one End process `QAction`, offered three ways: an End process button at the right end of the toolbar, the table's context menu (`Qt::ActionsContextMenu`), and the Delete key while the table has focus (`Qt::WidgetShortcut`). It is enabled while a terminator is set and the table has a current process that is not marked as exited (`ProcessModel::hasExited()`), and `confirmEndProcess()` checks both again. It is checked again when the current row changes, after the reset that switching tree mode causes, and after every snapshot, because a row can be marked as exited, or listed again, without the current index changing.
- **Confirmation:** a `QMessageBox` with the warning icon, titled "End process". Its text, from `processTerminationQuestion()`, asks whether to end the process, named with its PID, as plain text. Its informative text, `kEndProcessWarning` in `processes_view.cpp`, warns that unsaved data is lost and that programs that depend on the process may stop working. Its buttons are End process (`DestructiveRole`) and Cancel, which is both the default and the escape button. It is opened with `open()`, so it is window-modal, and it deletes itself when it closes. Its answer is read when it emits `finished`, from `clickedButton()`, so the terminator runs, and any message opens, after the dialog has closed.
- **Identity:** the dialog acts on a copy of the `ProcessInfo` taken when it opened. Confirming passes that copy to the terminator, whatever the table shows by then.
- **Terminator:** `ui::ProcessTerminator` is a `std::function<domain::ProcessTerminationResult(const domain::ProcessInfo&)>`, set with `ProcessesView::setProcessTerminator()`. `main.cpp` sets `platform::terminateOwnedProcess` ([DC-87](#dc-87)). It runs on the UI thread.
- **Result:** a process that was ended gets no message. For every other result, `ProcessesView` shows a `QMessageBox` titled "End process", with one OK button, opened with `open()` (window-modal) and deleted when it closes. `processTerminationMessage()` (`process_formatting.h`) gives its text, which names the process and its PID, and its informative text, which gives the reason in the user's terms. A process without a name has a reason of its own ([DC-13](#dc-13)), and `Failed`'s reason points to the log. The icon is Information for `AlreadyExited`, Warning for `NotOwnedByCurrentUser`, `Elevated`, `NeverOpened` and `AccessDenied`, and Critical for `Failed`.
- **Plain text:** the message's text format is `Qt::PlainText`, which `QMessageBox` also gives the informative text. Only the text contains the process's name. The question and the messages name the process the same way, "explorer.exe (PID 1234)", through one helper.
- **The monitor's own row:** `platform::currentProcessIdentity()` (`platform/windows/current_process.h`) returns the monitor's PID and its creation time from `GetProcessTimes`, and `main.cpp` passes it to `ProcessesView::setMonitorIdentity()`. End process on that row opens a dialog with the question icon instead of the warning. Its text, from `monitorCloseQuestion()`, says that the row is System Monitor itself and asks whether to close it, and its informative text is `kCloseMonitorExplanation`. Its buttons are Close System Monitor (`AcceptRole`) and Cancel, which is the default and the escape button. Confirming closes the window that holds the view, and the terminator is never called. If the identity cannot be read, a warning is logged and the row is treated like any other.
- **One dialog helper:** `askToConfirm()` in `processes_view.cpp` opens both confirmations, so the plain-text question, the Cancel default and the handling of `finished` are written once.
- **Rules:** [ui/AGENTS.md](../src/ui/AGENTS.md) says that an action that changes the system calls a function `main.cpp` sets, and is disabled while none is set.

**Rationale:**

- **One action:** it keeps the button, the menu item, and the key in step: one enabled state, one text, one dialog. The button makes the action easy to find, as Task Manager's End task button does, and the context menu and the Delete key are where Task Manager users also look for it. The key works only on the table, so Delete in the filter box still deletes text.
- **Cancel as default:** Enter, or a key pressed just as the dialog appears, never ends a process; ending one takes a click or a deliberate Tab. The question names the process for the user to recognise, and gives its PID to tell apart processes with the same name, such as the many svchost.exe processes. The name is plain text because the process chose it, and a name that looked like HTML would otherwise be rendered as rich text. A single `arg()` call keeps a `%2` in a name from being replaced by the PID.
- **`open()` and `finished`:** `open()` avoids the nested event loop of `exec()`, which Qt advises against, and lets the tests inspect and answer the dialog without blocking. Window modality keeps the user from starting a second request from the same window while the first is open. `QMessageBox` emits `buttonClicked` while it is still shown, and closes afterwards (`QMessageBoxPrivate::setClickedButton()` emits the signal, then calls `done()`). A message opened from `buttonClicked` would be stacked over the confirmation, and Windows would choose which window to activate when the confirmation hid. `finished` is emitted after the dialog has hidden. The title bar's close button also emits `finished`, but before `clickedButton()` is set, so it ends nothing, like Cancel. `~QDialog` only hides the dialog without emitting `finished`, so a view destroyed while its dialog is open ends nothing.
- **A copy of the process:** the table refreshes every two seconds while the dialog is open. The row it was opened from is removed a few seconds after the process exits ([DC-82](#dc-82)), and the current row then moves to a neighbour. Reading the selection when the user confirms could end a process the dialog never named. The copy keeps the identity the user saw, and `terminateOwnedProcess()` checks its creation time, so a process that exited in the meantime is reported as `AlreadyExited`, and a process that reused its PID is not touched.
- **An injected function:** the UI may depend only on Qt and domain types ([DC-20](#dc-20)), so it cannot name the platform function. A `std::function` suits a single operation better than an interface class, and lets the tests pass a lambda that records identities: a fake, not a mock. Calling an empty `std::function` throws `std::bad_function_call`, and the project uses no exceptions, so the action stays disabled until a function is set, and `confirmEndProcess()` checks again. The terminator runs on the UI thread because `terminateOwnedProcess()` only opens the process, reads its token, and calls `TerminateProcess`, and never waits for the process to exit. A worker thread would add a path back to the UI thread for the result, for calls that do not wait.
- **No message on success:** Windows ends the process asynchronously, and within one process refresh (two seconds) its row is marked as exited ([DC-82](#dc-82)), which shows the user that it worked without a box to dismiss after every success. Task Manager also stays silent when End task succeeds. A failure is rare and needs reading, so it gets a dialog rather than a line the user could miss. The main window has no status bar, and the user is looking at the spot where the confirmation dialog just was. The message is window-modal, as the confirmation is, so the user reads it before starting another request from the same window.
- **The messages:** `AlreadyExited` is not an error, since the user wanted the process gone and it is, so its icon is Information. It still gets a message, because otherwise nothing would happen when the user confirmed, and the list may already show another process with the same PID. The message says that process was left running. The icons follow the levels at which the terminator logs each result ([DC-87](#dc-87)): information, warning for a refusal or a denial, and error for anything else. The message and the log then agree on how serious the outcome was. The reasons describe the user's situation rather than Win32 error codes, and `Failed` points to the log, which has the code. Elevated processes are refused before Windows is asked, so a denial is most often a system process or a service while the monitor runs unelevated, and a protected process can deny even an elevated monitor. The `AccessDenied` reason names both, each with when it happens, so it does not claim that the monitor runs without administrator rights. The process chose its own name, so the name appears only in the text, as plain text, as in the confirmation.
- **Disabled on exited rows:** an exited row has nothing left to end. Confirming only produced the `AlreadyExited` message, which the red, italic row already showed. Checking the action after every snapshot costs one lookup.
- **The monitor's own row:** the monitor is listed like any other process. When it runs unelevated, every check of [DC-87](#dc-87) passes, so End process called `TerminateProcess` on the monitor itself. It stopped at once: `main()` never stopped the scheduler or called `spdlog::shutdown()`, and info-level lines written since the last flush were lost, because the log flushes on its own only at `warn`. When the monitor ran elevated, the request was refused as `Elevated`, which reads oddly when the monitor is asked to end itself. Closing the main window ends the event loop, and `main()` then stops the scheduler and flushes the log. Task Manager also lets users end it from its own list, so offering to close the monitor matches what they expect. Asking first keeps the Cancel default that every End process dialog has. Closing the monitor loses no data, so the question icon fits better than the warning.
- **An injected identity:** the code review suggested comparing the row's PID with `QCoreApplication::applicationPid()`, which needs no platform call. But a process is identified by its PID and creation time, never by its PID alone. Also, the unit tests run in a process whose PID could be one of the PIDs they list, such as 800 or 900, which would make those tests fail now and then. An injected identity keeps to the rule, lets the tests choose the monitor's identity, and follows the pattern of the terminator that `main.cpp` sets.

**History:** Phase 4, process termination; Phase 4, process termination (a message for each outcome but success); Phase 4, process termination (the `Elevated` message, for [DC-87](#dc-87)); code review F-31 (disabled on exited rows, and the monitor's own row); maintenance (DC-89 and DC-93 merged in).

---

<a id="dc-89"></a>
### DC-89: Merged into DC-88

The messages for each termination result are described in [DC-88](#dc-88).

---

<a id="dc-90"></a>
### DC-90: Merged into DC-87 and DC-88

The elevation check is described in [DC-87](#dc-87), and its message in [DC-88](#dc-88).

---

<a id="dc-93"></a>
### DC-93: Merged into DC-88

End process on exited rows and on the monitor's own row is described in [DC-88](#dc-88).

---

## Network tab

The adapter cards: which adapters are listed, in what order, and what each card shows.

<a id="dc-96"></a>
### DC-96: Network tab: a card per adapter in a fixed order, the adapters Windows hides behind "Show all adapters", and link details only while Up

**Decision:**

- **Cards:** `NetworkView` lists the adapters as `AdapterCard`s in a scrolling column, keyed by `NetworkSample::interfaceId` ([DC-35](#dc-35)). When the list changes, the view moves the card of each adapter that is still listed, creates cards for new adapters, and deletes the others. When a read fails, the cards of the last successful read stay. `MainWindow` forwards every snapshot to the view, as it does to the other tabs. `updateSnapshot()` stores it, and the cards are updated only while the view is visible, with `showEvent()` catching up ([DC-54](#dc-54)).
- **Which adapters:** by default the tab lists every adapter that is hardware or bound to IP, whatever its operational status, unless Windows hides it (`isListedByDefault()`). A "Show all adapters" check box adds the rest, after the others, under a "Hidden adapters" heading.
- **Hidden adapters:** `NetworkSample::isHidden` is true for an adapter whose driver has the `NCF_HIDDEN` characteristic. Network Connections and `Get-NetAdapter` leave these adapters out. `queryHiddenAdapterGuids()` (`hidden_adapters.h`) lists the network adapter setup class with SetupAPI, including adapters that are not present, and reads `Characteristics` and `NetCfgInstanceId` from each one's driver key. The network collector matches the GUIDs with `MIB_IF_ROW2::InterfaceGuid`, and reads them again only when an interface appears that was not there at the last read.
- **Order:** `isListedBefore()` puts the adapters listed by default first, then hardware adapters, then compares display names in natural order, then interface IDs. `compareNaturally()` folds case with Unicode's locale-independent folding and compares each run of ASCII digits as a number. The operational status takes no part. The Network tab's `listedAdapters()` and the Performance tab's `chartedAdapters()` both use this order.
- **A card:** its title is the adapter's alias, with its operational status beside it, and the device description is below the title when it differs from it. Readouts (`adapterDetailReadouts()`) show the type, the link speed, the adapter's networks ([DC-100](#dc-100)), the IPv4 and IPv6 addresses, and the DNS servers, and a chart shows receive and send ([DC-98](#dc-98)).
- **Type:** `NetworkSample::type` is a `domain::AdapterType`: Ethernet, Wi-Fi, Bluetooth, mobile broadband, tunnel, PPP, or other. `toAdapterType()` maps it at the platform boundary from `MIB_IF_ROW2`'s `PhysicalMediumType` when that names a medium (802.11, wireless LAN, Bluetooth, wireless WAN, or 802.3), and otherwise from its interface `Type`. Whether the adapter is physical stays in `isHardwareInterface`. `adapterTypeText()` combines the two, e.g. "Ethernet", "Wi-Fi (virtual)" and "Tunnel", or "Virtual" for a virtual adapter of type other. The Performance tab's "Type" readout uses it too.
- **Only while Up:** the link speed, the IPv4 and IPv6 addresses and the DNS servers read "Not connected" unless the adapter is Up. The Performance tab's readouts need no such rule, because its network page charts only adapters that are Up ([DC-53](#dc-53)).
- **Collapsed charts:** an adapter that is not Up shows a line in place of its chart that says throughput is charted while the adapter is Up. The view still records its history, and the card copies the history into the chart only while the chart is shown.
- **Address marks:** on both tabs, `ipAddressText()` marks each address "(temporary)" or "(link-local)" from its flags ([DC-35](#dc-35)). `formatAddressList()`, `ipv4AddressText()` and `ipv6AddressText()` in `network_formatting.h` serve both tabs.
- **The link speed's tooltip:** `Readout` has an optional `toolTip`, which `ReadoutGrid` shows on the readout's name and value after converting it from plain text with `Qt::convertFromPlainText()`. On both tabs, a virtual adapter's reported link speed has `linkSpeedToolTip()`: its driver chooses the speed it reports, so the speed need not limit the adapter's throughput. A hardware adapter's has none.

**Rationale:**

- **Which adapters:** the roadmap asks for all non-loopback adapters. The collector already drops loopback and NDIS filter rows ([DC-35](#dc-35)). What remains still includes adapters that Windows hides from Network Connections: the WAN Miniports, the 6to4, Teredo and IP-HTTPS tunnel pseudo-interfaces, and the kernel debugger adapter. On the development machine the collector reports 19 adapters, and 13 of them are not bound to IP: eight WAN Miniports, the RAS Async Adapter, 6to4, Teredo, IP-HTTPS and the kernel debugger adapter. (`Get-NetAdapter -IncludeHidden`, which leaves out the WAN Miniports, lists ten adapters, four of them not bound to IP.) The Performance tab charts only adapters that are Up. This tab is an inventory, though, so a disconnected Wi-Fi adapter or an unplugged Ethernet port is listed too. The check box keeps the hidden adapters reachable without a second rule.
- **Windows' own flag, not a heuristic:** listed by IP binding alone, four of the six cards on the development machine were Down, and two of the four were Wi-Fi Direct adapters that Windows hides. `MIB_IF_ROW2` has no hidden flag. The review suggested leaving out adapters that are virtual and not Up, but that would also hide the Bluetooth PAN adapter and a disconnected VPN, which Network Connections shows. `NCF_HIDDEN` is what Network Connections goes by, so the default list matches it: on the development machine Ethernet, Wi-Fi, Bluetooth and Mullvad. "Show all adapters" adds the other 15, of which 12 are marked hidden. The flag is set when a driver is installed, so the query runs at the first read and then only when an interface appears. It takes 0.5–0.8 ms on the tick thread. A failed query hides no adapter, which only lists more. The tunnel pseudo-interfaces have no adapter device, so the flag cannot hide them, but `hasIpInterface` still does. The class GUID is written out in the code, because `devguid.h` defines `GUID_DEVCLASS_NET` only after `initguid.h`, an order the include rules do not keep.
- **Cards:** rather than a table, cards give every adapter its own sparkline, which the roadmap asks for, and leave the large charts to the Performance tab's network page. Reusing an adapter's card when the list changes keeps the scroll position and avoids rebuilding widgets.
- **No status in the order:** when adapters that are Up came first, an adapter that came up moved its card to the top and shifted every other card under the scroll position. A card now stays where it is, and only the cards below it move when its chart collapses or expands. With its chart collapsed, a disconnected adapter's card is short, so leaving it in place costs little room. The interface-ID tie-break makes the order independent of `GetIfTable2`'s row order, so cards with the same name do not swap places between reads.
- **Natural, locale-independent order:** "Local Area Connection* 10" sorted before "… 2". `QString::localeAwareCompare()` would make the order depend on the machine's locale, which the readouts avoid for counts too ([DC-57](#dc-57)).
- **One order for both tabs:** the Performance tab sorted with a stable sort and no interface-ID tie-break, so adapters with the same name could swap places when `GetIfTable2`'s row order changed, and the two tabs ordered the same adapters differently.
- **Added cards last:** mixed into the list, the cards that "Show all adapters" added could not be told from the others.
- **Collapsed, not removed:** a disconnected adapter's 110-pixel chart could only draw a flat line at zero. The history keeps being recorded, so when the adapter comes up its chart shows the whole minute, with the time it was down at zero.
- **Type from the medium first:** the interface type alone mislabels adapters. A Bluetooth PAN adapter has the Ethernet interface type (6), and on the development machine only its physical medium told the two apart. Many virtual adapters report an unspecified medium, so the interface type is the fallback: it names tunnels (131) and PPP links. VPN clients such as Mullvad's use the proprietary virtual type (53), which has no better name than "Virtual". Keeping the type separate from the hardware flag lets a Hyper-V or kernel debugger adapter read as virtual Ethernet, which is what it is, and leaves the throughput totals, which count hardware adapters only ([DC-35](#dc-35)), unchanged. The domain type has no unknown value, because every interface row has a type and a medium.
- **"Not connected":** Windows keeps reporting a link speed for adapters that are not Up. On the development machine the disconnected Wi-Fi adapter reported 144.4 Mbps and the Bluetooth adapter 3 Mbps, so showing those values would describe a link that does not exist. The addresses and DNS servers describe the same missing link. The DNS servers are configuration that Windows keeps while the adapter is down: the disconnected Wi-Fi adapter still lists 192.168.1.1 from its last lease. The collector reports them as they are, and the tab decides what to show.
- **Marks on each line:** the Ethernet card showed three similar IPv6 addresses under one "IPv6 address" readout. ipconfig names them "IPv6 Address", "Temporary IPv6 Address" and "Link-local IPv6 Address". A readout has one name for several addresses, so each line carries its mark, and the stable address, like ipconfig's plain "IPv6 Address", carries none.
- **A tooltip rather than leaving the speed out:** a VPN tunnel reports a nominal speed, Mullvad's 100 Gbps, but the Wi-Fi Direct and Bluetooth PAN adapters, which are virtual too, report the rate of their real link. Leaving every virtual adapter's speed out would hide those. The tooltip says what the figure is without hiding it.
- **Plain text in a tooltip:** Qt shows a tooltip as rich text when it looks like markup. The grid converts each one, so a readout's tooltip, like its name and value ([coding guidelines](coding_guidelines.md#text-from-outside-the-program)), never needs to be escaped by its caller.

**History:** Phase 5, network view; code review F-36 (addresses and DNS servers only while Up); code review F-39 (the adapters Windows hides, the fixed order, and collapsed charts); code review F-44 (address marks and the link speed's tooltip); maintenance (DC-97 and DC-106 merged in, and the tab's parts of DC-103 and DC-108).

---

<a id="dc-97"></a>
### DC-97: Merged into DC-96

The adapter type and the link speed are described in [DC-96](#dc-96).

---

<a id="dc-98"></a>
### DC-98: Adapter throughput charts: one minute per card, with the history shared with the Performance tab

**Decision:** Each `AdapterCard` has a receive and send chart, 110 pixels high, set up like the Performance tab's network charts: an auto-scaled byte-rate axis with the shared axis-label templates, a grid, axis labels, and a legend with the current values. `NetworkView` records every adapter's throughput from every snapshot in an `AdapterThroughputHistory` of `kHistoryCapacity` (60) samples, one minute at 1 Hz as on the dashboard, and copies each listed adapter's history into its card when it refreshes. `AdapterThroughputHistory` is the recording logic that `NetworkPerformancePage` had, now shared: for every known adapter, one sample per series per snapshot. A rate that cannot be computed, or an adapter missing from the snapshot (every adapter, when the read failed), records a gap, and an adapter is dropped once it is missing and its history holds no values. The page now uses it with its 1800-sample capacity. `SampleHistory` and `hasAnyValue()` moved from `performance_page.h` to `sample_history.h`, so the class needs no widget header. `build.ps1 -Benchmark` times the tab with 2 to 20 adapters, including a list with "Show all adapters" and an adapter that goes down and up ([DC-66](#dc-66)).

**Rationale:** The tab is an inventory, and its charts show what each adapter is doing now. The Performance tab's network page already offers 5- and 30-minute windows for the active adapters. One minute keeps each card's history at 120 values and its chart sparse, and matches the dashboard. Every adapter's history is recorded, listed or not, so an adapter that comes up, or that "Show all adapters" adds, already has its last minute, and a hidden tab loses nothing ([DC-54](#dc-54)). Sharing the class keeps the two tabs in agreement on gaps and on when an adapter's history ends; a copy would have had to follow every change. Copying histories into the charts grows with the number of adapters, but only the cards in view are painted, and a collapsed chart ([DC-96](#dc-96)) is not updated, so 16 or 20 adapters cost about what 8 do ([performance.md](performance.md)). While the tab is hidden, its only cost is recording, two ring-buffer pushes per adapter.

**History:** Phase 5, network view; code review F-40 and F-41 (re-measured); maintenance (the figures moved to performance.md).

---

<a id="dc-106"></a>
### DC-106: Merged into DC-96

The adapters Windows hides, the adapter order and the collapsed charts are described in [DC-96](#dc-96).

---

## Build and tooling

`build.ps1`, formatting, the Qt build and its plugins, and the benchmarks.

<a id="dc-23"></a>
### DC-23: Formatting enforced with a pinned clang-format and the existing house style

**Decision:** `.clang-format` describes the style the code already followed: indented `case` labels, and a Windows SDK include block whose `SortPriority` keeps the order the SDK requires. Pointers and references bind to the type (`Type* name`, `Type& name`), as in the C++ Core Guidelines (NL.18) and the guideline examples. All files were reformatted once in a formatting-only commit. `build.ps1 -Format` formats the C++ files, and `-CheckFormat`, which CI runs, fails on one that differs. Both cover new files, so a file is formatted before its first commit. CI pins clang-format's version, and the script checks the major version. [build_system.md](build_system.md) describes which files they cover, how the script finds clang-format, and how `bootstrap.ps1` installs it.

**Rationale:** Code review F-7 added `.clang-format`, but nothing ran it, and drift had begun: lines over the hard limit, Qt headers inside the standard-library group, and SDK headers in different orders ([code review F-17](code_reviews/phase_2.md#f-17-clang-format-is-not-enforced)). Matching the configuration to the existing style kept the reformat to alignment and line wrapping, rather than re-indenting every `switch`. The SDK order cannot be left to alphabetical sorting: `iphlpapi.h` sorts before `ws2tcpip.h`, which leaves `CancelMibChangeNotify2` undeclared. clang-format's output changes between major versions, so CI and developers must use the same major version, and PyPI offers the exact release that Visual Studio ships. The standard-library category matches case-sensitively because clang-format's include regexes are otherwise case-insensitive and would put `<QString>` in the standard-library group. The left alignment was chosen over LLVM's default right alignment (`Type &name`, also Qt's style) because the project's guidelines follow the C++ Core Guidelines, which recommend emphasizing the type.

**History:** Code review F-17; Phase 3 (new files formatted before their first commit); code review F-25 (`benchmarks/` formatted too); maintenance (the script's behaviour moved to build_system.md).

---

<a id="dc-62"></a>
### DC-62: `build.ps1`: launch only with `-Run`, configure only when needed, and parallel tests

**Decision:** `build.ps1` builds, and tests with `-Test`, without launching the app; `-Run` launches it. It runs CMake's configure step only when the build directory needs it, which a stamp file of its own records, and CTest runs as many test processes at a time as there are logical processors. [build_system.md](build_system.md) describes when the script configures.

**Rationale:** Builds and test runs far outnumber launches, especially for agents, so launching is the opt-in. Measured on the 16-thread development machine, an edit to one UI file followed by `-Test` took about 26 s: about 3 s re-configuring although nothing had changed, about 4 s compiling and linking, and about 18 s of tests. Each test then ran in its own process, and process startup dominated, so the tests parallelized well. Together the two changes cut that cycle to about 8 s, and a build with nothing to do from about 5 s to under 2 s. The tests now run as GoogleTest shards, about two per logical processor, which keeps every processor busy until the slowest shards finish ([DC-113](#dc-113)).

- **Skipping configure is safe:** Ninja re-runs CMake itself when a `CMakeLists.txt` changes, and the vcpkg toolchain adds `vcpkg.json` and `vcpkg-configuration.json` to `CMAKE_CONFIGURE_DEPENDS`. Ninja does not track the presets files, hence the stamp. `CMakeCache.txt` cannot serve as the stamp, because CMake rewrites it only when its contents change.
- **Running in parallel is safe:** ten consecutive parallel runs passed. The integration tests' timing checks allow up to a second, and the one test that writes a file uses its own directory, so the tests do not interfere with each other.

**History:** Phase 3 (build script); maintenance (GoogleTest shards, for [DC-113](#dc-113)); maintenance (the script's behaviour moved to build_system.md).

---

<a id="dc-66"></a>
### DC-66: Committed tick-cost benchmarks, run with `build.ps1 -Benchmark` (code review F-25)

**Decision:** Two executables in `benchmarks/` time the UI thread's work for one tick. `system_monitor_benchmark` (`performance_tick_benchmark.cpp`) times each Performance page at each chart window, and the Network tab with 2 to 20 adapters: `MainWindow::onSnapshotReady()` followed by `grab()` of a 1280x800 window shown off screen. `system_monitor_process_benchmark` (`process_table_benchmark.cpp`) times the Processes tab applying a list of 400 to 2,000 processes in flat and tree views, with the tab hidden, and shown with a repaint. They are built with every preset but not registered with CTest. `build.ps1 -Benchmark` implies `-Release` and runs both after the build, and after the tests with `-Test`. Each prints the mean and maximum per tick as Markdown tables, after the environment it ran in ([DC-95](#dc-95)). The data is synthetic, fixed and a worst case: every chart series swings across most of its range every 20 to 30 samples, and 40% of the processes change values in every refresh. [performance.md](performance.md) describes the scenarios and records the figures.

**Rationale:** [DC-61](#dc-61)'s first figures came from a benchmark that was not committed, so they could not be reproduced, and the review's rebuilt benchmark differed from them by up to 2x, for reasons its unrecorded data could not show. That benchmark had already caught a 210 ms tick, so later changes to charts and pages need the same tool, and the process table became the next large cost on the UI thread. An executable outside CTest suits timings, which depend on the machine and would make a flaky pass/fail test. Building it with every preset keeps it compiling in CI without running it. The Performance tab records 1800 snapshots while hidden before it is timed, which uses the recording-only path ([DC-54](#dc-54)), so the warm-up takes seconds. Fixed data makes two runs comparable, and a worst case bounds what real data costs: real disk usage, for example, never swings on every sample. Printing through spdlog follows the logging rule.

**History:** Code review F-25; Phase 4, process explorer view (the process table benchmark, for [DC-79](#dc-79)); code review F-33 (the environment printed and the figures reproduced, for [DC-95](#dc-95)); maintenance (the process table benchmark described here rather than in DC-79, and the figures moved to performance.md).

---

<a id="dc-69"></a>
### DC-69: `build.ps1` retries a failed configure once from a clean cache

**Decision:** When configuring fails in a build directory that already has a CMake cache, `build.ps1` removes the cache and configures once more, keeping the restored dependencies and the build outputs. [build_system.md](build_system.md) says what it removes and when it stops.

**Rationale:** After a Visual Studio update, the Release build directory's cache recorded a compiler that no longer matched the preset's `cl`. CMake deleted the cache and re-ran itself, the re-run could not find vcpkg's packages, and the cache it left behind failed every later configure in the same way, including with `-Configure`. Deleting `CMakeCache.txt` and `CMakeFiles` by hand fixed it. A cache created for another source directory (a moved or copied checkout) fails in a similar way, and the retry was tested with that case. The presets define every cache variable and the script passes none of its own, so a clean cache loses nothing. Keeping `vcpkg_installed` avoids restoring dependencies again, and in the test Ninja found nothing to rebuild afterwards. Retrying only once, and only when a cache existed, means a genuine error in a `CMakeLists.txt` still fails, after one extra configure of a few seconds. Deleting the whole build directory was rejected, because it would force a full rebuild and a dependency restore for a problem confined to the cache.

**History:** Phase 3 (build script); maintenance (the script's behaviour moved to build_system.md).

---

<a id="dc-86"></a>
### DC-86: qtbase built with PNG support for Qt's own Windows cursors

**Decision:** `vcpkg.json` adds qtbase's `png` feature. `tests/unit/qt_configuration_test.cpp` checks that Qt can read PNG images and that the Windows platform plugin's cursor resources load.

**Rationale:** With its default features turned off, qtbase was built without its PNG handler (`QT_FEATURE_png` is -1) but still reported `QT_FEATURE_imageformat_png` as 1. Qt's Windows platform plugin checks the second flag and loads the split, open-hand, closed-hand, and drag cursors from PNG resources, which then come back as null pixmaps. Making a cursor from a null pixmap fails `Q_ASSERT(bm.format() == QImage::Format_Mono)` in `qpixmap_win.cpp` in a Debug build, and in a Release build the cursor is not created. The process table's header is the first widget in the app with resizable columns, so pointing at a column border in the Debug app aborted it with that assertion. PNG support is part of Qt's default feature set and adds only libpng. The alternatives were fixed column widths or avoiding every Qt feature that uses these cursors: splitters, resizable headers, and drag and drop. They give up features, and the next widget that uses one of the cursors would crash again. The test fails if the feature is ever dropped, before a cursor reaches the screen. The first build after the change rebuilds qtbase, locally and in CI, until the binary cache holds the new package.

**History:** Phase 4, process explorer view (a crash in the process table's header).

---

<a id="dc-95"></a>
### DC-95: Benchmarks print the build and the machine they ran on (code review F-33)

**Decision:**

- **Build and runtime:** both benchmarks start by printing, through `logBenchmarkEnvironment()` (`benchmarks/benchmark_environment.h`), the MSVC version they were built with (`_MSC_FULL_VER`), the configuration, the Qt version they run with (`qVersion()`), the Windows version (`QSysInfo`), and the number of logical processors.
- **Machine:** before it runs them, `build.ps1 -Benchmark` prints the processor's name, the power plan, and the Windows 11 power mode for the current power source: Best power efficiency, Balanced or Best performance. A value that cannot be read prints as "unknown".
- **Recording:** [performance.md](performance.md) records every table of figures with these lines.

**Rationale:** The code review measured the Performance tab 25–45% above the figures then recorded, and the Processes tab 15–20% above. The end-of-Phase-3 code measured the same, so the code was not the cause, but nothing recorded what had changed in the meantime. Runs on the same machine afterwards were 15–34% below the review's and back at the recorded figures. The review's runs therefore reflected the machine's state, not a lasting change in the code. Which state is not known: the power plan and power mode, which change how fast the processor runs and are easy to change and forget, are the likeliest candidates, so they are now printed with every run. With the environment printed, the next drift can be told apart from a regression without rebuilding older code. The benchmark knows its own compiler and Qt version exactly, and Qt reports the Windows version, so the benchmark prints those. The processor's name and the power settings need registry reads or Windows calls. Only `platform/windows` calls Windows APIs, and the benchmarks depend on the UI and domain modules only, so the script reads them with PowerShell instead.

**History:** Code review F-33; maintenance (the figures moved to performance.md).

---

<a id="dc-104"></a>
### DC-104: Qt's Windows style plugin deployed next to every executable (code review F-42)

**Decision:** The top-level `CMakeLists.txt` defines `sysmon_deploy_qt_plugins()`, which copies two plugins next to an executable after it links: the Windows platform plugin into `platforms/`, as before, and `Qt6::QModernWindowsStylePlugin` (`qmodernwindowsstyle.dll`) into `styles/`. The app, the tests and both benchmarks call it, in place of three copies of the platform plugin's step. The app does not choose a style: Qt picks `windows11` on Windows 11 and `windowsvista` on Windows 10. The `QtConfiguration` tests fail if either style is missing.

**Rationale:** The style plugin is loaded at run time, like the platform plugin. Only the platform plugin was copied, though, and the comment on that step said it was the only one needed. Without a `styles` directory, Qt falls back to its built-in classic `windows` style: bevelled check boxes, scroll bars, tabs and frames, and that style's dark palette. vcpkg's qtbase port ships no `windeployqt`, so the build copies the plugins itself, and one function deploys a plugin added later to every executable at once.
- **Tests and benchmarks get the same plugins** as the app, so that the layouts, colours and tick costs they measure are the ones the app shows.
- **Layout:** under `windows11` every tab still fits an 800×600 window, in both colour schemes.
- **Colours:** the style draws tab pages and card frames in the palette's base colour (#ffffff in the light scheme, #2d2d2d in the dark one), not the window colour. Its placeholder-text colour is the text colour at half opacity, which reaches only 4.0:1 on white. The [text colours](coding_guidelines.md#text-colours) are chosen against what this style draws.
- **Cost:** the style costs the Network tab, and the Performance tab's network page, about 1 ms a tick; on the other pages the difference is lost in the noise ([performance.md](performance.md)).

**History:** Code review F-42; maintenance (the figures moved to performance.md).

---

## Testing

How collectors are made testable, what integration tests assert, and how tests are shared and run.

<a id="dc-21"></a>
### DC-21: `test_main.cpp`: listing the tests creates no `QApplication`

**Decision:** The test runner (`tests/test_main.cpp`) creates no `QApplication` when `--gtest_list_tests` is passed. It runs the tests with a `QApplication` otherwise.

**Rationale:** Creating `QApplication` loads the platform plugin, which fails when the plugins are not deployed yet and can fail without a display session, with a modal error dialog or a timeout. Listing the tests needs no plugin, so it is instant and works anywhere. The build itself no longer lists the tests: CTest runs GoogleTest shards, not tests that `gtest_discover_tests` found at build time ([DC-113](#dc-113)). The check stays, so that listing the tests by hand or from a tool starts no `QApplication` either.

**History:** Phase 1, basic UI shell; maintenance (the build no longer lists the tests).

---

<a id="dc-27"></a>
### DC-27: Collectors take their OS calls as injected readers, beside pure calculation functions

**Decision:** Each collector has a default constructor that calls the Windows APIs and a constructor that takes them as injected `std::function` readers, which unit tests supply. What a collector computes from the readers' data is a pure function that tests call directly. The Phase 2 collectors set the pattern:

- `CpuCollector` takes a `SystemTimesReader`, a `CorePerformanceReader` and a `SteadyClockReader`, and computes with `calculateCpuUsage`.
- `MemoryCollector` takes a `MemoryStatusReader` and a `PerformanceInfoReader`, and computes with `calculateMemorySample`.
- `DiskCollector` takes a `FixedDriveEnumerator` and a `DiskSpaceReader`, and computes with `calculateDiskSample`.
- `NetworkCollector` takes a `NetworkAdaptersReader` and a `SteadyClockReader`, and computes with `calculateNetworkSamples`, which keeps each adapter's baseline and drops the baselines of adapters that have left.

The later collectors take theirs the same way, for example the process list query and details reader ([DC-70](#dc-70), [DC-73](#dc-73)) and the connectivity subscriber ([DC-39](#dc-39)).

**Rationale:** Unit tests must be fast, deterministic, and free of OS dependencies. Injected readers and clocks let them produce what a real machine rarely shows on demand, without calling the Windows kernel, relying on sleep timers, or changing the host's configuration:

- **CPU:** exact idle, user and kernel ratios, zero elapsed intervals, clock jitter in which idle time briefly exceeds kernel time, and failed queries.
- **Memory and disk:** zero total memory or disk space, free space above the total because of quotas, a full volume, failed queries, and several volumes, without creating partitions.
- **Network:** several interfaces, varying link speeds, counters that go backwards, IP and DNS configurations, filtered loopback interfaces, and baselines dropped across elapsed intervals, without network privileges or changes to the hardware.

Integration tests in `tests/integration/` call the real APIs separately ([DC-30](#dc-30)).

**History:** Phase 2, CPU collector; Phase 2, memory and disk collector (the memory and disk collectors); Phase 2, network collector (the network collector); maintenance (DC-33 and DC-38 merged in).

---

<a id="dc-30"></a>
### DC-30: Integration tests assert invariants of the real machine, and the process tests use a suspended child process

**Decision:** Integration tests run against the live Windows APIs and assert what holds on every machine, rather than fixed values.

- **CPU:** over several consecutive real samples, usage stays within $0.0 \le \text{usage} \le 100.0$ and is never NaN or infinite, the core count matches `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`, and usage rises while the test runs a multi-threaded synthetic load.
- **Processes:** `tests/integration/process_tracking_test.cpp` tests the process collector end to end:
  - **Enumeration:** a `cmd.exe` child, created suspended with a command line unique to the run, appears with its exact identity, this process as its parent, and its real path and command line. After it is terminated and its handles closed, it leaves the list. The test polls for up to 5 seconds, because the list can take a moment to drop an exited process. In the Phase 4 code review, a terminated child whose handle was still held open had left the list 0.5 seconds later.
  - **Cache behaviour:** a collector built from the real list reader and a counting wrapper around the real details reader reads exactly the identities of the first list, minus the processes it never opens ([DC-13](#dc-13)). On the second collect it reads exactly the identities that are new, the child among them. The sets are compared exactly.
  - **Access-denied handling:** every process's `detailsStatus` must match the details present (`Available` with both, `NotQueried` only for the processes never opened and with neither, `AccessDenied` or `Unavailable` with at most one). When the test runs unelevated, at least one process must deny access and still carry its name, creation time, and memory counters. When elevated (as on CI runners), that test is skipped.

  Paths are compared ignoring case, because `GetSystemDirectoryW` can return `C:\WINDOWS\system32` while `QueryFullProcessImageNameW` returns the on-disk case.
- **The scheduler:** one test runs every collector the app registers ([DC-110](#dc-110)).

**Rationale:** Unlike unit tests with injected data ([DC-27](#dc-27)), integration tests run against live kernel APIs, where background OS services, indexing, and power-saving C-states make utilization fluctuate continuously. Asserting fixed numbers produces flaky tests. Invariants over several real samples, and a rise under a controlled synthetic load, show that the collector works on real hardware under variable load without false positives.

A child created by the test is the one process whose identity, parent, path, and command line are known in advance. Creating it suspended means it never runs, needs no window, and cannot exit on its own during the test, and it shows that the command line is readable before the process has started. PID reuse cannot be forced on a real machine, so the unit tests cover it ([DC-73](#dc-73)). Instead, the exact read sets show that the real cache reads each identity once. Which processes deny access depends on the account and elevation, so the access test asserts invariants that hold everywhere, and counts denials only where they are certain to occur. Both follow the integration-test rule to tolerate varying environments.

**History:** Phase 2, CPU collector; Phase 4, process collector (the process tracking tests); code review F-33 (why an exited child can stay listed); maintenance (the scheduler pipeline tests replaced by [DC-110](#dc-110)'s, and DC-78 merged in).

---

<a id="dc-33"></a>
### DC-33: Merged into DC-27

The memory and disk collectors' injected readers are described in [DC-27](#dc-27).

---

<a id="dc-38"></a>
### DC-38: Merged into DC-27

The network collector's injected readers are described in [DC-27](#dc-27).

---

<a id="dc-78"></a>
### DC-78: Merged into DC-30

The process integration tests are described in [DC-30](#dc-30).

---

<a id="dc-109"></a>
### DC-109: Shared test builders and harnesses in `tests/support/`, header-only

**Decision:** Test data builders (`adapter()`, `process()`, `populatedSnapshot()`, `demandingSnapshot()`, …) and harnesses (`waitForSnapshot()`, `FakeSubscription`, `ScopedLogCapture`, the text contrast checks) that more than one test file uses live in `tests/support/`, as inline functions and classes in headers, in namespace `sysmon::tests`. `system_monitor_tests` adds `tests/` to its include path, and tests include them as `"support/<name>.h"`. A builder returns a realistic default, such as an Ethernet adapter that is Up, bound to IP and receiving 2 KiB/s, and takes only the values its callers' cases differ in. Tests set any other field on the result. `tests/support/` follows the rules of `tests/unit/`: no Windows headers. Helpers that need the Windows SDK, such as `SuspendedChildProcess`, stay beside the integration tests that use them.

**Rationale:** Six adapter builders and four process builders had drifted apart: names with and without braces, IDs from a hash or none, rates or none. A test's data depended on the file it was in, and a new field in a domain type had to be added to each copy. Helpers of a few lines need no library target of their own, and headers compile into the one test executable that uses them. Named `AdapterTraits` constants (`kUnboundVirtualAdapter`) replace the positional `bool` arguments of the old builders. `process()` takes its creation time as `std::chrono::seconds`, whose constructor is explicit, so a CPU percentage passed in its place does not compile. `waitForSnapshot()` waits on a condition variable for a snapshot that satisfies a predicate instead of polling a count. Two of the loops it replaced appended to a `std::vector` on the tick thread while the test thread read its size, which is a data race.

**History:** Maintenance, tests.

---

<a id="dc-110"></a>
### DC-110: One scheduler pipeline test, with the collectors `registerSystemCollectors()` registers

**Decision:** `sysmon::app::registerSystemCollectors()` (`src/app/system_collectors.h`, in the `sysmon_app_collectors` library) registers the Windows collector of every metric with a `SamplingScheduler`. `main.cpp` calls it, and so does `SchedulerPipelineIntegration.AppCollectors_SnapshotCarriesEveryField`, the one integration test that runs the scheduler with real collectors. The test waits for a snapshot that has per-core CPU values and the slow collectors' results, then checks every field of `SystemSnapshot`, including the health the scheduler evaluated from it. It replaces the five per-collector `SchedulerPipeline_*` tests and the process tracking tests' scheduler test.

**Rationale:** Each per-collector test registered one collector and checked one field. Together they showed that each collector works with the scheduler, but not that the app registers them all, or that every field is right while all of them run at once, as they do in the app. A copy of `main.cpp`'s registrations in the test could drift from it; a shared function cannot. A library of its own follows `sysmon_app_logging`, which exists so that tests can exercise it. The test waits for a condition, not a number of snapshots: the old CPU test took the third snapshot and failed now and then under the parallel run, because the collector leaves out the per-core values of a tick in which some core's counters did not advance. A collector of the test's own first finds out whether the Network List Manager answers on the host, so a host without it does not fail the test.

**History:** Maintenance, tests.

---

<a id="dc-111"></a>
### DC-111: Fixed-value fake collectors in `tests/support/`, not synthetic collectors in `monitoring`

**Decision:** `SyntheticCpuCollector` and `SyntheticMemoryCollector`, which generated varying values for Phase 1, are removed from `src/monitoring/` with their tests. Tests that need a CPU or memory collector use `FakeCpuCollector` and `FakeMemoryCollector` from `tests/support/fake_collectors.h`, which return the same sample from every `collect()`, by default `populatedSnapshot()`'s.

**Rationale:** Since Phase 2 only tests used the synthetic collectors, so they were test code in a production library, with tests of their own that checked the generated values. The three tests that used them needed a collector that returns a sample, and none depended on the variation. A fixed sample lets those tests check exact values instead of "not the placeholder", like the other fakes (`FakeUptimeCollector`, `FakeProcessCollector`, …), which return what the test sets. Moving the generators to `tests/support/` unchanged would have kept code, and tests of it, that no test needs.

**History:** Maintenance, tests; maintenance (the `Collector` concept removed, and with it the `static_assert`s on the fakes, [DC-18](#dc-18)).

---

<a id="dc-112"></a>
### DC-112: A mapping's values tested in one test, as a table of named cases

**Decision:** A mapping or a threshold function is tested by one test that loops over a table of cases, a local `Case` struct in a `std::array`, and passes each case's name to `SCOPED_TRACE`. It is not tested by one `TEST` per value, nor by a parameterized `TEST_P`.

**Rationale:** A test per value repeated the same line under a name that restated the value (`ToConnectivityLevel_LocalAccess_ReturnsLocalAccess`), and one mapping's cases were spread over as many as fourteen tests. A table shows them side by side, so a missing value stands out, and `SCOPED_TRACE` names the row that fails. `TEST_P` also reports each row on its own, but it needs a fixture, an instantiation and a printer for the parameter, and `gtest_discover_tests` runs each row in a process of its own. `TEST_P` remains the tool for running a whole test over different inputs, as `SparklineBufferSize` runs the same checks at three buffer sizes.

**History:** Maintenance, tests.

---

<a id="dc-113"></a>
### DC-113: Tests run as GoogleTest shards, one CTest test each, not one process per test

**Decision:** `tests/CMakeLists.txt` registers each test executable as GoogleTest shards rather than calling `gtest_discover_tests`: `system_monitor_tests` as twice as many shards as the configuring machine has logical processors, and `system_monitor_platform_tests` as one. Each shard is a CTest test that sets `GTEST_TOTAL_SHARDS` and `GTEST_SHARD_INDEX`. `build.ps1 -Test` still runs CTest with `--parallel` set to the number of logical processors.

**Rationale:** Every `system_monitor_tests` process creates a `QApplication` and loads its plugins before its first test. With a process per test, the 775 tests took 194 s of process time, and 12.6 s on 16 threads, while each executable ran all of its tests in one process in about 9 s together. On the same machine, in Debug, the 717 tests took 11.7 s with a process per test, 4.0 s with a process per suite (75 suites), 2.8 s as 16 shards, and 2.1 to 2.6 s as 32. GoogleTest deals the tests to the shards round-robin, so a shard's time depends on the slow tests it draws (the slowest takes 1.1 s), and with more shards than processors CTest starts another while the slowest finish. Registered with CTest, the shards keep its scheduling, `--output-on-failure` and timeouts, with no process management in `build.ps1`, and CI, which runs CTest without `--parallel`, starts a few processes rather than one per test. Before the change the tests passed in shuffled orders (`--gtest_shuffle`: ten of the main executable, twenty of the platform one), so the order within a shard cannot change a result, and eight consecutive runs as shards passed. A failed shard's output names the failing tests, and one test runs with the executable's `--gtest_filter` rather than `ctest -R`.

**History:** Maintenance, tests.

---

## Design decision log

How this file is organized and kept current.

<a id="dc-114"></a>
### DC-114: Links name an entry's ID anchor, not its heading, and `build.ps1` checks that they resolve

**Decision:** Each entry heading has `<a id="dc-NN"></a>` on the line above it, and every link to an entry names that anchor. `Test-DesignDecisionLinks` in `scripts/build.ps1` runs after clang-format in both `-Format` and `-CheckFormat`. It reads this file and every tracked or new text file (Markdown, C++, CMake, PowerShell, …), and fails when an entry heading lacks its anchor, when a Markdown link into this file does not lead to it or names neither an anchor nor a `##` section heading, when a link whose text is `DC-NN` names another entry, or when a file cites a `DC-NN` that is not an entry. An entry whose heading says that it was merged or retired is a stub ([DC-115](#dc-115)). In this file only its line in the contents may link to it, and no other file may cite it, apart from those in `docs/code_reviews/`.

**Rationale:** Links named GitHub's slug of the heading, which is the whole title, so retitling or merging an entry broke every link to it, and nothing noticed. The ID does not change, and merged entries keep theirs as stubs. The anchor sits on a line of its own, not inside the heading, so headings read as before and `^### DC-` still finds them. In CommonMark a line holding only an inline tag is a paragraph, which the heading ends. An entry heading's slug is not accepted, so a link written the old way fails at once rather than at the next retitle. Section slugs are, because the contents list uses them. Code comments cite entries as plain `DC-NN`, which the citation check covers. The check needs nothing installed: it is PowerShell in the script that CI's Format Check job already runs, and it reads only the repository. `-Format`, which AGENTS.md says to run before committing, runs it too, so a broken link shows up before CI does. A stub resolves, so a link or citation that still names one passes the other checks but sends the reader to a pointer instead of the decision. The code reviews keep theirs, because they record what each review led to at the time. This file's history lines and rationale may name a stub in plain text, to say what was merged.

**History:** Maintenance, design decisions; maintenance (no link or citation may name a stub, for [DC-115](#dc-115)).

---

<a id="dc-115"></a>
### DC-115: Living entries: an entry states the current decision, is rewritten when it changes, and ends with a one-line history

**Decision:**

- **Current state:** an entry describes the decision as the code now has it. A change that answers its question differently, or adds to the answer, rewrites its Decision and Rationale under the same ID. It adds no note ("Superseded in part by", "Extended by", "Amended", "Note"), and no new entry that patches the old one. A new entry is for a new question. The Rationale keeps an earlier choice only as a rejected alternative, when the reason it was dropped still argues for the current one.
- **History line:** an entry ends with `**History:**` followed, separated by semicolons, by where the decision was made and each change since: a phase and its roadmap section (`Phase 4, process collector`) or the known deviation it resolved (`Phase 2, known deviation D-6`), a code-review finding (`code review F-29`), or `maintenance`. Each change says in parentheses what it changed and, if it was made for another entry, which one. DC-76's reads "Phase 4, process collector; Phase 4, process explorer view (I/O rates, for DC-79); code review F-33 (what the CPU column adds up to)".
- **Git for the rest:** `.gitattributes` gives Markdown files Git's `markdown` diff driver, so `git log -L ':DC-76\::docs/design_decisions.md'` lists every commit that changed an entry, with each version, and diff hunk headers name the entry a change is in.
- **Stubs:** an ID is never reused, renumbered or deleted. An entry merged into another, or retired, keeps its anchor, its heading, retitled to say so (`DC-89: Merged into DC-88`, `DC-102: Retired, the rule is in the coding guidelines`), and its line in the contents. Its body is one line that says where the decision is now. A stub whose decision went to two entries names both (`DC-103: Merged into DC-35 and DC-96`). Links and citations elsewhere in the repository move to that entry, except in `docs/code_reviews/`, which record what each review led to at the time, and `build.ps1` fails on one that still names a stub ([DC-114](#dc-114)).
- **Merges:** entries that patch one another, or answer one question between them, are merged into one entry per topic ([DC-116](#dc-116)). The lowest ID among them under that topic keeps the merged decision, and the others become stubs. A chain whose entries sit under different topics becomes one entry under each: the slow-collector thread is in DC-77, under the scheduler, and the connectivity collector that runs on it in DC-39. The merged entry's history line keeps the sources of every entry merged into it and ends with the merge: `maintenance (DC-72, DC-74 and DC-94 merged in)`.
- **Applied:** every entry ends with its history line, and the notes in DC-06, DC-18, DC-21, DC-46, DC-70, DC-71, DC-76 and DC-111 are folded into their entries. 34 entries that patched one another are merged into DC-12, DC-13, DC-27, DC-30, DC-35, DC-39, DC-48, DC-77, DC-79, DC-81, DC-82, DC-87, DC-88, DC-96 and DC-100. The notes in DC-62 and DC-66 are folded in too, and DC-98's went to performance.md with the figures it recorded ([DC-117](#dc-117)). The last note, DC-68's, went when DC-68 was retired ([DC-117](#dc-117)).

**Rationale:** The log was append-only: a change left the earlier entry as written and added a note to it. 40 notes patched 31 entries, so learning how End process works meant reading DC-88 and its two notes, then DC-89, DC-90 and DC-93, and working out which sentences still held; DC-88 still said the result was not shown. A reader, or an agent, who stopped at the first entry followed a decision the code no longer makes. A living entry is right when read alone. What it drops, the wording of earlier versions, git keeps beside the commit message that explains each change.

- **Immutable records**, as in Michael Nygard's architecture decision records, where a new record supersedes an old one, suit decisions made once that an audit has to trace. Here most changes refine a decision, often after a code-review finding, and a new record would repeat most of the old one.
- **A change log in each entry** would grow as the notes did, and the Decision would still need rewriting to be right. The history line keeps only where each change came from, which is enough to find the finding in `docs/code_reviews/` and the commit in git.
- **The `markdown` driver:** without it Git finds no heading in a Markdown file, so `-L ':DC-76\::…'` fails, and hunk headers show the nearest line above that starts with a letter. The escaped colon matches only the entry's heading, not one whose ID starts with the same digits or a stub that names DC-76. Moving an entry, as grouping by topic did ([DC-116](#dc-116)), looks to `git log -L` like adding it; the same command run from that commit's parent continues further back.
- **The lowest ID under the topic:** it is the decision's oldest ID, so the oldest links lead to all of it, and choosing it needs no judgement. One entry for a chain that spans topics would take a decision away from the code it decides: the slow-collector thread, which runs the process and network-name collectors too, would have become part of a connectivity entry.

**History:** Maintenance, design decisions; maintenance (a known deviation as a source, and every entry given its history line, for [DC-116](#dc-116)); maintenance (how chains are merged, and stubs neither linked nor cited); maintenance (the notes in DC-62, DC-66 and DC-98, for [DC-117](#dc-117)); maintenance (a retired entry's title, for [DC-117](#dc-117)).

---

<a id="dc-116"></a>
### DC-116: Entries grouped by topic, in ID order within each topic

**Decision:** The entries are grouped under `##` headings by what they decide, not by the phase that made them: Windows APIs, modules and dependencies, the domain model, the scheduler and collectors, each collector area, charts, the main window and shared UI, each tab and process termination, build and tooling, testing, and this log. A line under each heading says what the topic covers. Within a topic the entries are in ID order, so a new entry goes at the end of its topic. An entry that spans topics goes under the one whose code it mostly decides, and the contents list each entry once. The phase that made an entry, and each phase or finding that changed it, are in its **History:** line ([DC-115](#dc-115)).

**Rationale:** A reader comes to this file with a question about one part of the code, not about one phase. Later phases change earlier decisions, so under phase headings one topic's entries were spread out: the network names over "Windows API choices" and Phase 5, the connectivity collector over Phases 2, 4 and 5. Entries made for code-review findings went under whichever heading was near: DC-94 among DC-70 to DC-78, and DC-91, DC-92 and DC-95 under the process explorer view. The phase still leads to the roadmap item or the review behind a decision, so the history line keeps it.

- **ID order within a topic:** filing a new entry needs no judgement beyond its topic, and no entry moves when one is added. A reading order would have to be decided again for every new entry.
- **One file:** links name `design_decisions.md#dc-NN` ([DC-114](#dc-114)), so moving an entry to another topic breaks none, and one search covers every decision. A file per topic would put the topic into every link.
- **Process termination as a topic:** its checks in `platform/windows` and its action on the Processes tab are one feature, with an entry for each: the checks in [DC-87](#dc-87) and the action in [DC-88](#dc-88).

**History:** Maintenance, design decisions.

---

<a id="dc-117"></a>
### DC-117: Entries hold decisions: benchmark figures in `performance.md`, build-script behaviour in `build_system.md`, rules in the coding guidelines, and no test lists, copied UI text or code locations

**Decision:**

- **Benchmark figures:** [performance.md](performance.md) records what `build.ps1 -Benchmark` prints: a baseline per tab with the environment lines ([DC-95](#dc-95)), and a dated history of earlier baselines and measured changes. An entry quotes at most the figure that justifies its choice, and links there.
- **Build-script behaviour:** what `build.ps1` and `bootstrap.ps1` do, and when, is in [build_system.md](build_system.md) and the script's help. An entry about the script keeps what it chose and why.
- **Tests:** an entry lists no tests. The test names list the cases, and an entry names at most the suite. How something is tested is in an entry only when it is a decision of its own, such as the unelevated children of the termination tests ([DC-87](#dc-87)).
- **UI text:** an entry names the function or constant that gives a text, such as `processTerminationMessage()`, rather than copying it. Short labels, such as "N/A" or "Show all adapters", are still quoted.
- **Rules:** a rule that every change follows is in the [coding guidelines](coding_guidelines.md), with its reasons, and in short in AGENTS.md or the module's AGENTS.md. An entry does not restate it, and links to the guideline where its decision applies it. An entry that only restated a rule is retired, and the reasons it gave move to the guideline.
- **Code locations:** a header's comment says what it holds. An entry names the function, type or header that carries its decision, but a list of what lives where is not a decision, and an entry that was only such a list is retired.
- **Applied:** the figures of DC-61, DC-66, DC-79, DC-95, DC-98 and DC-104 are in performance.md, with a baseline of all three tabs measured on 2026-10-18. DC-23's, DC-62's and DC-69's account of the script is in build_system.md. The test lists in DC-13, DC-35, DC-79, DC-82, DC-87, DC-88, DC-96, DC-100, DC-102 and DC-105 are dropped, and DC-13, DC-79 and DC-88 name formatters instead of their text. DC-26, DC-68, DC-102 and DC-105 are retired: the guidelines' "Windows API wrapping" now says that a header other modules include has no Windows headers, which was all of DC-26 that still held; the headers that DC-68 listed say what they hold; and the reasons that DC-102 and DC-105 gave are in the guidelines' "Text from outside the program" and "Text colours", which already stated their rules.

**Rationale:** Test lists, benchmark tables and copied text made up much of the longest entries, and none of it is a decision. Each copy went stale without anything noticing: figures change with the code and the machine, a test list with every test, a text with every rewording. The tests, the formatters and the script's help are current by construction, so an entry points to them. Benchmark figures have no such place in the code, so they get one document, where a tab's figures sit beside its last baseline, rather than six entries' tables, each from a different day and none current. An appendix to this file was rejected: the figures would still grow the file that is searched for decisions.

- **Rules in the guidelines:** a rule kept in two places drifts too. DC-26 said that Windows headers stay inside translation units, but eight headers in `platform/windows` include them, as the module's rules allow for a header that no other module includes. A rule is read where code is written, so its reasons belong beside it, where someone who wants to bend it finds them; an entry is found only by someone already looking for the decision.
- **No code locations:** a list of what lives where goes stale with every move, and DC-68's already named a sort that DC-96 had replaced. The headers' own comments move with the code.

**History:** Maintenance, design decisions; maintenance (rules in the coding guidelines and no code locations; DC-26, DC-68, DC-102 and DC-105 retired).
