# Design Decisions

Rationale for non-obvious implementation choices made during development, grouped by topic ([DC-116](#dc-116)). The line under each topic's heading says what it covers, and an entry's **History:** line names the phase or code-review finding that made it.

An entry holds a decision and its reasons in about 250 words, not counting its heading and its **History:** line ([DC-117](#dc-117)). Its fields, each at the start of a line, are **Decision:**, what was chosen; **Rationale:**, why, and the alternatives rejected; **Consequences:**, only when the choice has a cost or constrains later changes in a way the Rationale leaves out; and last **History:**. `.\scripts\build.ps1 -Format` and `-CheckFormat` fail on a field that is missing, unknown or out of order. An entry quotes at most the one measurement that justifies its choice, and a merged entry gives each of its parts a sentence. Measured tick costs are in [performance.md](performance.md), what `build.ps1` does is in [build_system.md](build_system.md), the rules that every change follows are in [AGENTS.md](../AGENTS.md) and the [coding guidelines](coding_guidelines.md), with their reasons, and the tests, the program's text and where code lives are in the code.

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
  - [DC-117](#dc-117): Entries: Decision, Rationale, optional Consequences and History in about 250 words, with figures, script behaviour, rules, tests, UI text and code locations kept elsewhere

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

- **Samples:** the CPU, memory and uptime samples are optional, and a calculation returns `std::nullopt` for input it cannot use, such as a counter that went backwards or a zero capacity. Per-core usages are all or nothing: an empty list means unavailable.
- **Collections:** the disk, network and process lists are `std::optional<std::vector<...>>`. `std::nullopt` means no collector or a failed query, and an empty vector a query that found nothing. A volume that cannot be read is skipped and logged.
- **Rates:** a rate without a baseline, as on an adapter's or a process's first sample, is `std::nullopt` ([DC-34](#dc-34), [DC-76](#dc-76)).
- **Text:** a process's image path and command line are optional strings, with `detailsStatus` saying why one is missing ([DC-13](#dc-13)).
- **Enums:** connectivity ([DC-39](#dc-39)) and health ([DC-45](#dc-45)) have an Unknown level.
- **In the UI:** a missing value shows "N/A", and a value before the first snapshot "--".

**Rationale:** A 0% CPU reading cannot be told from an idle system or a broken query ([known deviation D-7](known_deviations.md#d-7-memory-and-cpu-data-report-zeros-when-missing)), a zero-capacity volume would read as 0% used ([D-8](known_deviations.md#d-8-zero-capacity-disk-volumes-report-0-usage)), and a missing sample must not read as healthy. A genuine 0% is still reported. A partly filled per-core list would misalign the core indices. With plain vectors, a failed query looked like a machine with no adapters, and the dashboard said "No active adapter" when nothing had been observed ([code review F-12](code_reviews/phase_2.md#f-12-empty-disk-and-network-vectors-hide-collector-failures)). The volumes that can be read are real data, so a partial result is still a list. An empty path could mean not yet queried, failed, or none, while `std::nullopt` says the data is not available.

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

**Decision:** `SamplingScheduler` owns a second `std::jthread` that runs the slow collectors, processes, connectivity and network names, every `slowInterval()` (2 seconds by default), outside every lock, and then moves the results into a mutex-protected `SlowResults`. The tick runs only the fast collectors and copies the latest slow results into its snapshot. Until the first slow refresh, a snapshot has no process list, no network names and `ConnectivityLevel::Unknown`. Connectivity and the network names are `IEventDrivenCollector`s, whose change listener the scheduler sets when it registers them. A collector calls it, with its own mutex held, once it has stored a changed value. The listener wakes the slow-collector thread, which then collects only the event-driven collectors and keeps the process list and its cadence.

**Rationale:** A process refresh takes 3.5–6 ms on a 400-process desktop ([DC-75](#dc-75)), several times what the fast collectors share, and on the tick it would make the 1 Hz cadence uneven. Building results before taking the mutex means the tick waits only for a move. Running the whole connectivity collector there, not only its polling fallback, keeps every connectivity call off the tick ([D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)). Not waiting for the first refresh keeps `start()` and the first tick immediate. Polled every 2 seconds and merged up to a second later, a notified change would reach the UI up to 3 seconds late. An extra snapshot on a change would put two samples into the histories in one second, and a full refresh would enumerate processes off their cadence during a reconnect's burst of changes. `main.cpp` creates the collectors before the scheduler, which is what needs waking, so the scheduler sets the listener. Calling it with the collector's mutex held means that once the scheduler clears it, no call is still running; locks are always taken collector first, then the sleep mutex.

**History:** Phase 4, process collector (known deviations [D-1](known_deviations.md#d-1-sampling-scheduler-has-no-slow-collector-thread) and [D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)); Phase 5, network view (an event-driven collector wakes the thread); code review F-38 (the network names wake it after their read, for [DC-100](#dc-100)); maintenance (DC-101 merged in).

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

**Decision:** Every process stays in the list, and `ProcessInfo::detailsStatus` says whether its image path and command line were read: `NotQueried` by default, `Available`, `AccessDenied`, or `Unavailable` for any other reason, such as a kernel process with no image file. The metrics come from the list whatever the status. `isNeverOpenedProcess()` is true for `lsass.exe` and `csrss.exe`, by image name ignoring ASCII case, and for a process without a name other than PID 0. `ProcessCollector` wraps every details reader so that it is never called for them, the Windows reader checks again before `OpenProcess` ([DC-75](#dc-75)), and the terminator refuses them ([DC-87](#dc-87)). A process without a name has an explanation of its own.

**Rationale:** `NtQuerySystemInformation` returns every process's name and metrics without opening it ([DC-01](#dc-01)), so only the details can be missing. Omitting such a process would hide it, and an error list beside them would make the UI merge two collections. In-band, the UI shows a partial row with an indicator ([DC-85](#dc-85)), as Task Manager does. An enum tells a process the monitor never opens from one that refused access, and a `NotQueried` default never claims a query that did not happen. Opening `lsass.exe` or `csrss.exe`, even with limited rights, trips credential-theft heuristics in most EDR products ([architecture.md](architecture.md#antivirus-and-edr-considerations)). Filtering in the collector makes the rule hold for any reader, and matching by name never risks opening the real process. The parser leaves a name empty when it does not lie inside the buffer ([DC-71](#dc-71)), and such a process could be either, so refusing it fails closed at the cost of its details and End process. The Idle process has no name but cannot be either, because `OpenProcess` rejects PID 0.

**Consequences:** The check trusts the name it is given, so `terminateOwnedProcess()` and the details readers require the name from the process list.

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

**Decision:** `platform/windows/system_process_information.h` defines `FullSystemProcessInformation`, the complete entry layout with the field names Windows uses, and `static_assert`s tie its size and offsets to the partial `SYSTEM_PROCESS_INFORMATION` in `winternl.h`. `parseSystemProcessInformation()` follows `NextEntryOffset`, copies each entry out with `memcpy`, and returns `std::nullopt` if an entry does not fit in the buffer. An image name is read by its `Length`, and only when it lies inside the buffer; otherwise it is empty. The private working set is `WorkingSetPrivateSize`, the commit charge `PrivatePageCount`, and the I/O counters `ReadTransferCount` and `WriteTransferCount`, which include pipes and devices. A negative signed counter reads as zero. `wideToUtf8()` converts the names in portable C++, which the network collector shares.

**Rationale:** `winternl.h` hides the creation time, the CPU times, the parent PID and the I/O counters in `Reserved` fields, and the collector needs all of them. A complete definition names them, and the `static_assert`s catch a layout mistake at compile time instead of as wrong numbers at run time. Copying each entry avoids reading through a cast pointer and any dependence on the buffer's alignment, and the bounds checks stop a corrupt list from reading past it. The parser's tests build lists from SDK types, so they are in `tests/platform/`, which may not call Windows APIs, and a conversion in plain C++ keeps them free of OS calls. The transfer counts are the only I/O byte counters in the list, so the domain's comments say what they include rather than calling them disk I/O.

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

**Decision:** The production details reader, from `makeWindowsProcessDetailsReader()`, refuses the processes the monitor never opens ([DC-13](#dc-13)), opens each new process with `PROCESS_QUERY_LIMITED_INFORMATION` only, and checks that the creation time from `GetProcessTimes` is the identity's. On a mismatch the details are `Unavailable`. It reads the image path with `QueryFullProcessImageNameW` and the command line with `NtQueryInformationProcess(ProcessCommandLineInformation)`, resolved from `ntdll.dll` once. An access-denied error maps to `AccessDenied` and any other failure to `Unavailable`, and failures are logged at `debug`.

**Rationale:** `PROCESS_QUERY_LIMITED_INFORMATION` is the only access the security rules allow, and both queries need nothing more. `ProcessCommandLineInformation` reads the command line without `PROCESS_VM_READ`, which reading the PEB would need and the rules forbid. Between reading the list and opening the process, it can exit and its PID be reused. Without the creation-time check, the new process's details would be cached under the old identity. An unelevated monitor is refused by about a third of the processes on a desktop at every start, so a warning for each would add about 150 warnings, each with a log flush; the UI shows each status, and `debug` keeps the detail for diagnosis. On the development machine, unelevated with 413 processes, the first refresh, which opens every process, took about 18 ms, and later ones 3.5–6 ms, most of it in `NtQuerySystemInformation` itself, so enumeration belongs on the slow-collector thread ([DC-77](#dc-77)).

**History:** Phase 4, process collector.

---

<a id="dc-76"></a>
### DC-76: Per-process CPU usage and I/O rates computed in the collector, CPU as a share of all logical processors

**Decision:** `ProcessCollector` sets each process's `cpuUsagePercent` and its I/O read and write rates with the pure `calculateProcessRates()`. The CPU usage is the change in user and kernel time since the process's baseline, divided by the elapsed time times the number of active logical processors in all groups, clamped to 0–100. The I/O rates are the changes in the transfer counts over the same time. Baselines are keyed by `ProcessIdentity` and replaced on every successful read, and a failed read keeps them. A rate is `std::nullopt` on a process's first sample, when no time has elapsed, and when its counter went backwards. The timestamp is taken right after the list is read, before new processes' details are.

**Rationale:** The collector owns the previous sample, so a rate computed there is the same for every consumer. The UI could not compute it anyway: the list is read every 2 seconds but reaches the UI every second, so its rates would alternate between zero and double ([DC-79](#dc-79)). Dividing by every logical processor puts each process on the scale of the total CPU usage, as Task Manager does, and a single-threaded process on 16 processors peaks at 6.25%. Keying by identity keeps a new process from inheriting an exited one's baseline. Process CPU times cover every processor group, unlike `GetSystemTimes` ([DC-29](#dc-29)). Taking the timestamp before the details keeps the first details pass out of the interval. Windows credits CPU time per clock tick, about 15.6 ms, so a busy process can briefly exceed its share, hence the clamp.

**Consequences:** The column adds up to slightly less than the total CPU usage, because interrupt time is charged to no process and a process that exited during the interval is no longer listed. In the Phase 4 code review it added up to 98.7%.

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

**Decision:** `GetIfTable2` gives each interface's counters, link speed and status on every tick ([DC-34](#dc-34)), and `GetAdaptersAddresses` its names, addresses and DNS servers, matched to it by LUID. Loopback and NDIS filter rows are dropped. The LUID, as `NetworkSample::interfaceId`, keys each adapter's baselines, history, chart and card. `isHardwareInterface` comes from the interface's flags, and `hasIpInterface` is set when `GetAdaptersAddresses` lists the adapter. `AdapterDetailsCache` keeps those details, re-read after an address or interface change notification, a new interface, or 30 seconds. `calculateNetworkSamples()` keeps Preferred addresses, and Deprecated ones only while the adapter is Up. `isPlaceholderDnsServer()` drops site-local placeholder DNS servers. Each address is marked `isTemporary`, from a random suffix origin, or `isLinkLocal`, and a link-local IPv6 address carries its zone index.

**Rationale:** `GetIfTable2` has no addresses or DNS servers, and the LUID pairs the two APIs exactly. Filter rows carry their adapter's counters, so a total over Up rows counted its traffic several times ([code review F-9](code_reviews/phase_2.md#f-9-network-throughput-total-counts-the-same-traffic-several-times)). Virtual adapters stay but are not hardware, so totals leave them out ([DC-46](#dc-46)). Names repeat, and keyed by name, two adapters shared one history. Nothing in `MIB_IF_ROW2` tells the WAN Miniports, which are always Up, from a VPN tunnel, but IP binding does. `GetAdaptersAddresses` took about 2 ms a call, over the fast-collector budget, for data that changes only with the configuration ([code review F-10](code_reviews/phase_2.md#f-10-getadaptersaddresses-runs-on-every-scheduler-tick)); no notification covers DNS servers, hence the maximum age. A disconnected Wi-Fi adapter kept its last lease and seemed to be on the LAN, while a Deprecated address still serves a link that is Up. The status is read every tick while the addresses are cached, so the pure calculation applies the rule. No network configures the placeholders, which come from an expired draft. Two adapters can have the same fe80:: address, which is unusable without its zone.

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

**Decision:** `ConnectivityCollector` seeds a mutex-protected cache with `GetNetworkConnectivityHint` in its constructor, then registers `NotifyNetworkConnectivityHintChange`, through an injected subscriber whose RAII subscription is the collector's last member. The callback only stores the value, and `collect()` returns the cache, logs each change once at `info`, and polls instead if registration failed. It runs on the slow-collector thread, which a notified change wakes ([DC-77](#dc-77)). The level defaults to `ConnectivityLevel::Unknown`, and metered and roaming are `std::optional<bool>`. All three are unknown before the first read and after a failed one, and roaming whenever the cost is. The Network tab's Connectivity section has a readout each for the level, the cost, roaming and the connected networks ([DC-100](#dc-100)).

**Rationale:** Connectivity changes rarely, so notifications avoid a call on every refresh. Seeding before registering makes the first `collect()` accurate, and a newer callback value can never be overwritten by the seed. `CancelMibChangeNotify2` waits for callbacks in flight, so destroying the subscription first keeps the callback away from a destroyed cache. The injected subscriber lets unit tests deliver notifications ([known deviation D-5](known_deviations.md#d-5-notification-callback-wiring-has-no-unit-test)). Writing the log is blocking I/O, which the callback, on an OS thread-pool thread, must not do ([code review F-14](code_reviews/phase_2.md#f-14-connectivity-callback-writes-to-the-log-on-the-os-thread)), so a change that reverts before the next `collect()` goes unlogged. With only `None`, "could not determine" read as offline, and a kept previous value would show stale data as current ([DC-12](#dc-12)). An enum value keeps one value type through the snapshot and the UI. The hint's roaming flag reads false whenever the cost is unknown, though Windows treats roaming as a cost flag. A readout per field shows each unknown without a sentence that combines three states.

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

**Decision:** `NetworkProfileCollector` reports each connected network's name, its category and the interface IDs of its adapters, read from `INetworkListManager`, with each connection's adapter GUID converted to the LUID ([DC-35](#dc-35)). A `std::jthread` that the collector owns makes every query, in a multithreaded apartment that it keeps for its lifetime. It reads when it starts, when a connectivity notification has marked the names stale, and when they are a minute old, but never within a second of its last read. It stores each result and, if it changed, wakes the scheduler ([DC-77](#dc-77)); `collect()` only copies the latest. COM pointers are `std::unique_ptr`s with releasing deleters. The adapter cards and the connectivity section show them ([DC-39](#dc-39)).

**Rationale:** A name is most useful beside its adapter, and the LUID is the adapter's identity everywhere else. The query is a COM call into the Network List Service, which [DC-06](#dc-06) keeps off the tick, and nothing bounds how long it waits: on the shared slow-collector thread, a stuck service would stop the process list and connectivity too. Renaming a network or changing its category changes no connectivity and has no C-style notification, so the minute picks it up, and a failure is retried only at the same points. In the code review a burst of notifications made 130 reads in 2 seconds; a second between reads bounds that. A seed in the constructor would run COM on the UI thread, whose apartment Qt has made single-threaded, so the first read takes its place. Owning the apartment keeps `CoInitializeEx` and `CoUninitialize` on one thread. The header of WRL's `ComPtr`, `<wrl/client.h>`, has a path that the include order does not cover. The category decides which firewall profile applies.

**Consequences:** `CoCancelCall` cannot interrupt the in-process query, so if the service hangs, closing the window waits for the read in progress.

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

**Decision:** `SparklineWidget` draws every chart, from the dashboard's bare sparklines to labelled charts with legends. The caller owns the history and passes a view of it each tick, and a missing sample breaks the line. A chart's series share one Y range. Only the primary series is filled, and only its minimum and maximum are marked, under its name when the chart has a legend. The grid, axis labels and annotations are off by default. The grid moves with a sample index from the caller, the axis gutter fits the widest of the caller's label templates, and annotations can have their own formatter. Series take five fixed hues, or another palette slot's, and past the fifth a shared grey. A chart with more samples than pixel columns draws each column's lowest and highest point with a one-pixel cosmetic pen.

**Rationale:** A caller-owned view serves any length of history; gaps follow [DC-12](#dc-12). Series of different units belong on separate charts, and one fill keeps fills from turning muddy. Beside a legend, unnamed extremes did not say whose they were. A full history keeps its length, so only the caller knows that time has moved on. A gutter sized from the current labels moved as traffic came and went, so stacked charts did not line up. An annotation of 8% above a readout of 7.5% looked like a disagreement. The palette, checked for separation under colour-vision deficiency, leaves out red, amber and green, the health status colours. Generated hues past the fifth would look alike, and a slot keeps a volume's colour when another appears. At 30 minutes the CPU page took about 210 ms per tick, mostly in Qt's stroker for pens wider than a pixel. Min/max decimation bounds the work by the chart's width and, unlike averaging, keeps one-sample spikes.

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

**Decision:** The dashboard is a "System status" line and five `ResourceCard`s, CPU, memory, disk, network and uptime, in a three-column grid. Each card has a title, a large value, a detail line, a health status, and a fixed-height slot for its one-minute sparkline ([DC-48](#dc-48)), which stays empty on the uptime card. The status is a `TonedLabel`, with a light and a dark shade of each tone ([coding guidelines](coding_guidelines.md#text-colours)). All text comes from pure functions in `ui/dashboard_formatting.h`, with sizes in binary units. Disk shows the fullest volume. Network shows the total throughput of the hardware adapters that are Up, or "N/A" while any of them has no rate yet, with connectivity as its detail line. String literals are ASCII.

**Rationale:** One card type keeps the five resources consistent, and a chart slot of the same height in every card keeps the uptime card in line. Pure formatting keeps `DashboardView` thin and its edge cases testable without widgets. Health comes precomputed in the snapshot ([DC-45](#dc-45)), so the UI only renders it. No single colour reaches WCAG's 4.5:1 contrast on both a light and a dark background, hence a shade for each. Binary units match how Windows reports sizes. A virtual adapter's traffic also crosses a hardware adapter, so counting it would count that traffic twice ([DC-35](#dc-35)), and a sum with a rate missing would understate it. Targets that link Qt compile with `/utf-8`, but `domain` and `platform/windows` do not, so a non-ASCII literal would be read in the system code page there.

**History:** Phase 2, dashboard view; code review F-9 (only hardware adapters in the network total); code review F-19 (why literals are ASCII); code review F-37 (status colours that reach 4.5:1 in both colour schemes).

---

## Performance tab

The resource pages, their histories and their readouts.

<a id="dc-53"></a>
### DC-53: Performance view: a sidebar of resource pages that each own their history

**Decision:** `PerformanceView` is a sidebar of resource pages, CPU, memory, disk and network, each entry with a summary of the latest values, beside a `QStackedWidget` that shows one page. Each page derives from `PerformancePage` and owns the `RingBuffer` histories of its charts. The CPU page has a total chart above a grid of small per-core charts, rebuilt with a new history when the core count changes. The memory page charts physical memory and the commit charge, the disk page one series per volume, and the network page a chart per adapter that is Up and can carry traffic ([DC-35](#dc-35)). Volumes are keyed by name and adapters by `interfaceId`. A key missing from a snapshot records a gap, and is dropped once its history holds no samples. Every adapter's history is recorded, down or not.

**Rationale:** Task Manager's layout, a resource list with one page at a time, gives each chart room, and lets the cores use small multiples instead of 16 or more series that would run out of distinct colours after five ([DC-48](#dc-48)). The histories stay in the UI, as the dashboard's do, because the UI may not depend on `monitoring`, and keeping each beside the page that draws it keeps `SystemSnapshot` a per-tick value. After a change in the core count, the indices no longer line up. Keying by name or ID rather than position keeps a line continuous when other volumes or adapters come and go, and an adapter that comes up shows its history.

**History:** Phase 3, Performance tab; code review F-20 (the per-core grid scrolls, for [DC-58](#dc-58)); code review F-21 (only adapters that can carry traffic, for [DC-35](#dc-35)); code review F-23 (adapters keyed by interface ID, for [DC-35](#dc-35)).

---

<a id="dc-57"></a>
### DC-57: Performance readouts: pure builders with stable names, shown in a `ReadoutGrid`

**Decision:** Each Performance page shows readouts beside its charts, in a `ReadoutGrid` with a dimmed name above each value: on the memory page, for example, used, available, total, committed, system cache and the pools ([DC-63](#dc-63)). Pure functions in `ui/performance_readouts.h` give the text. They return the same names in the same order whether or not the data is available, with "N/A" for a missing value, including a link speed of 0. `ReadoutGrid::setReadouts()` updates the values in place, and rebuilds its cells only when the names change. Counts have comma thousands separators, and link speeds are in decimal bits per second, rounded to three significant digits before `formatLinkSpeed()` chooses the unit.

**Rationale:** Fixed names keep the layout still when a value is missing, and show the missing value rather than quietly removing it. Pure builders keep the formatting and its edge cases testable without widgets, as `dashboard_formatting.h` does. Updating labels in place avoids rebuilding widgets every second. A fixed separator rather than `QLocale` keeps tests independent of the machine's locale, and localized numbers can come with the Phase 6 display settings. Network rates are quoted in decimal units by convention, while byte quantities keep the binary units used elsewhere. `QString::number(…, 'g', 3)` switches to an exponent once rounding reaches 1000, which showed "1e+03 Mbps"; choosing the unit after rounding avoids that at every boundary.

**History:** Phase 3, Performance tab; code review F-22 ("Used" and "System cache", for [DC-63](#dc-63)); code review F-26 (link speeds rounded before the unit is chosen); maintenance (that rule moved here from DC-67).

---

<a id="dc-58"></a>
### DC-58: Performance pages fit an 800×600 window: re-flowing readouts and a scrolling per-core grid (code review F-20)

**Decision:** `ReadoutGrid` lays out its readouts with its own `QLayout`, which uses as many columns as fit the width, up to the grid's maximum, and reports a height for width. Each cell remembers the widest it has been since the names last changed. The network page, and the CPU page's per-core grid, scroll vertically only; per-core charts are at least 12×12 pixels, so the 16 columns of a 256-core grid fit the page. `MainWindow` opens at 1024×720, within 90% of the screen's available area, and the content must still fit 800×600.

**Rationale:** A `QLabel` without word wrap cannot be narrower than its text, and a fixed five-column grid of IPv6 addresses and adapter descriptions was 875 pixels wide on a 535-pixel page. Re-flowing in a layout, not in `resizeEvent()`, lets Qt's height-for-width handling size the scroll area's content and the pages' layouts, and moves the labels without recreating them. Wrapping or eliding values was rejected, because addresses must stay whole to be read and copied, and full-width rows for long values would be a special case in a generic grid. Without the remembered widths, a transfer rate near a column boundary would change the column count every second. The per-core grid scrolls rather than shrink, because a chart a few pixels tall shows nothing. The larger default size gives the network readouts more columns, and the limit keeps the window on a 1366×768 screen.

**Consequences:** A scroll area that does not scroll horizontally squeezes content that is too wide and shows no scroll range, so the widget tests compare the content's minimum width with the viewport.

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

**Decision:** The memory page's readout of `PERFORMANCE_INFORMATION::SystemCache` is named "System cache", not "Cached", and the comment on `MemorySample::systemCacheBytes` says what it is without claiming that it matches Task Manager. The readout of total minus available, which includes the modified page list, is named "Used", not "In use".

**Rationale:** Task Manager's "Cached" is the standby list plus the modified list, and its "In use" leaves the modified list out. `SystemCache` is neither, nor what Microsoft documents, the standby list plus the system working set. On the development machine it differed from Task Manager's figure by 3.8 GiB in one reading and by 0.4 GiB the other way in the next, so no correction can be applied. The exact list sizes come from `NtQuerySystemInformation(SystemMemoryListInformation)`, which needs a privilege that unelevated users lack, or from performance counters through PDH, which core metrics do not use ([DC-02](#dc-02)). The size of the system cache is still useful, so the readout stays, under a name that promises no more than it reports. "Used" is the plain description of total minus available.

**Consequences:** An elevated component, if one is added for [elevated process termination](../roadmap.md#elevated-process-termination), could read `SystemMemoryListInformation` and give Task Manager's breakdown.

**History:** Code review F-22.

---

## Processes tab

The process table: its model, columns, sorting, tree, highlights and access indicator.

<a id="dc-79"></a>
### DC-79: Process table model: one row per `ProcessIdentity`, its columns, and updates that signal only the cells that changed

**Decision:** The Processes tab shows a `QTreeView` over `ProcessModel`, a `QAbstractItemModel` with one row per `ProcessIdentity`. `updateProcesses()` updates kept rows in place, inserts new ones, removes exited ones ([DC-82](#dc-82)), and never resets the model. It applies every list, shown or not ([DC-54](#dc-54)). A snapshot without a list keeps the last rows, and the summary line says so. Memory is the private working set, with the working set and the commit size in its tooltip. I/O is read plus write bytes per second, computed in the collector ([DC-76](#dc-76)). Network reads "N/A" until ETW collects it. A refresh compares raw values and signals only the cells that changed, one `dataChanged()` per parent naming its roles, in a batch that the proxy sorts once ([DC-81](#dc-81)).

**Rationale:** The tree needs parent and child rows in one model. Keyed by identity, a reused PID gets a new row, and changing rows in place keeps the selection, the scroll position and persistent indexes. Treating a failed read as empty would remove every row and add them back. The list is read every two seconds but reaches the UI every second, so a rate computed in the UI would alternate between zero and double; the collector knows when each list was read. The I/O counters include pipes and devices, so "Disk I/O" would mislabel them. A working set counts each shared page again for every process that maps it: in the code review the working sets added up to 33.2 GiB with 30.5 GiB in use. Private working sets add up and match Task Manager; the commit size counts paged-out memory too. Value columns have fixed widths, because `ResizeToContents` measures every row on each change. With recursive filtering, Qt re-filters every ancestor of a block whose signal names no roles, so a repeated list cost milliseconds.

**History:** Phase 4, process explorer view; Phase 4, process explorer view (exited rows kept for three seconds, for [DC-82](#dc-82)); code review F-29 (Memory shows the private working set); code review F-33 (re-measured, for [DC-95](#dc-95)); code review F-37 (N/A in the dimmed text colour); maintenance (DC-80, DC-83 and DC-91 merged in, DC-83's single sort per refresh into [DC-81](#dc-81)); maintenance (the benchmark moved to [DC-66](#dc-66), and its figures to performance.md).

---

<a id="dc-80"></a>
### DC-80: Merged into DC-79

The process table's columns are described in [DC-79](#dc-79).

---

<a id="dc-81"></a>
### DC-81: Sorting and filtering: a proxy that compares domain values, with missing values lowest, and sorts once per refresh

**Decision:** `ProcessSortFilterProxyModel` sits between `ProcessModel` and the view. Its `lessThan()` compares the `ProcessInfo` values behind the two indexes, so through `std::optional`'s ordering a missing value sorts below every value. Equal rows fall back to the name, compared on its UTF-8 bytes with ASCII case folded, then the PID, then the creation time. The filter box keeps a row whose name or PID contains its text, ignoring case. The first click on Name, PID or Network sorts ascending, and on a measured value largest first, through `Qt::InitialSortOrderRole`. The table starts sorted by name, with dynamic sorting on. When a refresh's value signals, which the model announces as a batch ([DC-79](#dc-79)), cover the sorted column under more than one parent, the proxy turns dynamic sorting off for the batch, so every level sorts once.

**Rationale:** Display text sorts "9.0%" after "10.0%". Numeric `QVariant`s would fix that, but Qt never orders an invalid `QVariant` before a valid one, so "N/A" rows would lead a largest-first sort. Domain values are type-checked and create no `QVariant`s. Most processes show 0.0% CPU at any moment, and without a tie-breaker equal rows could swap on a refresh. Folding ASCII case on the raw bytes compares names without allocating, and image names are almost always ASCII. A fixed-string filter keeps the dot in "a.exe" from being a wildcard. Network has no values yet, so the names decide, and a descending first click would list them Z to A. Each re-sort that moves a row makes the proxy remap every persistent index, one per expanded row, so re-sorting once per parent was quadratic: a CPU-sorted tree of 2,000 processes cost 24.7 ms. New rows still arrive with dynamic sorting on, so each is placed by binary search instead of a full sort.

**History:** Phase 4, process explorer view; code review F-34 (the first click on Network sorts ascending); maintenance (the single sort per refresh merged in from DC-83).

---

<a id="dc-82"></a>
### DC-82: Process tree and highlights: parents matched by PID, creation time and listing time, and new and exited rows marked for three seconds

**Decision:** "Show as tree" switches `ProcessModel` between a flat list and a tree. A process's parent is the latest listed process with its parent PID that was created no later than itself and was not marked exited before the process was first listed. A link is made when the row is inserted and never changes, and when an exited parent's row is removed, its children move to the top level. Showing the tree or changing the filter expands every branch, and a first child expands its parent. A process that appears after the first list is green for three seconds. One that leaves the list keeps its row and its last values, red and italic, as long; a tooltip says which. Highlights start and end on snapshot timestamps, with no timers.

**Rationale:** Windows records only the parent's PID, which a younger process may hold by then, and a parent cannot be younger than its child; "no later than" allows for the same clock tick. A process missing from an earlier list had exited before a new child was created, though its row is still shown, and in the code review Windows reused a freed PID within half a second. A running process's parent PID never changes, and the process it names cannot appear later, so links are fixed. Moving orphans, as Process Explorer does, keeps their selection and expansion. `QTreeView` cannot tell a branch the user collapsed from one never expanded, so only a first child expands it. Snapshot times keep the model deterministic in tests, and a snapshot arrives every second even without a new list. Three seconds spans at least one two-second refresh. The italics and the tooltip serve users who cannot tell red from green. Keeping an exited row shows a launcher with the child it started.

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

**Decision:** The name column's decoration is `detailsStatusIcon()`: an amber padlock when Windows denied access, a blue shield for a process the monitor never opens ([DC-13](#dc-13)), a grey barred circle when there was nothing to read, and a transparent icon of the same size when the details were read. `ProcessModel` paints the four with `QPainter` at 1x, 1.5x and 2x once, and keeps them as members. The name cell's tooltip, `processDetailsToolTip()`, is rich text: the path and command line, or why they are missing and whichever was read, escaped and cut off after 2,000 characters. The name header's tooltip explains the icons.

**Rationale:** The three reasons mean different things ([DC-13](#dc-13)). A denial depends on the monitor's rights and is routine when it runs unelevated, Never opened is the monitor's own rule, and Unavailable means there is nothing to read. Different shapes and colours tell them apart without relying on colour alone, and the tooltip gives the reason in words. The icons are painted because the project has no resource files and `QStyle`'s standard icons vary by style and have no padlock. Painting each scale keeps them sharp on high-DPI screens. A function-local static pixmap would outlive the `QGuiApplication`, which it must not. The default delegate leaves room for a decoration only in rows that have one, so the transparent icon keeps every name in the same place. Qt wraps a rich-text tooltip, but plain text only when it is wider than the screen, and command lines can run to thousands of characters.

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

**Decision:** `platform::terminateOwnedProcess()` takes a process's `(PID, creation time)` identity and returns a `domain::ProcessTerminationResult`. It first refuses a process the monitor never opens ([DC-13](#dc-13)). It opens the process with `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE`, checks its creation time and that it has not exited, and reads its token once. It refuses a process whose token's user is not the monitor's, and one that runs elevated: an elevated token, or high integrity or above. Only then, with the first handle still open, does it open the process again with `PROCESS_TERMINATE`, check the identity again, and call `TerminateProcess` with exit code 1, without waiting. The monitor ends only unelevated processes, even when it runs elevated. Each result is logged: `info` for a process ended or already gone, `warn` for a refusal or a denial, and `error` otherwise. The integration tests end only children they start with a filtered, medium-integrity copy of their own token.

**Rationale:** An open handle keeps Windows from reusing its PID, so the second handle reaches the process whose owner was checked. Checking the owner and the elevation first, the monitor never asks for rights it is not meant to use. The MVP ends only unelevated processes ([architecture.md](architecture.md#unelevated-process-termination)), and refusing elevated ones up front lets the message give the reason, not a denial. `TokenElevation` matches Task Manager's Elevated column but misses UIAccess programs and service helpers at high integrity, which an unelevated caller cannot end either. `TerminateProcess` reports a process that has already exited as denied, and a zero-timeout wait, which `SYNCHRONIZE` allows, tells the two apart. On CI every process the tests start is elevated, so without the filtered token the termination path would go untested.

**Consequences:** With UAC off, every process an administrator starts is elevated, so the monitor ends none. The roadmap has an item for [elevated process termination](../roadmap.md#elevated-process-termination).

**History:** Phase 4, process termination; Phase 4, process termination (elevated processes refused); code review F-32 (a process without an image name refused, for [DC-13](#dc-13)); maintenance (DC-90 merged in).

---

<a id="dc-88"></a>
### DC-88: End process: one action, a confirmation with Cancel as default, a message for each outcome but success, and an offer to close the monitor on its own row

**Decision:** End process is one `QAction`: a toolbar button, a context-menu item, and the Delete key on the table. It is enabled while `main.cpp` has set a terminator and the current row has not exited, rechecked after every snapshot. It opens a window-modal confirmation that names the process and its PID as plain text, with Cancel as the default and escape button. The dialog acts on a copy of the process taken when it opened, and is answered on `finished`, after it has closed. The terminator is a `std::function` that runs on the UI thread ([DC-87](#dc-87)). Success shows nothing. Any other result shows a message from `processTerminationMessage()`, with an icon at the level the terminator logs it. On the monitor's own row, known by an identity from `main.cpp`, End process offers to close the monitor instead.

**Rationale:** One action keeps the button, the menu item and the key in step, in the places Task Manager users look. With Cancel as the default, Enter or a stray key never ends a process. The name is plain text because the process chose it. A message opened on `buttonClicked` would stack over the confirmation, which is still shown then. The table refreshes while the dialog is open and the current row can move, so reading the selection on confirming could end a process the dialog never named. The UI may not name platform functions ([DC-20](#dc-20)), and `terminateOwnedProcess()` never waits for the process to exit, so a worker thread gains nothing. An ended process's row is marked as exited within one refresh, which shows it worked, while a failure is rare and needs reading. Ending the monitor's own process skipped `main()`'s shutdown and lost log lines, while closing its window shuts it down in order. A PID alone would break the process identity rule.

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

**Decision:** The Network tab has an `AdapterCard` per adapter, kept when the list changes. By default it lists every adapter that is hardware or bound to IP, whatever its status, unless its driver has `NCF_HIDDEN`, which SetupAPI reads when an interface appears. "Show all adapters" adds the rest after them. `isListedBefore()` orders both tabs' adapters: those listed by default first, then hardware, then by name in natural, locale-independent order, then by interface ID, never by status. An adapter's type comes from its physical medium when that names one, and otherwise from its interface type. Unless the adapter is Up, its link speed, addresses and DNS servers read "Not connected", and its chart collapses, though its history is still recorded. Each address is marked temporary or link-local, and a virtual adapter's link speed has a tooltip that says its driver chooses it.

**Rationale:** As an inventory, the tab lists adapters that are down, but the adapters left after [DC-35](#dc-35)'s filters include WAN Miniports and tunnel pseudo-interfaces. `NCF_HIDDEN` is what Network Connections goes by; hiding virtual adapters that are not Up would also hide a Bluetooth PAN adapter and a disconnected VPN. Sorted by status, an adapter coming up moved every card under the scroll position. Natural order puts "… 2" before "… 10", and the interface ID keeps cards with the same name from swapping. A Bluetooth PAN adapter has the Ethernet interface type; only its medium tells them apart. Windows keeps reporting a link speed and DNS servers for a link that is down, where a chart could only be flat. ipconfig names three kinds of IPv6 address, which one readout holds, so each line carries its mark. A VPN reports a nominal speed but Wi-Fi Direct its real link's, so the tooltip qualifies the figure instead of hiding it.

**History:** Phase 5, network view; code review F-36 (addresses and DNS servers only while Up); code review F-39 (the adapters Windows hides, the fixed order, and collapsed charts); code review F-44 (address marks and the link speed's tooltip); maintenance (DC-97 and DC-106 merged in, and the tab's parts of DC-103 and DC-108).

---

<a id="dc-97"></a>
### DC-97: Merged into DC-96

The adapter type and the link speed are described in [DC-96](#dc-96).

---

<a id="dc-98"></a>
### DC-98: Adapter throughput charts: one minute per card, with the history shared with the Performance tab

**Decision:** Each `AdapterCard` has a receive and send chart, set up like the Performance tab's network charts. `NetworkView` records every adapter's throughput from every snapshot in an `AdapterThroughputHistory` of 60 samples, one minute at 1 Hz as on the dashboard, and copies each listed adapter's history into its card when it refreshes. The Performance tab's network page records its 1,800 samples with the same class. A rate that cannot be computed, or an adapter missing from the snapshot, records a gap, and an adapter is dropped once it is missing and its history holds no values. `build.ps1 -Benchmark` times the tab with 2 to 20 adapters ([DC-66](#dc-66)).

**Rationale:** The tab is an inventory, and its charts show what each adapter is doing now; the Performance tab already offers 5- and 30-minute windows for the active adapters. One minute keeps each card's chart sparse and matches the dashboard. Every adapter's history is recorded, listed or not, so an adapter that comes up, or that "Show all adapters" adds, already has its last minute, and a hidden tab loses nothing ([DC-54](#dc-54)). One class keeps the two tabs in agreement on gaps and on when an adapter's history ends, where a copy would have had to follow every change. Only the cards in view are painted, and a collapsed chart ([DC-96](#dc-96)) is not updated, so 20 adapters cost about what 8 do ([performance.md](performance.md)).

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

**Decision:** Two executables in `benchmarks/` time the UI thread's work for one tick: `system_monitor_benchmark` each Performance page at each chart window and the Network tab with 2 to 20 adapters, and `system_monitor_process_benchmark` the Processes tab applying 400 to 2,000 processes, flat and as a tree, hidden and shown. They are built with every preset but not registered with CTest. `build.ps1 -Benchmark` builds Release and runs both, which print the mean and maximum per tick as Markdown tables after the environment they ran in ([DC-95](#dc-95)). The data is synthetic, fixed and a worst case, and [performance.md](performance.md) records the figures.

**Rationale:** [DC-61](#dc-61)'s first figures came from a benchmark that was not committed, so they could not be reproduced, and the review's rebuilt one differed from them by up to 2x for reasons nobody could show. That benchmark had already caught a 210 ms tick, so later changes to charts and pages need the same tool. Timings depend on the machine and would make a flaky test, so the benchmarks stay outside CTest, and building them with every preset keeps them compiling in CI. Fixed data makes two runs comparable, and a worst case bounds what real data costs. Printing through spdlog follows the logging rule.

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

**Decision:** Integration tests run against the live Windows APIs and assert what holds on every machine, never fixed values. The CPU tests check, over several real samples, that usage stays within 0–100% and is never NaN, that the core count matches `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`, and that usage rises under a multi-threaded synthetic load. The process tests start a suspended `cmd.exe` child with a command line unique to the run, and check that it is listed with its exact identity, parent, path and command line, and that it leaves the list within 5 seconds of being ended. A counting wrapper around the real details reader shows that each identity's details are read once. Every process's `detailsStatus` must match the details it has, and when the tests run unelevated, at least one process must deny access and still have its metrics. One test runs every collector the app registers ([DC-110](#dc-110)).

**Rationale:** On a real machine, background services, indexing and power states make utilization fluctuate, so fixed numbers make flaky tests, while invariants and a rise under a known load show that the collector works. A child the test starts is the one process whose identity, parent, path and command line are known in advance. Suspended, it never runs, needs no window and cannot exit on its own, and it shows that the command line is readable before the process starts. PID reuse cannot be forced on a real machine, so the unit tests cover it ([DC-73](#dc-73)). Which processes deny access depends on the account and elevation, so denials are counted only where they are certain, and elevated runs, as on CI, skip that check.

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

**Decision:** Each entry heading has `<a id="dc-NN"></a>` on the line above it, and every link to an entry names that anchor. `Test-DesignDecisionLinks`, which `build.ps1 -Format` and `-CheckFormat` run, reads every tracked or new text file. It fails on a heading without its anchor, a link into this file that leads nowhere or names an entry heading's slug, a link whose text is one `DC-NN` and whose target another, and a cited `DC-NN` that is not an entry. Only the contents may link to a stub ([DC-115](#dc-115)), and no other file may cite one, except in `docs/code_reviews/`.

**Rationale:** Links named GitHub's slug of the whole heading, so retitling or merging an entry broke every link to it, and nothing noticed. The ID never changes. On a line of its own, the anchor leaves the heading as it was, and `^### DC-` still finds it; in CommonMark a line holding only an inline tag is a paragraph, which the heading ends. Refusing an entry heading's slug makes a link written the old way fail at once rather than at the next retitle, and section slugs pass because the contents use them. The check is PowerShell in the script that CI's Format Check job already runs, so it needs nothing installed, and `-Format` shows a broken link before CI does. A link to a stub resolves but leads to a pointer instead of the decision. The code reviews keep theirs, because they record what each review led to at the time.

**History:** Maintenance, design decisions; maintenance (no link or citation may name a stub, for [DC-115](#dc-115)).

---

<a id="dc-115"></a>
### DC-115: Living entries: an entry states the current decision, is rewritten when it changes, and ends with a one-line history

**Decision:** An entry states the decision as the code now has it. A change that answers its question differently rewrites its Decision and Rationale under the same ID, with no note such as "Superseded in part by" and no new entry that patches it. A new entry is for a new question. The entry's last line, **History:**, names the phase, finding or maintenance item that made the decision and each one that changed it, with what it changed. Git's `markdown` diff driver, set in `.gitattributes`, lets `git log -L ':DC-76\::docs/design_decisions.md'` list an entry's versions. An ID is never reused, renumbered or deleted. A merged or retired entry becomes a one-line stub that says where the decision is now, and links and citations move there, except in `docs/code_reviews/` ([DC-114](#dc-114)). Entries that patch one another merge into the lowest ID among them under each topic ([DC-116](#dc-116)).

**Rationale:** In the append-only log, 40 notes patched 31 entries. Learning how End process worked meant reading DC-88, its notes and three more entries, and a reader who stopped at the first followed a decision the code no longer made. A living entry is right when read alone, and git keeps the earlier wording beside the commit that explains each change. Immutable records, as in Michael Nygard's architecture decision records, suit decisions that an audit traces, but most changes here refine a decision, and a new record would repeat most of the old one. Without the `markdown` driver, Git finds no heading in a Markdown file, and `-L` fails. The lowest ID is the oldest, so the oldest links lead to the whole decision, and choosing it needs no judgement. One entry for a chain across topics would take a decision away from the code it decides.

**History:** Maintenance, design decisions; maintenance (a known deviation as a source, and every entry given its history line, for [DC-116](#dc-116)); maintenance (how chains are merged, and stubs neither linked nor cited); maintenance (the notes in DC-62, DC-66 and DC-98, for [DC-117](#dc-117)); maintenance (a retired entry's title, for [DC-117](#dc-117)).

---

<a id="dc-116"></a>
### DC-116: Entries grouped by topic, in ID order within each topic

**Decision:** The entries are grouped under `##` headings by what they decide, not by the phase that made them: Windows APIs, modules and dependencies, the domain model, the scheduler and collectors, each collector area, charts, the main window and shared UI, each tab and process termination, build and tooling, testing, and this log. A line under each heading says what the topic covers. Within a topic the entries are in ID order, so a new entry goes at the end of its topic. An entry that spans topics goes under the one whose code it mostly decides, and the contents list each entry once. The phase that made an entry, and each change since, are in its **History:** line ([DC-115](#dc-115)).

**Rationale:** A reader comes to this file with a question about one part of the code, not about one phase. Later phases change earlier decisions, so under phase headings one topic's entries were spread out, the connectivity collector's over Phases 2, 4 and 5, and entries made for code-review findings went under whichever heading was near. The phase still leads to the roadmap item or the review behind a decision, so the history line keeps it. ID order needs no judgement beyond the topic, and no entry moves when one is added, whereas a reading order would have to be decided again for every new entry. In one file, links name `design_decisions.md#dc-NN` ([DC-114](#dc-114)), so moving an entry breaks none, and one search covers every decision. Process termination is a topic of its own because its checks in `platform/windows` and its action on the Processes tab are one feature, with an entry each ([DC-87](#dc-87), [DC-88](#dc-88)).

**History:** Maintenance, design decisions.

---

<a id="dc-117"></a>
### DC-117: Entries: Decision, Rationale, optional Consequences and History in about 250 words, with figures, script behaviour, rules, tests, UI text and code locations kept elsewhere

**Decision:**

- **Fields:** **Decision:**, what was chosen; **Rationale:**, why, and the alternatives rejected; **Consequences:**, only for a cost or a constraint on later changes that the Rationale leaves out; then the one-line **History:** ([DC-115](#dc-115)). `build.ps1 -Format` and `-CheckFormat` fail on a field that is missing, unknown or out of order, and on a stub longer than one line.
- **Budget:** about 250 words, without the heading and the History line, and with at most the one measurement that justifies the choice. A merged entry gives each of its parts a sentence.
- **Kept elsewhere:** benchmark figures are in [performance.md](performance.md), what the build scripts do in [build_system.md](build_system.md), and the rules that every change follows, with their reasons, in the [coding guidelines](coding_guidelines.md). An entry lists no tests, unless how something is tested is itself a decision ([DC-87](#dc-87)), names the function that gives a text, such as `processTerminationMessage()`, instead of copying it, and names the code that carries its decision without listing where code lives.

**Rationale:** Test lists, figures, copied text and lists of files filled the longest entries, and each copy went stale unnoticed. The tests, formatters, script help and header comments stay current, so entries point to them. Figures get one document; an appendix would grow the file searched for decisions. A rule restated here drifts from the guideline, as DC-26 did.

The first 48 entries averaged about 150 words and the next 60 about 430, mostly on how the code carries out the choice, which the code says. Without it, the entries average about 200 words, and none has more than 300. The check counts fields, not words, because a hard limit would cut whichever sentence crossed it, and refusing unknown fields keeps out the notes that DC-115 removed. Consequences is optional because most costs are already in the Rationale.

**History:** Maintenance, design decisions; maintenance (rules in the coding guidelines and no code locations; DC-26, DC-68, DC-102 and DC-105 retired); maintenance (the fields and the budget); maintenance (every entry brought within the budget).
