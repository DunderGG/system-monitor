# Design Decisions

Rationale for non-obvious implementation choices made during development.
Each entry links to the relevant roadmap phase and source files.

Each entry has a permanent ID (`DC-01`, `DC-02`, …) that other documents and code comments can cite. A new entry takes the next unused number, even when it is filed under an earlier heading. Numbers are never reused or renumbered, and a superseded entry stays in place with a note pointing to the entry that replaced it.

## Contents

- [Windows API choices](#windows-api-choices)
  - [DC-01](#dc-01-process-enumeration-ntquerysysteminformation-over-tool-help): Process enumeration: `NtQuerySystemInformation` over Tool Help
  - [DC-02](#dc-02-core-metrics-direct-win32-apis-over-pdh): Core metrics: direct Win32 APIs over PDH
  - [DC-03](#dc-03-connectivity-status-getnetworkconnectivityhint-over-ad-hoc-probes): Connectivity status: `GetNetworkConnectivityHint` over ad-hoc probes
  - [DC-04](#dc-04-wmi-restricted-to-startup-hardware-inventory): WMI restricted to startup hardware inventory
  - [DC-05](#dc-05-etw-deferred-to-post-mvp): ETW deferred to post-MVP
  - [DC-06](#dc-06-network-profile-names-inetworklistmanager-lazy-and-cached): Network profile names: `INetworkListManager` lazy and cached
- [Phase 1 — Domain types (`src/domain/`)](#phase-1--domain-types-srcdomain)
  - [DC-07](#dc-07-cmake-library-type-interface-header-only): CMake library type: `INTERFACE` (header-only)
  - [DC-08](#dc-08-processinfocreationtime-type-stdchronosystem_clocktime_point): `ProcessInfo::creationTime` type: `std::chrono::system_clock::time_point`
  - [DC-09](#dc-09-systemsnapshottimestamp-type-stdchronosteady_clocktime_point): `SystemSnapshot::timestamp` type: `std::chrono::steady_clock::time_point`
  - [DC-10](#dc-10-networksample-stores-cumulative-byte-counters-not-rates): `NetworkSample` stores cumulative byte counters, not rates
  - [DC-11](#dc-11-operationalstatus-enum-all-7-if_oper_status-values): `OperationalStatus` enum: all 7 `IF_OPER_STATUS` values
  - [DC-12](#dc-12-stdoptional-for-processinfoimagepath-and-commandline): `std::optional` for `ProcessInfo::imagePath` and `::commandLine`
  - [DC-13](#dc-13-processinfoaccessdenied-flag-kept-in-band): `ProcessInfo::accessDenied` flag kept in-band
  - [DC-14](#dc-14-cpu-times-stored-as-cumulative-milliseconds-not-percentages): CPU times stored as cumulative milliseconds, not percentages
  - [DC-15](#dc-15-domain-string-encoding-utf-8-stdstring-over-wide-strings-stdwstring): Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)
- [Phase 1 — Monitoring infrastructure (`src/monitoring/`)](#phase-1--monitoring-infrastructure-srcmonitoring)
  - [DC-16](#dc-16-ringbuffert-mirrored-contiguous-storage-for-zero-copy-stdspan): `RingBuffer<T>`: Mirrored contiguous storage for zero-copy `std::span`
  - [DC-17](#dc-17-samplingscheduler-responsive-cancellation-via-stdcondition_variable_any-with-stdstop_token): `SamplingScheduler`: Responsive cancellation via `std::condition_variable_any` with `std::stop_token`
  - [DC-18](#dc-18-icollectort-dual-layer-interface-and-c20-concept): `ICollector<T>`: Dual-layer interface and C++20 concept
  - [DC-19](#dc-19-samplingscheduler-pointer-snapshotting-during-sampleonce-to-avoid-holding-m_collectormutex-during-collections): `SamplingScheduler`: Pointer snapshotting during `sampleOnce()` to avoid holding `m_collectorMutex` during collections
- [Phase 1 — Basic UI shell (`src/ui/`)](#phase-1--basic-ui-shell-srcui)
  - [DC-20](#dc-20-ui-module-dependency-isolation-and-composition-root-in-srcapp): UI module dependency isolation and composition root in `src/app/`
  - [DC-21](#dc-21-test_maincpp-test-discovery-without-qapplication-instantiation): `test_main.cpp`: Test discovery without `QApplication` instantiation
- [Phase 2 — Real Windows collectors and dashboard (`src/platform/windows/`)](#phase-2--real-windows-collectors-and-dashboard-srcplatformwindows)
  - [DC-22](#dc-22-utf-8-active-code-page-through-an-application-manifest): UTF-8 active code page through an application manifest
  - [DC-23](#dc-23-formatting-enforced-with-a-pinned-clang-format-and-the-existing-house-style): Formatting enforced with a pinned clang-format and the existing house style
  - [DC-24](#dc-24-collector-interfaces-in-their-own-qt-free-cmake-target): Collector interfaces in their own Qt-free CMake target
  - [DC-25](#dc-25-total-cpu-utilization-formula-deltakernel--deltauser---deltaidle--deltakernel--deltauser): Total CPU utilization formula: `(deltaKernel + deltaUser - deltaIdle) / (deltaKernel + deltaUser)`
  - [DC-26](#dc-26-encapsulation-of-windows-sdk-headers-strictly-inside-translation-units): Encapsulation of Windows SDK headers strictly inside translation units
  - [DC-27](#dc-27-deterministic-unit-testing-via-injected-time-and-clock-readers): Deterministic unit testing via injected time and clock readers
  - [DC-28](#dc-28-per-core-cpu-utilization-ntquerysysteminformationsystemprocessorperformanceinformation-dynamic-resolution-and-resizing-buffer-pattern): Per-core CPU utilization: `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` dynamic resolution and resizing-buffer pattern
  - [DC-29](#dc-29-multi-processor-group-support-64-logical-cores-ntquerysysteminformationex-iteration-and-total-cpu-aggregation): Multi-processor-group support (>64 logical cores): `NtQuerySystemInformationEx` iteration and total CPU aggregation
  - [DC-30](#dc-30-cpu-collector-integration-testing-invariant-based-assertions-and-synthetic-load-sensitivity): CPU collector integration testing: Invariant-based assertions and synthetic load sensitivity
  - [DC-31](#dc-31-memory-sampling-globalmemorystatusex-and-commit-charge-calculation): Memory sampling: `GlobalMemoryStatusEx` and commit charge calculation
  - [DC-32](#dc-32-disk-space-sampling-getdiskfreespaceexw-strictly-filtered-to-drive_fixed): Disk space sampling: `GetDiskFreeSpaceExW` strictly filtered to `DRIVE_FIXED`
  - [DC-33](#dc-33-deterministic-testing-of-memory-and-disk-collectors-via-reader-injection): Deterministic testing of memory and disk collectors via reader injection
  - [DC-34](#dc-34-network-adapter-traffic--throughput-getiftable2-with-raii-freemibtable-and-delta-rates): Network adapter traffic & throughput: `GetIfTable2` with RAII `FreeMibTable` and delta rates
  - [DC-35](#dc-35-adapter-identity--addresses-correlating-getiftable2-with-getadaptersaddresses): Adapter identity & addresses: Correlating `GetIfTable2` with `GetAdaptersAddresses`
  - [DC-36](#dc-36-adapter-details-cached-and-re-read-only-on-change): Adapter details cached and re-read only on change
  - [DC-37](#dc-37-repeated-collector-failures-logged-once-with-the-recovery): Repeated collector failures logged once, with the recovery
  - [DC-38](#dc-38-deterministic-testing-of-network-collection-via-reader-injection): Deterministic testing of network collection via reader injection
  - [DC-39](#dc-39-connectivity-status-notifynetworkconnectivityhintchange-with-a-synchronously-seeded-cache): Connectivity status: `NotifyNetworkConnectivityHintChange` with a synchronously seeded cache
  - [DC-40](#dc-40-explicit-connectivitylevelunknown-and-optional-ismetered): Explicit `ConnectivityLevel::Unknown` and optional `isMetered`
  - [DC-41](#dc-41-windows-to-domain-connectivity-mapping-in-a-separate-testable-header): Windows-to-domain connectivity mapping in a separate, testable header
  - [DC-42](#dc-42-system-uptime-gettickcount64-as-a-fast-collector-with-an-optional-snapshot-field): System uptime: `GetTickCount64` as a fast collector with an optional snapshot field
  - [DC-43](#dc-43-no-zero-placeholders-for-cpu-and-memory-optional-samples-end-to-end): No zero placeholders for CPU and memory: optional samples end to end
  - [DC-44](#dc-44-optional-collections-a-failed-query-is-not-an-empty-result): Optional collections: a failed query is not an empty result
  - [DC-45](#dc-45-health-evaluation-domain-types-a-pure-evaluator-in-monitoring-and-the-result-carried-in-the-snapshot): Health evaluation: domain types, a pure evaluator in `monitoring`, and the result carried in the snapshot
  - [DC-46](#dc-46-dashboard-cards-resourcecard-widgets-fed-by-pure-formatting-helpers): Dashboard cards: `ResourceCard` widgets fed by pure formatting helpers
  - [DC-47](#dc-47-ringbuffert-lives-in-domain-so-the-ui-can-use-it): `RingBuffer<T>` lives in `domain` so the UI can use it
  - [DC-48](#dc-48-minimal-sparklinewidget-built-early-for-the-dashboard-extended-in-phase-3): Minimal `SparklineWidget` built early for the dashboard, extended in Phase 3
- [Phase 3 — Sparkline charts and performance views (`src/ui/charts/`, `src/ui/`)](#phase-3--sparkline-charts-and-performance-views-srcuicharts-srcui)
  - [DC-49](#dc-49-sparkline-grid-scrolls-with-a-caller-supplied-sample-index): Sparkline grid scrolls with a caller-supplied sample index
  - [DC-50](#dc-50-multiple-series-share-one-time-axis-and-y-range-with-series-0-as-the-primary): Multiple series share one time axis and Y range, with series 0 as the primary
  - [DC-51](#dc-51-axis-labels-and-annotations-reserve-space-outside-the-plot): Axis labels and annotations reserve space outside the plot
  - [DC-52](#dc-52-sparkline-series-colours-a-validated-five-hue-palette-without-status-hues): Sparkline series colours: a validated five-hue palette without status hues
  - [DC-53](#dc-53-performance-view-a-sidebar-of-resource-pages-that-each-own-their-history): Performance view: a sidebar of resource pages that each own their history
  - [DC-54](#dc-54-snapshot-forwarding-every-view-records-every-snapshot-and-hidden-views-skip-repainting-code-review-f-4): Snapshot forwarding: every view records every snapshot, and hidden views skip repainting (code review F-4)
  - [DC-55](#dc-55-cpu-base-speed-from-callntpowerinformation-read-once-no-current-clock-speed): CPU base speed from `CallNtPowerInformation`, read once; no current clock speed
  - [DC-56](#dc-56-memory-detail-and-system-activity-counts-from-getperformanceinfo-as-fast-collectors): Memory detail and system activity counts from `GetPerformanceInfo`, as fast collectors
  - [DC-57](#dc-57-performance-readouts-pure-builders-with-stable-names-shown-in-a-readoutgrid): Performance readouts: pure builders with stable names, shown in a `ReadoutGrid`
  - [DC-58](#dc-58-performance-pages-fit-an-800600-window-re-flowing-readouts-and-a-scrolling-per-core-grid-code-review-f-20): Performance pages fit an 800×600 window: re-flowing readouts and a scrolling per-core grid (code review F-20)
  - [DC-59](#dc-59-network-page-charts-only-adapters-that-can-carry-traffic-networksamplehasipinterface-code-review-f-21): Network page charts only adapters that can carry traffic: `NetworkSample::hasIpInterface` (code review F-21)
  - [DC-60](#dc-60-dense-sparklines-minmax-decimation-a-one-pixel-cosmetic-line-and-an-envelope-fill): Dense sparklines: min/max decimation, a one-pixel cosmetic line, and an envelope fill
  - [DC-61](#dc-61-30-minutes-of-performance-history-a-chart-window-selector-and-the-measured-tick-cost): 30 minutes of performance history, a chart-window selector, and the measured tick cost
  - [DC-62](#dc-62-buildps1-launch-only-with--run-configure-only-when-needed-and-parallel-tests): `build.ps1`: launch only with `-Run`, configure only when needed, and parallel tests
  - [DC-63](#dc-63-memory-readouts-name-what-getperformanceinfo-reports-not-task-managers-figures-code-review-f-22): Memory readouts name what `GetPerformanceInfo` reports, not Task Manager's figures (code review F-22)
  - [DC-64](#dc-64-network-adapters-identified-by-interface-luid-not-by-name-code-review-f-23): Network adapters identified by interface LUID, not by name (code review F-23)
  - [DC-65](#dc-65-y-axis-gutter-sized-from-caller-supplied-label-templates-code-review-f-24): Y-axis gutter sized from caller-supplied label templates (code review F-24)
  - [DC-66](#dc-66-a-committed-tick-cost-benchmark-run-with-buildps1--benchmark-code-review-f-25): A committed tick-cost benchmark, run with `build.ps1 -Benchmark` (code review F-25)
  - [DC-67](#dc-67-chart-annotations-at-readout-precision-named-extremes-and-stable-volume-colours-code-review-f-26): Chart annotations at readout precision, named extremes, and stable volume colours (code review F-26)
  - [DC-68](#dc-68-shared-ui-constants-and-helpers-where-each-one-lives-code-review-f-28): Shared UI constants and helpers: where each one lives (code review F-28)
  - [DC-69](#dc-69-buildps1-retries-a-failed-configure-once-from-a-clean-cache): `build.ps1` retries a failed configure once from a clean cache
- [Phase 4 — Process collector (`src/platform/windows/`)](#phase-4--process-collector-srcplatformwindows)
  - [DC-70](#dc-70-process-list-read-into-a-kept-growing-buffer-through-an-injected-query): Process list read into a kept, growing buffer through an injected query
  - [DC-71](#dc-71-process-list-parsed-through-a-full-system_process_information-layout-checked-against-winternlh): Process list parsed through a full `SYSTEM_PROCESS_INFORMATION` layout checked against `winternl.h`
  - [DC-72](#dc-72-processdetailsstatus-replaces-processinfoaccessdenied): `ProcessDetailsStatus` replaces `ProcessInfo::accessDenied`
  - [DC-73](#dc-73-process-details-cached-by-pid-creation-time-with-a-domain-processidentity-key): Process details cached by `(PID, creation time)` with a domain `ProcessIdentity` key
  - [DC-74](#dc-74-lsassexe-and-csrssexe-recognised-by-image-name-and-filtered-before-any-details-reader): `lsass.exe` and `csrss.exe` recognised by image name and filtered before any details reader
  - [DC-75](#dc-75-process-details-read-with-a-limited-handle-checked-against-the-identity-and-logged-at-debug): Process details read with a limited handle, checked against the identity, and logged at debug
  - [DC-76](#dc-76-per-process-cpu-usage-computed-in-the-collector-as-a-share-of-all-logical-processors): Per-process CPU usage computed in the collector, as a share of all logical processors
  - [DC-77](#dc-77-a-slow-collector-thread-in-the-scheduler-shared-by-processes-and-connectivity): A slow-collector thread in the scheduler, shared by processes and connectivity
  - [DC-78](#dc-78-process-integration-tests-a-suspended-child-process-exact-read-sets-and-an-elevation-aware-access-check): Process integration tests: a suspended child process, exact read sets, and an elevation-aware access check
- [Phase 4 — Process explorer view (`src/ui/`)](#phase-4--process-explorer-view-srcui)
  - [DC-79](#dc-79-process-table-a-qabstractitemmodel-with-one-row-per-processidentity-changed-in-place): Process table: a `QAbstractItemModel` with one row per `ProcessIdentity`, changed in place
  - [DC-80](#dc-80-process-table-columns-io-rates-from-the-collector-and-na-for-missing-values): Process table columns: I/O rates from the collector, and N/A for missing values

---

## Windows API choices

### DC-01: Process enumeration: `NtQuerySystemInformation` over Tool Help

**Decision:** Use `NtQuerySystemInformation(SystemProcessInformation)` for process enumeration, not `CreateToolhelp32Snapshot`.

**Rationale:** `CreateToolhelp32Snapshot` internally calls `NtQuerySystemInformation`, strips out CPU, memory, and I/O metrics, and forces the caller to make hundreds of supplementary `OpenProcess` calls to reconstruct data the kernel already returned in that single call. On a system with ~300 processes this produces 300+ extra kernel transitions and takes 15–45 ms compared to 0.5–1.5 ms for the direct call. Beyond performance, `CreateToolhelp32Snapshot` denies access to metrics for 30–50% of processes on an unelevated caller because each `OpenProcess` can fail individually; `NtQuerySystemInformation` returns complete CPU, memory, and I/O data for all processes including protected and elevated ones. `NtQuerySystemInformation` is declared in `<winternl.h>`, has been ABI-stable since Windows 2000, and is the API used internally by Task Manager, Process Explorer, and System Informer.

---

### DC-02: Core metrics: direct Win32 APIs over PDH

**Decision:** Use `GetSystemTimes`, `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`, and `GetIfTable2` for core metrics. PDH is reserved only for advanced per-instance counters (e.g. per-physical-disk I/O rates) where no direct Win32 alternative exists.

**Rationale:** PDH has two structural problems that make it unsuitable for core metrics. First, PDH counter names are locale-dependent; `PdhAddCounterW` with a hard-coded English path fails on non-English Windows installations unless `PdhAddEnglishCounterW` is used, but even then PDH reads counter definitions from the registry — which is frequently corrupted on long-lived installations, causing `PDH_CSTATUS_NO_OBJECT` or `PDH_CSTATUS_NO_COUNTER` errors at runtime. The direct Win32 APIs (`GetSystemTimes` etc.) have zero registry dependency. Second, for per-process metrics PDH assigns index suffixes to processes that share a name (`chrome#0`, `chrome#1`) and silently re-indexes when one terminates; any open PDH query handle will silently read data from a different process after re-indexing, producing corrupt data with no error signal. `NtQuerySystemInformation` has no such concept — processes are identified by PID and creation time.

---

### DC-03: Connectivity status: `GetNetworkConnectivityHint` over ad-hoc probes

**Decision:** Use `GetNetworkConnectivityHint` (and `NotifyNetworkConnectivityHintChange` for async updates) to determine network connectivity state.

**Rationale:** Ad-hoc ICMP/TCP probes require choosing a target, have non-trivial latency (100–2000 ms), consume bandwidth, and produce a binary online/offline result that misses important states like captive portals and metered connections. `GetNetworkConnectivityHint` returns a structured `NL_NETWORK_CONNECTIVITY_LEVEL_HINT` — None, LocalAccess, ConstrainedInternetAccess (captive portal), or InternetAccess — along with cost information (metered vs. unmetered) and roaming state. This is a direct C-style kernel API with no COM overhead. The companion notification API delivers change events without polling. Requires Windows 10 version 2004 or later, which is within the project's target baseline.

---

### DC-04: WMI restricted to startup hardware inventory

**Decision:** WMI is queried at most once at application startup for static hardware information (`Win32_ComputerSystem`, `Win32_Processor`, `Win32_BaseBoard`). WMI is never polled for real-time metrics.

**Rationale:** WMI is an out-of-process COM call to `WmiPrvSE.exe`. Each query incurs 50–500 ms of latency and measurable CPU cost on the host. Polling it for metrics that change frequently (CPU usage, memory) would make the monitor a significant load source, contradicting the low-impact goal. WMI repository corruption is also common on long-lived Windows installations and manifests as silent data errors or hung calls. For the one legitimate use case — reading static hardware labels at startup — the latency is acceptable because it occurs once before the UI is shown. All real-time metrics use direct Win32 APIs.

---

### DC-05: ETW deferred to post-MVP

**Decision:** ETW (Event Tracing for Windows) is excluded from the MVP. It will be reconsidered only when per-process network bandwidth tracking or event-level diagnostics are a product requirement.

**Rationale:** ETW kernel trace sessions require administrator privileges, making them incompatible with the unelevated-by-default security posture. Windows enforces a hard system-wide limit of 64 concurrent ETW sessions, shared with EDR products, antivirus, and system loggers; exceeding this limit returns `ERROR_NO_SYSTEM_RESOURCES` and the session fails to start. Buffer sizing and flush tuning are non-trivial and highly workload-dependent. None of the MVP metrics require ETW — per-process CPU, memory, and I/O are fully available through `NtQuerySystemInformation`. The complexity and privilege cost are not justified until a specific feature (per-process network bandwidth) cannot be built without it.

---

### DC-06: Network profile names: `INetworkListManager` lazy and cached

**Decision:** The COM-based Network List Manager (`INetworkListManager`) is used only to retrieve a friendly network name (e.g. "Home-Wi-Fi") for display. It is queried at most once per network change event, never on every refresh tick.

**Rationale:** COM initialization carries per-thread cost and `INetworkListManager` calls cross into a system service. For a value that changes only when the user switches networks, querying it on every 1-second tick would be unnecessary overhead. All connectivity state (level, cost, roaming) and all throughput data come from `GetNetworkConnectivityHint` and `GetIfTable2`, which are C-style kernel APIs. The NLM is used purely for the human-readable label that those APIs do not provide.

---

## Phase 1 — Domain types (`src/domain/`)

### DC-07: CMake library type: `INTERFACE` (header-only)

**Decision:** `sysmon_domain` is declared as a CMake `INTERFACE` library with no compiled sources.

**Rationale:** Every domain type is a plain aggregate struct or `enum class`. There are no method bodies, no `.cpp` translation units, and nothing to link. An `INTERFACE` target is the idiomatic CMake choice in this situation: it propagates the `src/` include path transitively to any downstream target via `target_link_libraries`, so consumers never need to repeat `target_include_directories`. A `STATIC` library would produce an empty archive and add a link step with zero benefit.

---

### DC-08: `ProcessInfo::creationTime` type: `std::chrono::system_clock::time_point`

**Decision:** The process creation timestamp is stored as `std::chrono::system_clock::time_point`, not `uint64_t` raw FILETIME or `steady_clock::time_point`.

**Rationale:** The source value is a Windows `FILETIME` — a 100-nanosecond tick count since 1601-01-01, which is a calendar epoch. `system_clock` is the C++ clock that maps to calendar time, making the conversion at the platform boundary lossless and semantically correct. `steady_clock` is reserved for `SystemSnapshot::timestamp` where monotonic elapsed-time arithmetic is needed (CPU %, network throughput rates). Storing a raw `uint64_t` would leak a Windows type into the domain layer, which must remain platform-free.

---

### DC-09: `SystemSnapshot::timestamp` type: `std::chrono::steady_clock::time_point`

**Decision:** `SystemSnapshot::timestamp` uses `steady_clock`, not `system_clock`.

**Rationale:** The timestamp exists solely for rate calculations (CPU usage %, network bytes/sec) between consecutive snapshots. `steady_clock` is monotonic and unaffected by system clock adjustments or DST transitions, making delta calculations reliable. It carries no calendar meaning; the UI must use `system_clock::now()` separately if it needs a human-readable time to display.

---

### DC-10: `NetworkSample` stores cumulative byte counters, not rates

**Decision:** `inBytesTotal` and `outBytesTotal` are cumulative totals as returned by `GetIfTable2`, not pre-computed bytes/sec rates.

**Rationale:** Rate computation requires dividing a byte delta by a precisely measured time delta. That measurement belongs in the collector, which owns the `steady_clock` baseline from the previous tick. Storing a rate in the domain type would mean the type carries a value that depends on when it was last queried — breaking the immutable-snapshot guarantee. Cumulative counters are also correct across tick-interval jitter, whereas a rate pre-computed with an assumed interval would drift if the scheduler tick is late.

---

### DC-11: `OperationalStatus` enum: all 7 `IF_OPER_STATUS` values

**Decision:** `OperationalStatus` includes all seven values from the Windows `IF_OPER_STATUS` enum (`Up`, `Down`, `Testing`, `Unknown`, `Dormant`, `NotPresent`, `LowerLayerDown`), not a trimmed subset.

**Rationale:** A 1:1 mapping between the domain enum and the Windows API enum makes the platform-boundary conversion trivial and keeps the domain type self-documenting. Collapsing values (e.g. merging `Dormant` and `NotPresent` into a generic `Inactive`) would lose information that the UI or future alert logic might need. The domain module has no Windows dependency — having the same *shape* as a Windows enum does not violate the no-Windows-headers rule.

---

### DC-12: `std::optional` for `ProcessInfo::imagePath` and `::commandLine`

**Decision:** Image path and command line are `std::optional<std::string>`, not empty strings.

**Rationale:** These fields are acquired lazily — only on the first detection of a new `(pid, creationTime)` pair — and may be permanently unavailable for protected or system processes even with `PROCESS_QUERY_LIMITED_INFORMATION`. An empty string is ambiguous: it could mean "not yet queried", "query failed", or "the process genuinely has no path". `std::nullopt` is unambiguous: this data is not available. The UI can branch on `has_value()` to render a clear indicator rather than displaying a blank field.

---

### DC-13: `ProcessInfo::accessDenied` flag kept in-band

**Decision:** `ProcessInfo` includes an `accessDenied` bool rather than being omitted from the snapshot or replaced with an error variant at the `vector<ProcessInfo>` level.

**Rationale:** Even when a process cannot be fully introspected, `NtQuerySystemInformation` returns its name and PID. Omitting the process from the snapshot entirely would silently hide it from the user (wrong: Task Manager shows all processes). A separate error list alongside `vector<ProcessInfo>` would force all UI code to merge two collections. The in-band flag keeps the snapshot a single coherent list and lets the UI render a partial row — image name visible, metrics showing `—` — which is the same pattern used by Task Manager and Process Explorer.

**Superseded in part by [DC-72](#dc-72-processdetailsstatus-replaces-processinfoaccessdenied):** the status stays in-band, but the bool became the `ProcessDetailsStatus` enum `detailsStatus`, because `NtQuerySystemInformation` never fails to return a process's metrics; only the image path and command line can be missing.

---

### DC-14: CPU times stored as cumulative milliseconds, not percentages

**Decision:** `ProcessInfo::cpuUserTimeMs` and `::cpuKernelTimeMs` are cumulative totals in milliseconds, not pre-computed CPU percentages.

**Rationale:** CPU percentage for a process requires comparing the CPU time delta between two snapshots against the wall-clock delta between those same snapshots. This calculation belongs in the collector or view model, which tracks both baselines. Storing a percentage in the domain struct would mean the value is meaningless without knowing the interval over which it was measured — and that interval is not part of the struct. Cumulative milliseconds are the raw truth; any derived value can be computed correctly from them.

---

### DC-15: Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)

**Decision:** All string fields in domain types use `std::string` with UTF-8 encoding. Wide-to-UTF-8 conversion happens at the `platform/windows` boundary.

**Rationale:** The domain layer must be platform-independent. `std::wstring` is a Windows-specific convention (16-bit code units); using it would leak a platform assumption into `src/domain/`. UTF-8 is the de facto standard encoding for cross-platform C++ and is natively supported by Qt (`QString::fromUtf8`), spdlog, and nlohmann/json. The Windows API wrapper functions in `platform/windows/` convert from `WCHAR*` / `std::wstring` to UTF-8 `std::string` using `WideCharToMultiByte(CP_UTF8, ...)` or equivalent. Supplementary-plane characters (e.g., emoji in filenames) round-trip correctly through UTF-8.

---

## Phase 1 — Monitoring infrastructure (`src/monitoring/`)

### DC-16: `RingBuffer<T>`: Mirrored contiguous storage for zero-copy `std::span`

**Decision:** `RingBuffer<T>` pre-allocates $2 \times \text{capacity}$ contiguous storage and mirrors every push to both `pos` and `pos + capacity`.

**Rationale:** A traditional circular buffer wraps around the end of its storage array, requiring either linearizing elements into a temporary buffer (incurring heap allocation or copying) or forcing callers to consume two disjoint slices (`span1` and `span2`). Chart widgets (e.g. sparklines) and consumer models require a single contiguous `std::span<const T>` representing samples in chronological order. By pre-allocating twice the capacity and writing each sample to `pos` and `pos + capacity`, any window of length $\le \text{capacity}$ starting at the oldest element's index is guaranteed to be contiguous in memory. This achieves $O(1)$ zero-copy contiguous span access while strictly ensuring zero heap allocations on `push()` after construction.

---

### DC-17: `SamplingScheduler`: Responsive cancellation via `std::condition_variable_any` with `std::stop_token`

**Decision:** The scheduler thread tick loop uses `std::condition_variable_any::wait_until` with `std::stop_token` rather than `std::this_thread::sleep_for` or a polling sleep loop.

**Rationale:** A standard `sleep_for(m_interval)` cannot be interrupted until the interval expires, delaying application shutdown by up to 1000 ms. A polling loop (e.g. sleeping 10 ms repeatedly) wastes CPU cycles and adds wake jitter. In C++20, `std::condition_variable_any` natively accepts a `std::stop_token` in `wait_until`. If a stop is requested (e.g. when `SamplingScheduler::stop()` or the destructor is called), the condition variable wakes immediately and exits the thread. Furthermore, calculating `nextTick = tickStart + m_interval` prevents interval drift across ticks when collection work takes measurable time.

---

### DC-18: `ICollector<T>`: Dual-layer interface and C++20 concept

**Decision:** Provide `template<typename T> class ICollector` with a pure virtual `collect()` method for dynamic polymorphism in `SamplingScheduler`, paired with a `Collector<C, T>` C++20 concept.

**Rationale:** `SamplingScheduler` must store heterogeneous collectors (CPU, memory, disk, etc.) whose concrete implementations can be swapped between synthetic implementations (for testing and early phases) and real Windows collectors (in later phases). Making `SamplingScheduler` an un-templated `QObject` holding `std::unique_ptr<ICollector<T>>` keeps Qt signal/slot metadata clean and avoids template bloat. Meanwhile, the `Collector<C, T>` concept enables compile-time verification in tests and non-virtual template contexts, ensuring consistency across static and dynamic collection code.

---

### DC-19: `SamplingScheduler`: Pointer snapshotting during `sampleOnce()` to avoid holding `m_collectorMutex` during collections

**Decision:** `sampleOnce()` locks `m_collectorMutex` only long enough to copy raw collector pointers (`get()`), then invokes `collect()` on each collector outside the lock. The invariant that collectors may only be registered before `start()` or after `stop()` is enforced with assertions and documented.

**Rationale:** In Phase 2, real Windows API collectors may take 0.5–1.5 ms (e.g. process enumeration) or query network tables. Holding `m_collectorMutex` for the duration of all collections would unnecessarily serialize collector configuration and sampling, and block any caller wanting to inspect or modify scheduler registration. Releasing the lock before collection keeps critical sections minimal ($O(1)$ pointer copies) while maintaining strict safety under the documented stopped-state registration invariant.

---

## Phase 1 — Basic UI shell (`src/ui/`)

### DC-20: UI module dependency isolation and composition root in `src/app/`

**Decision:** The `ui` module depends exclusively on `Qt6::Widgets` and `sysmon_domain`, with zero dependencies on `sysmon_monitoring` or Windows headers. Wiring between `SamplingScheduler` signals and `MainWindow` slots is handled strictly in `src/app/main.cpp` (the application composition root).

**Rationale:** Maintaining a one-way dependency flow prevents cyclic references and tight coupling. The UI layer only knows about domain values (`SystemSnapshot`). It does not know how or when data is collected, nor what thread model is used. By placing the signal/slot `Qt::QueuedConnection` wiring in `main.cpp`, `MainWindow` can be tested in isolation with mock snapshots, and collectors can be altered or replaced without recompiling any UI translation units.

---

### DC-21: `test_main.cpp`: Test discovery without `QApplication` instantiation

**Decision:** In the test runner (`tests/test_main.cpp`), `QApplication` instantiation is bypassed when `--gtest_list_tests` is passed during CMake's `gtest_discover_tests` step.

**Rationale:** When CMake invokes the test binary with `--gtest_list_tests` at build time to discover tests, initializing `QApplication` would attempt to load platform plugins before they are fully deployed or in headless environments without an active display session, risking modal error dialogues or timeouts. Bypassing `QApplication` for list-only execution allows instantaneous and reliable test discovery, while full test execution creates `QApplication` and leverages the deployed platform plugin.

---

## Phase 2 — Real Windows collectors and dashboard (`src/platform/windows/`)

### DC-22: UTF-8 active code page through an application manifest

**Decision:** `src/app/system_monitor.manifest` sets `activeCodePage` to `UTF-8`, and CMake embeds it in `system_monitor.exe` and in `system_monitor_tests.exe`, so tests run under the same code page as the app. Narrow strings passed to Windows and the CRT are therefore UTF-8. The logging setup moved into the `sysmon_app_logging` library so an integration test can configure logging with a path that contains CJK and Cyrillic characters.

**Rationale:** `configureLogging()` passes `path::string()` to spdlog. Under a legacy ANSI code page, MSVC's `path::string()` throws `std::system_error` for characters that page cannot represent. A user profile path with a Chinese or Cyrillic name on a Western-locale system therefore made the app terminate at startup, and against the no-exceptions rule ([code review F-19](code_reviews/phase_2.md#f-19-log-file-path-is-converted-to-the-ansi-code-page)). The UTF-8 code page fixes every narrow-string path in the process, not only the log file, and it matches the decision that domain strings are UTF-8. It needs Windows 10 1903, below the project's existing 2004 requirement (`GetNetworkConnectivityHint`). The rejected alternative, building spdlog with wide file names, would fix only the log path and depends on what the vcpkg port offers. The manifest is also where the Phase 6 DPI-awareness setting belongs. A test checks `GetACP() == CP_UTF8`, so losing the manifest fails the build's tests.

---

### DC-23: Formatting enforced with a pinned clang-format and the existing house style

**Decision:** `.clang-format` describes the style the code already followed: indented `case` labels, and a Windows SDK include block whose `SortPriority` keeps the order the SDK requires. Pointers and references bind to the type (`Type* name`, `Type& name`), as in the C++ Core Guidelines (NL.18) and the guideline examples. All files were reformatted once in a formatting-only commit. `build.ps1 -Format` and `-CheckFormat` run clang-format on every C++ file under `src/`, `tests/`, and `benchmarks/` that is tracked, or new and not ignored, so files created since the last commit are formatted before they are first committed. A CI job pins clang-format 22.1.3 from PyPI, and `build.ps1` fails in CI (and warns locally) when the major version differs. `bootstrap.ps1` reports clang-format and can install LLVM with `-InstallMissing`, but a missing one is only a warning because building does not need it.

**Rationale:** Code review F-7 added `.clang-format`, but nothing ran it, and drift had begun: lines over the hard limit, Qt headers inside the standard-library group, and SDK headers in different orders ([code review F-17](code_reviews/phase_2.md#f-17-clang-format-is-not-enforced)). Matching the configuration to the existing style kept the reformat to alignment and line wrapping, rather than re-indenting every `switch`. The SDK order cannot be left to alphabetical sorting: `iphlpapi.h` sorts before `ws2tcpip.h`, which leaves `CancelMibChangeNotify2` undeclared. clang-format's output changes between major versions, so CI and developers must use the same major version, and PyPI offers the exact release that Visual Studio ships. The standard-library category matches case-sensitively because clang-format's include regexes are otherwise case-insensitive and would put `<QString>` in the standard-library group. The left alignment was chosen over LLVM's default right alignment (`Type &name`, also Qt's style) because the project's guidelines follow the C++ Core Guidelines, which recommend emphasizing the type.

---

### DC-24: Collector interfaces in their own Qt-free CMake target

**Decision:** `monitoring/collector.h` is the header-only `INTERFACE` target `sysmon_collector_interfaces`, which depends only on `sysmon_domain`. `sysmon_monitoring` and `sysmon_platform_windows` both link it publicly. `platform/windows` may include `collector.h` and no other `monitoring` header.

**Rationale:** Every Windows collector implements an interface from `collector.h`, but `sysmon_platform_windows` linked only `sysmon_domain`. The include worked only because all modules share the `src/` include root, so CMake did not know about the dependency ([code review F-13](code_reviews/phase_2.md#f-13-platformwindows-depends-on-monitoring-headers-without-declaring-it)). Linking `sysmon_monitoring` instead would have pulled `Qt6::Core` into a module whose rules forbid Qt. A separate target states the real dependency, keeps Qt out, and is exercised by `system_monitor_platform_tests`, which links `sysmon_platform_windows` without Qt. Because the include root is shared, CMake cannot stop a `platform/windows` file from including another `monitoring` header. That rule is enforced by review and stated in `src/platform/windows/AGENTS.md`.

---

### DC-25: Total CPU utilization formula: `(deltaKernel + deltaUser - deltaIdle) / (deltaKernel + deltaUser)`

**Decision:** Calculate total CPU utilization by subtracting `deltaIdle` from `(deltaKernel + deltaUser)` and dividing by `(deltaKernel + deltaUser)` over monotonic elapsed time.

**Rationale:** Windows `GetSystemTimes` returns cumulative idle, kernel, and user times in 100 ns `FILETIME` ticks across all logical processors. A key characteristic of Windows kernel accounting is that `lpKernelTime` already includes the execution time of the idle thread. Thus, total system capacity elapsed across all processors is `deltaKernel + deltaUser`, and active busy time is `(deltaKernel + deltaUser) - deltaIdle`. This ratio is mathematically invariant to CPU frequency scaling (Intel SpeedStep / AMD Cool'n'Quiet), processor group topologies, and core count. We guard against multi-core non-atomic counter jitter by clamping usage to $[0.0\%, 100.0\%]$. When no rate can be computed (non-positive elapsed time, a counter that went backwards, or no elapsed CPU time), `calculateCpuUsage` returns `std::nullopt` rather than 0% (see [DC-43](#dc-43-no-zero-placeholders-for-cpu-and-memory-optional-samples-end-to-end)).

---

### DC-26: Encapsulation of Windows SDK headers strictly inside translation units

**Decision:** `cpu_collector.h` contains no `<Windows.h>` or Windows SDK types. All Windows types (`FILETIME`, `DWORD`, etc.) are converted to standard fixed-width integer types (`uint64_t`) in `cpu_collector.cpp`.

**Rationale:** Including `<Windows.h>` in a header exposes macros like `min`, `max`, `near`, `far`, and thousands of global symbols transitively to any module that includes it, violating the project rule that only `platform/windows` interacts with the Windows SDK. By confining `<Windows.h>` strictly to `.cpp` files, headers remain clean, lightweight standard C++20.

---

### DC-27: Deterministic unit testing via injected time and clock readers

**Decision:** `CpuCollector` provides a dependency-injected constructor accepting custom `SystemTimesReader` and `SteadyClockReader` lambdas alongside its default Windows constructor, paired with a pure `calculateCpuUsage` function.

**Rationale:** Unit tests must be fast, deterministic, and free of OS dependencies per project rules. The injected constructor allows simulating edge cases such as exact idle/user/kernel ratios, zero elapsed intervals, clock jitter where idle temporarily appears greater than kernel, and API query failures without invoking the Windows kernel or relying on sleep timers. Real host integration tests in `tests/integration/` verify live `GetSystemTimes` execution separately.

---

### DC-28: Per-core CPU utilization: `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` dynamic resolution and resizing-buffer pattern

**Decision:** Resolve `NtQuerySystemInformation` dynamically from `ntdll.dll` via `GetModuleHandleW` / `GetProcAddress` and query `SystemProcessorPerformanceInformation` (class 8) using a resizing-buffer loop. Per-core utilization is calculated by applying the same `calculateCpuUsage` logic to each core's `IdleTime`, `KernelTime`, and `UserTime`.

**Rationale:** `NtQuerySystemInformation` is the native NT kernel interface used by Task Manager and Process Explorer to query per-processor counters. In Windows, `ntdll.dll` is mapped into every user-mode process at process creation, so resolving the function pointer dynamically via `GetProcAddress` avoids static import dependencies while ensuring universal compatibility. The returned `SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION` structure includes idle time within `KernelTime` for each individual core, mirroring the aggregate behavior of `GetSystemTimes`. The resizing-buffer loop pattern starts with an estimated allocation equal to the detected logical core count and automatically handles `STATUS_INFO_LENGTH_MISMATCH` (0xC0000004) if processor topology or active counts change.

---

### DC-29: Multi-processor-group support (>64 logical cores): `NtQuerySystemInformationEx` iteration and total CPU aggregation

**Decision:** Use `NtQuerySystemInformationEx` to iterate through all active processor groups (`0 .. GetActiveProcessorGroupCount() - 1`), with fallback to `NtQuerySystemInformation` for Group 0 if `Ex` resolution or runtime calls fail. When more than one processor group is active (`GetActiveProcessorGroupCount() > 1`), compute total CPU usage by aggregating `idleTime`, `kernelTime`, and `userTime` across all cores rather than relying on `GetSystemTimes`. The decision is keyed on the group count, not the core count, because Windows can form several groups on machines with 64 or fewer logical processors (for example one group per NUMA node, or `bcdedit /set groupsize`). If the per-core query fails on such a system, no total is reported for that tick rather than presenting `GetSystemTimes`' single group as the whole machine ([code review F-15](code_reviews/phase_2.md#f-15-cpu-multi-group-aggregation-keys-on-core-count-not-group-count)).

**Rationale:** On 64-bit Windows, processor affinities are limited to 64 bits per group, so high-core machines (>64 logical processors) are partitioned into multiple processor groups. On such machines, `GetSystemTimes` and legacy `NtQuerySystemInformation` only report statistics for the primary group of the calling thread, rendering workloads in secondary groups invisible. Calling `NtQuerySystemInformationEx` with `USHORT group` in the input buffer retrieves each group's cores sequentially, guaranteeing that `CpuSample::coreUsagePercents` captures all logical cores in continuous processor index order. Furthermore, aggregating all cores' counter deltas to compute `totalUsagePercent` on multi-group systems ensures accurate system-wide utilization metrics regardless of how the OS scheduler distributes threads across groups.

---

### DC-30: CPU collector integration testing: Invariant-based assertions and synthetic load sensitivity

**Decision:** Design host integration tests around system and mathematical invariants ($0.0 \le \text{usage} \le 100.0$, topology matching `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`, non-NaN/Inf) and synthetic workload sensitivity rather than asserting fixed utilization values.

**Rationale:** Unlike deterministic unit tests with injected data, host integration tests execute against live Windows kernel APIs where background OS services, indexing, and power-saving C-states cause continuous utilization fluctuations. Asserting fixed numbers produces flaky tests. Verifying invariants across multiple consecutive real-time samples, observing load increases during controlled multi-threaded synthetic loops, and validating live signal delivery through `SamplingScheduler` proves the collector operates correctly on real hardware under variable load without false positives.

---

### DC-31: Memory sampling: `GlobalMemoryStatusEx` and commit charge calculation

**Decision:** Sample physical and virtual memory using `GlobalMemoryStatusEx`. Compute `commitCurrent` as `ullTotalPageFile - ullAvailPageFile`. Clamp memory usage percent defensively to $[0.0\%, 100.0\%]$.

**Rationale:** `GlobalMemoryStatusEx` is an ultra-fast kernel query (< 1 microsecond) with zero external or registry dependencies. In `MEMORYSTATUSEX`, `ullTotalPhys` and `ullAvailPhys` report physical RAM. `ullTotalPageFile` represents the current system commit limit (physical RAM plus current pagefile allocation), and `ullAvailPageFile` represents the remaining commit space available to processes before allocations fail. Subtracting available pagefile from total pagefile yields the active system commit charge (`commitCurrent`), matching Task Manager's commit metric ("Committed x / y GB"). Defensive boundary clamping protects against transient kernel counter jitter.

---

### DC-32: Disk space sampling: `GetDiskFreeSpaceExW` strictly filtered to `DRIVE_FIXED`

**Decision:** Enumerate drive letters via `GetLogicalDrives()`, inspect each drive root using `GetDriveTypeW()`, and call `GetDiskFreeSpaceExW` only for drives returning `DRIVE_FIXED`.

**Rationale:** Calling disk space APIs on removable drives (`DRIVE_REMOVABLE`), optical media (`DRIVE_CDROM`), or remote shares (`DRIVE_REMOTE`) can cause blocking network timeouts or trigger Windows system modal dialogs ("Insert disk into drive..."), freezing the background scheduler tick. Restricting collection strictly to `DRIVE_FIXED` ensures fast, non-blocking queries of all local SSDs and HDDs. If `GetDiskFreeSpaceExW` fails for a specific drive (e.g. unformatted volume or BitLocker locked partition), that drive is skipped gracefully without interrupting collection for other healthy drives.

---

### DC-33: Deterministic testing of memory and disk collectors via reader injection

**Decision:** Provide dependency-injected constructors in `MemoryCollector` and `DiskCollector` accepting reader callbacks (`MemoryStatusReader`, `LogicalDrivesReader`, `DiskSpaceReader`) alongside pure calculation functions `calculateMemorySample` and `calculateDiskSample`.

**Rationale:** In accordance with project rules, unit tests must be fast, deterministic, and free of OS dependencies. Injected readers allow testing boundary conditions — such as zero total memory/disk space, free space exceeding total due to quota configurations, 100% capacity, API query failures, and multi-drive volume sets — without altering host configuration or creating synthetic drive partitions. Integration tests in `tests/integration/` independently verify live Windows API queries on real host hardware.

---

### DC-34: Network adapter traffic & throughput: `GetIfTable2` with RAII `FreeMibTable` and delta rates

**Decision:** Use `GetIfTable2` to read 64-bit cumulative byte counters (`InOctets`, `OutOctets`), negotiated link speed, and operational status. Wrap allocated `MIB_IF_TABLE2` memory in a custom RAII deleter calling `FreeMibTable`. Compute instantaneous throughput rates (`inBytesPerSec`, `outBytesPerSec`) from counter deltas divided by elapsed monotonic time (`std::chrono::steady_clock`).

**Rationale:** `GetIfTable2` is the modern Windows IP Helper interface that returns 64-bit octet counters without registry or PDH dependencies. Unlike legacy 32-bit `GetIfTable`, 64-bit counters will not roll over under modern gigabit and 10 GbE traffic within days. RAII wrapping via `std::unique_ptr<MIB_IF_TABLE2, MibTableDeleter>` guarantees memory is safely released on all return paths. Throughput rates are computed per adapter baseline over measured monotonic durations. A rate is `std::nullopt`, never 0, when it cannot be computed: the first sample for an adapter, zero elapsed time, or a counter that went backwards (e.g. an interface reset), where the delta across the reset is unknown. Each direction is evaluated independently, so a reset of one counter does not hide the other direction's rate. A genuinely idle link still reports 0. See [known deviation D-6](known_deviations.md#d-6-network-throughput-reports-zero-when-no-baseline-exists).

---

### DC-35: Adapter identity & addresses: Correlating `GetIfTable2` with `GetAdaptersAddresses`

**Decision:** Query `GetAdaptersAddresses` (with `AF_UNSPEC` and `GAA_FLAG_INCLUDE_PREFIX`) to retrieve friendly names, device descriptions, IPv4/IPv6 addresses, and DNS servers, correlating them with `GetIfTable2` rows using `NET_LUID::Value` (falling back to `IfIndex`). Filter out loopback interfaces (`IF_TYPE_SOFTWARE_LOOPBACK`) and NDIS filter interfaces (`InterfaceAndOperStatusFlags.FilterInterface`). Carry `InterfaceAndOperStatusFlags.HardwareInterface` into `NetworkSample::isHardwareInterface`.

**Rationale:** While `GetIfTable2` provides high-precision counters and link state, it lacks network addresses and DNS configuration. `GetAdaptersAddresses` provides comprehensive address and DNS information. Correlating both APIs via the 64-bit `InterfaceLuid.Value` ensures exact, collision-free pairing between interface counters and IP addresses across all physical, virtual, and Wi-Fi adapters. Filtering `IF_TYPE_SOFTWARE_LOOPBACK` removes local pseudo-interfaces (`127.0.0.1` / `::1`) from user-facing monitor tables.

`GetIfTable2` also returns one row per NDIS lightweight filter bound to an adapter (for example "Ethernet-WFP Native MAC Layer LightWeight Filter-0000" and "Ethernet-QoS Packet Scheduler-0000"). These rows are Up and carry the same counters as the adapter, so a sum over all Up rows counted the physical traffic several times ([code review F-9](code_reviews/phase_2.md#f-9-network-throughput-total-counts-the-same-traffic-several-times)). They mean nothing to a user and Task Manager does not show them, so they are dropped at the platform boundary. Virtual adapters such as VPN tunnels and Hyper-V vEthernet are kept, because the Phase 5 network view should list them, but they are marked non-hardware: their traffic also crosses a physical adapter, so totals must not include them.

---

### DC-36: Adapter details cached and re-read only on change

**Decision:** `NetworkCollector` reads `GetIfTable2` on every tick but keeps the `GetAdaptersAddresses` results (names, descriptions, IP addresses, DNS servers) in an `AdapterDetailsCache`. The cache re-reads them when an address or interface change notification (`NotifyUnicastIpAddressChange`, `NotifyIpInterfaceChange`) has marked it stale, when `GetIfTable2` shows an interface that was not present at the last refresh, or when 30 seconds have passed. A failed re-read clears the details and is retried on the next tick. The production reader, `WindowsAdapterReader`, lives in `network_collector.cpp` and is owned by the collector. The refresh policy is a separate class with no Windows types so `tests/unit/` can test it.

**Rationale:** On the review machine `GetAdaptersAddresses` took about 2 ms per call, compared with about 0.7 ms for `GetIfTable2`. That alone exceeded the fast-collector budget, and it was spent re-reading data that changes only when the network configuration does ([code review F-10](code_reviews/phase_2.md#f-10-getadaptersaddresses-runs-on-every-scheduler-tick)). The notifications are an event-driven source under the architecture's rules: the callbacks only set the stale mark, the registrations are RAII members declared after the cache so they are cancelled first, and the cache starts stale so the first sample is complete. The stale mark is cleared before re-reading, so a change reported during a re-read triggers another one. No IP Helper notification reports DNS server changes, so the 30-second maximum age catches those, and it keeps details current if registration fails. The rejected alternative was refreshing only when the set of interfaces changes, which misses address changes on an existing adapter (for example a DHCP renewal with a new lease).

---

### DC-37: Repeated collector failures logged once, with the recovery

**Decision:** Every OS query a collector repeats on each tick reports failures through a `RepeatedFailureLog` (`platform/windows/repeated_failure_log.h`). The first failure after a success is logged at the level the call site chooses, consecutive repeats are logged at `debug`, and the first success afterwards is logged at `info` with the failure count. The state lives with the query: in the default reader closures (`mutable` lambdas), in `WindowsAdapterReader`, or per volume for `GetDiskFreeSpaceExW`.

**Rationale:** Collectors run every second, so a persistent failure such as a BitLocker-locked fixed volume logged about 86,000 warnings a day. Because the logger flushes on `warn`, each of those also forced a disk write, and the 15 MB of rotating logs pushed out the useful history within a day or two ([code review F-11](code_reviews/phase_2.md#f-11-persistent-failures-are-logged-on-every-tick)). Logging only when the state changes follows the connectivity collector's change-only logging. It keeps the log readable and still leaves the full detail available at `debug`. Keeping the state beside each query, rather than in one shared object, lets a failing volume and a healthy one report independently. It also keeps the injected test readers unchanged.

---

### DC-38: Deterministic testing of network collection via reader injection

**Decision:** Inject `NetworkAdaptersReader` and `SteadyClockReader` lambdas into `NetworkCollector`, supported by a pure `calculateNetworkSamples` function managing historical baselines and purging disconnected adapters.

**Rationale:** Simulating multiple network interfaces, varying link speeds, counter rollovers, IP/DNS configurations, loopback filters, and baseline eviction across elapsed intervals requires deterministic control over input data and monotonic timestamps. Injected readers allow comprehensive unit testing without network privileges or hardware manipulation, while host integration tests verify real Windows IP Helper API calls.

---

### DC-39: Connectivity status: `NotifyNetworkConnectivityHintChange` with a synchronously seeded cache

**Decision:** `ConnectivityCollector` seeds a mutex-protected `ConnectivityStatus` cache with `GetNetworkConnectivityHint` in its constructor, then registers `NotifyNetworkConnectivityHintChange` (with `InitialNotification = TRUE`). `collect()` returns the cached value on the scheduler tick. Registration goes through an injected `ConnectivitySubscriber` that returns an RAII `ConnectivitySubscription`. The production `WindowsConnectivitySubscription`, defined in the `.cpp`, calls `CancelMibChangeNotify2` in its destructor. The subscription is the collector's last member, so it is destroyed before the mutex and cache. If registration fails at runtime, the collector falls back to polling `GetNetworkConnectivityHint` in `collect()` (see [known deviation D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)). `collect()` logs each change of status once at `info`; the notification callback only stores the value.

**Rationale:** Connectivity changes rarely, so event-driven updates avoid a kernel round-trip every tick while keeping `collect()` cheap. Seeding before registration makes the first `collect()` accurate without waiting for the asynchronous initial callback, and ordering it before registration means a newer callback value can never be overwritten by the seed; the initial notification still covers a change between the two. `CancelMibChangeNotify2` waits for in-flight callbacks, so destroying the handle first guarantees the callback never touches a destroyed cache. The Windows subscription class lives in the `.cpp` so no Windows types appear in `connectivity_collector.h`. Injecting the subscriber, like the reader, lets unit tests deliver notifications and verify seeding order, cancellation on destruction, and the polling fallback (resolving [known deviation D-5](known_deviations.md#d-5-notification-callback-wiring-has-no-unit-test)). Logging only on change follows the architecture's `info` level for connectivity state changes without repeating the same state every tick. It happens in `collect()` on the scheduler thread rather than in the callback, because event-driven collector rule 1 allows the callback only to store: a file-sink write is blocking I/O on an OS thread-pool thread ([code review F-14](code_reviews/phase_2.md#f-14-connectivity-callback-writes-to-the-log-on-the-os-thread)). A change that reverts within one tick is therefore not logged, which is acceptable for a status log.

**Superseded in part by [DC-77](#dc-77-a-slow-collector-thread-in-the-scheduler-shared-by-processes-and-connectivity):** `collect()` now runs on the slow-collector thread rather than the scheduler tick, in notification and polling mode alike, so the polling fallback no longer runs on the tick (D-2 resolved).

---

### DC-40: Explicit `ConnectivityLevel::Unknown` and optional `isMetered`

**Decision:** Add `ConnectivityLevel::Unknown` as the default level and make `ConnectivityStatus::isMetered` a `std::optional<bool>`. The collector reports `Unknown` / `std::nullopt` before the first successful read, after a failed read, and when Windows reports `NetworkConnectivityLevelHintUnknown`, `NetworkConnectivityLevelHintHidden`, or `NetworkConnectivityCostHintUnknown`. A failed read does not keep the previous value.

**Rationale:** The guidelines forbid substituting a default for missing data. With only `None`, "we could not determine connectivity" was indistinguishable from "offline", and a retained previous value would present stale data as current. `Unknown` is an enum value rather than wrapping the whole status in `std::optional` so the scheduler, snapshot, and UI keep a single value type and the UI can render the state directly (for example "Unknown" or a dimmed indicator).

---

### DC-41: Windows-to-domain connectivity mapping in a separate, testable header

**Decision:** The pure conversions `toConnectivityLevel`, `toIsMetered`, and `toConnectivityStatus` live in `platform/windows/connectivity_hint_mapping.h/.cpp`, which exposes Windows SDK types, and are unit tested directly.

**Rationale:** The mapping is the part most likely to be wrong and is impossible to exercise from a host integration test, because the host's connectivity state cannot be forced. Keeping it separate keeps `connectivity_collector.h` free of Windows types. The tests live in the separate `tests/platform/` target (`system_monitor_platform_tests`), which may include Windows SDK headers but makes no API calls, so `tests/unit/` stays free of Windows headers (resolving [known deviation D-4](known_deviations.md#d-4-a-unit-test-includes-windows-sdk-headers)). A separate target was chosen over mirroring SDK constants as plain integers, which would lose the compile-time link to the real SDK values. Note that `netioapi.h` must be reached through `<iphlpapi.h>`, and `CancelMibChangeNotify2` is only declared when `<ws2tcpip.h>` is included first.

---

### DC-42: System uptime: `GetTickCount64` as a fast collector with an optional snapshot field

**Decision:** `UptimeCollector` returns `std::chrono::milliseconds` from `GetTickCount64` and runs on the scheduler tick as a fast collector. `SystemSnapshot::uptime` is `std::optional<std::chrono::milliseconds>`, empty when no uptime collector is registered.

**Rationale:** `GetTickCount64` is a trivial, non-failing call with a 64-bit counter, so unlike `GetTickCount` it does not wrap after 49.7 days. Its 10–16 ms resolution is irrelevant for displaying uptime. It counts time spent in sleep and hibernation, which is what users expect from "uptime". `QueryUnbiasedInterruptTime` was rejected because it excludes sleep time. The snapshot field is optional so a missing collector is explicit rather than a zero uptime (per the domain rules). Uptime is computed from a monotonic tick counter, not by subtracting a boot timestamp from wall-clock time, so it is unaffected by clock changes.

---

### DC-43: No zero placeholders for CPU and memory: optional samples end to end

**Decision:** `ICpuCollector` and `IMemoryCollector` return `std::optional<CpuSample>` / `std::optional<MemorySample>`, and `SystemSnapshot::cpu` / `::memory` are optional. `CpuCollector` returns `std::nullopt` when both queries fail and on the sample that only establishes its baseline. `calculateCpuUsage` returns `std::nullopt` for non-positive elapsed time, any counter that went backwards, or zero elapsed CPU time. Per-core usages are all-or-nothing: an empty `coreUsagePercents` means "unavailable" (after a core-count change or an uncomputable core), never zero-filled. `calculateMemorySample` returns `std::nullopt` for a zero physical-memory total, and `MemoryCollector` returns it when `GlobalMemoryStatusEx` fails. Likewise, `calculateDiskSample` returns `std::nullopt` for a zero-capacity volume and `DiskCollector` skips it, as it already skipped volumes whose query fails ([D-8](known_deviations.md#d-8-zero-capacity-disk-volumes-report-0-usage)). The dashboard shows "N/A" for missing values. Resolves [known deviation D-7](known_deviations.md#d-7-memory-and-cpu-data-report-zeros-when-missing).

**Rationale:** The domain rules forbid representing missing data as zero. A 0% CPU or 0 GiB memory reading is indistinguishable from a real idle system or a broken query, and it would also mislead the upcoming threshold-based health status (a missing sample must not read as "healthy"). A genuine 0% (a fully idle interval) is still reported as 0%. Rejecting deltas across a counter reset also fixes a latent bug: if only one counter went backwards, the old saturating subtraction could report a meaningless rate such as 100%. Per-core values are all-or-nothing because a partially filled list would misalign core indices.

---

### DC-44: Optional collections: a failed query is not an empty result

**Decision:** `IDiskCollector`, `INetworkCollector`, and `IProcessCollector` return `std::optional<std::vector<...>>`, and `SystemSnapshot::disks`, `::networks`, and `::processes` are optional. `std::nullopt` means no collector is registered or the query failed. An empty vector means the query succeeded and found nothing. `DiskCollector` returns `std::nullopt` when `GetLogicalDrives` fails, and also when fixed drives exist but none of them could be read. `NetworkCollector` returns it when `GetIfTable2` fails. The dashboard shows "N/A" for missing network data and reserves "No active adapter" for a successful read with no hardware adapter Up.

**Rationale:** With plain vectors a failed query looked the same as a machine with no volumes or no adapters, and the network card stated "No active adapter" when nothing had been observed. That is the same missing-data-as-default problem that D-6, D-7, and D-8 removed for single values ([code review F-12](code_reviews/phase_2.md#f-12-empty-disk-and-network-vectors-hide-collector-failures)). `IProcessCollector` has no implementation yet but takes the same shape now, so the Phase 4 process collector starts with it. A partial disk result, where some volumes are readable, is still reported as a vector, because the readable volumes are real data. The unreadable ones are skipped and logged, as before.

---

### DC-45: Health evaluation: domain types, a pure evaluator in `monitoring`, and the result carried in the snapshot

**Decision:** `domain/health_status.h` defines `HealthLevel` (Unknown, Healthy, Warning, Critical) and `SystemHealth` (cpu, memory, disk, network, overall). `monitoring/health_evaluator.h` provides the pure function `evaluateHealth(snapshot, HealthThresholds)`, and `SamplingScheduler` calls it at the end of each `sampleOnce()` to fill `SystemSnapshot::health`. Thresholds are strictly "above": warning above 90% and critical above 95% for memory and disk. CPU is critical only above 98%, because short full-load bursts are normal. Disk health is the worst fixed volume. Network health comes from connectivity: InternetAccess is Healthy, any lesser level is Warning. Overall is Critical if any component is Critical, else Warning if any is Warning, else Unknown if any is Unknown, else Healthy. Thresholds are set on the scheduler (`setHealthThresholds`, stopped-only like collector registration) so the Phase 6 settings file can supply them later.

**Rationale:** The architecture places the "health/connectivity evaluator" in `monitoring`, and `domain/AGENTS.md` lists health states as domain types. The domain target is header-only, so it holds only the types. The UI may not depend on `monitoring`, so it cannot call the evaluator. Carrying the result in the snapshot keeps the UI a pure renderer and makes health available to future consumers (tray icon, alerts) without duplicating logic. Unknown is a first-class level so missing data is never reported as Healthy, but a known problem still surfaces when other components lack data. Health is evaluated from each instantaneous sample with no smoothing or hysteresis, so CPU can flicker between levels under bursty load. Sustained-threshold rules belong with the planned alert rules (see the roadmap's alert-rules section).

---

### DC-46: Dashboard cards: `ResourceCard` widgets fed by pure formatting helpers

**Decision:** The dashboard is an overall "System status" line plus five `ResourceCard` widgets (CPU, memory, disk, network, uptime) in a three-column grid with equal column stretch. Each card has a title, a large value, a detail line, a colour-coded health status label (hidden for uptime, which has no health dimension), and a fixed-height slot reserved for a mini sparkline. All text comes from pure functions in `ui/dashboard_formatting.h`, which are unit tested without widgets. Before the first snapshot a card shows "--". When a snapshot lacks the data it shows "N/A", never a zero. Specific rules:
- Byte quantities use binary (IEC) units (KiB, MiB, GiB).
- Disk shows the fullest volume.
- Network shows total throughput over hardware adapters that are Up. Virtual adapters (VPN tunnels, virtual switches) are left out because their traffic also crosses a hardware adapter. It shows "N/A" if any active hardware adapter has no rate yet, because a partial sum would understate traffic, and "No active adapter" when no hardware adapter is Up. Connectivity is the detail line.
- String literals are ASCII only (no arrows or middle dots). Targets that link Qt compile with `/utf-8`, which Qt adds to its usage requirements, but `domain` and `platform/windows` do not link Qt, so a non-ASCII literal would be read in the system code page there. Non-ASCII text in code is written as escapes or code units.

**Rationale:** A reusable card keeps the five resources visually consistent and reserves room for the upcoming mini sparkline so adding it does not re-lay out the dashboard. Keeping formatting and aggregation in pure functions keeps `DashboardView` thin and makes edge cases (unit boundaries, missing rates, no volumes) testable. Health comes precomputed in `SystemSnapshot::health`, so the UI only renders it, in line with `src/ui/AGENTS.md`. Binary units match how Windows reports memory and disk sizes. Other tabs are still placeholders, so snapshot forwarding to them remains deferred (code review F-4).

---

### DC-47: `RingBuffer<T>` lives in `domain` so the UI can use it

**Decision:** Move `RingBuffer<T>` from `monitoring` to `domain` (`domain/ring_buffer.h`, namespace `sysmon::domain`) with no change to its behaviour. The dashboard's mini sparklines keep their own short presentation history in `RingBuffer`s owned by the UI. Longer histories (e.g. for the Performance view or alert rules) may still live in `monitoring`, using the same type.

**Rationale:** The UI must not depend on `monitoring` (`src/ui/AGENTS.md`), but sparklines need a bounded history with the contiguous-span access `RingBuffer` was designed for. `RingBuffer` is header-only and uses only the standard library, so it already meets the domain's dependency rules, and it had no production users, so the move is cheap. Alternatives considered:
- A UI-only `std::deque` history would duplicate `RingBuffer` with a less efficient design.
- Carrying history inside every `SystemSnapshot` is cheap for the dashboard but grows with Phase 3's longer, per-core histories.
- A new header-only `src/common/` module is semantically cleaner, since `domain` otherwise holds data types. It is deferred until a second shared utility justifies a new module; see the "Code organization" item in the roadmap.

---

### DC-48: Minimal `SparklineWidget` built early for the dashboard, extended in Phase 3

**Decision:** Build a minimal single-series `SparklineWidget` in `src/ui/charts/` (its own `sysmon_ui_charts` library, linked by `sysmon_ui`) for the Phase 2 dashboard, rather than deferring mini sparklines to Phase 3. It has these properties:
- **Geometry:** a pure `sparklineSegments()` function maps samples to polyline segments and is unit tested without painting. The newest sample sits at the right edge, so a partial history grows in from the right.
- **Missing data:** samples are `std::optional<float>`, and a missing sample breaks the line (a visible gap) instead of plotting zero.
- **Y range:** fixed (0–100% for CPU, memory, and disk) or auto-scaled to the data with 10% headroom (network).
- **Ownership:** the caller owns the history. `DashboardView` keeps a 60-sample `domain::RingBuffer<std::optional<float>>` per card (one minute at 1 Hz) and passes a view of it with `setSamples()` each tick. The network sparkline charts combined in + out throughput.
- **Colour:** the line is a fixed neutral blue, not the system accent colour. On systems with a red accent, the accent would read as the "Critical" status colour. (Phase 3 replaced this blue with the first slot of a theme-aware series palette; see [DC-52](#dc-52-sparkline-series-colours-a-validated-five-hue-palette-without-status-hues).)

**Rationale:** Roadmap item "each card shows a mini sparkline" belongs to Phase 2. The minimal widget covers exactly what the dashboard needs, and Phase 3 extends it (grid, multiple series, axis labels, annotations) rather than replacing it. Rendering from a caller-supplied view keeps the widget reusable for the Performance view's longer histories. Gaps follow the project rule against representing missing data as zero. Copying 60 samples per chart per tick is negligible.

---

## Phase 3 — Sparkline charts and performance views (`src/ui/charts/`, `src/ui/`)

### DC-49: Sparkline grid scrolls with a caller-supplied sample index

**Decision:** `SparklineWidget::setSampleIndex()` takes the running index of the newest sample (e.g. the number of samples recorded so far), and the pure function `verticalGridLines()` places vertical grid lines on every `capacity / 10`th sample of that sequence. Each new sample moves every line one slot to the left, so the grid scrolls with the data as in Task Manager. Horizontal lines are fixed at quarters of the Y range. The grid, like the other decorations, is off by default.

**Rationale:** Once the history is full, the samples span passed to the widget keeps the same length, so the widget cannot tell from the samples alone that time has moved on. Counting `setSamples()` calls inside the widget would break as soon as a series is set twice per tick or the widget is refreshed for other reasons, so the caller passes the index explicitly. Deriving the spacing from the capacity keeps about ten columns whether the chart shows 60, 300, or 1800 samples. Lines are computed with modular arithmetic on the running index, so a short history (index smaller than the capacity) does not underflow. `RingBuffer` was left unchanged; a caller that needs the index keeps its own counter.

### DC-50: Multiple series share one time axis and Y range, with series 0 as the primary

**Decision:** A `SparklineWidget` holds one or more series (`setSeriesCount()`, `setSeriesSamples()`), each with its own colour, optional legend label, and fill flag. All series share the capacity, time axis (newest sample at the right edge), and Y range; the auto range covers every series. Series 0 is the *primary* series: `setSamples()` / `samples()` are shortcuts for it, only it is filled by default, it is painted on top, and the min/max annotations describe it. The existing single-series API is kept, so the dashboard did not change.

**Rationale:** The roadmap asks for several series on one chart (per-core CPU, and network receive/send). One Y scale keeps the chart honest: series of different units or scales belong on separate charts, not a second axis. Filling only the primary series avoids stacked translucent fills turning into a muddy block. Min/max markers for every series would clutter a per-core chart, so they follow the one series the chart is about.

### DC-51: Axis labels and annotations reserve space outside the plot

**Decision:** Two optional decorations: *axis labels* (Y range min and max in a left gutter, the history window, e.g. "1 min", and "now" below the plot) and *annotations* (a line above the plot with the current value, the primary series' min and max, and dot markers at those points on its line). With two or more series and at least one label, the current-value area becomes a legend: a short line in the series colour, then the label and current value in the text colour. Values are formatted by a caller-supplied `ValueFormatter`, so the charts library stays independent of `ui`'s formatting helpers. A missing current value shows "N/A". `plotArea()` exposes the rectangle left for the series, and pure `summarizeSeries()` and `formatHistoryWindow()` functions carry the logic for unit tests. (The gutter's width can be fixed by label templates; see [DC-65](#dc-65-y-axis-gutter-sized-from-caller-supplied-label-templates-code-review-f-24). Annotations can have their own formatter; see [DC-67](#dc-67-chart-annotations-at-readout-precision-named-extremes-and-stable-volume-colours-code-review-f-26).)

**Rationale:** Drawing text over the plot would collide with the lines, so labels get their own bands and the plot shrinks instead. Keeping everything off by default lets the same widget stay a bare 36-pixel dashboard sparkline. Text stays in the palette's text colour (dimmed for secondary labels), never the series colour, so it is readable on any theme, and a series is identified by its colour key plus its label, never by colour alone. On ties, the newest occurrence is marked, which keeps the marker close to the live edge on flat data.

### DC-52: Sparkline series colours: a validated five-hue palette without status hues

**Decision:** Unless the caller overrides a series colour, series 0–4 take fixed hues in this order: blue, orange, aqua, violet, magenta (`defaultSeriesColor()`), with separate light and dark steps. The widget picks the dark steps when its background colour is dark, at paint time, so it follows theme changes. Series 5 and later share a muted grey instead of generated or repeated hues. The grid, frame, and secondary text use the palette's text colour at reduced opacity.

**Rationale:** The hues and steps come from a categorical palette whose order was checked with a colour-vision-deficiency validator: every adjacent pair stays separable under deuteranopia, protanopia, and tritanopia, and has enough contrast against the chart background, in both light and dark mode. Red, amber, and green were removed from that palette because they are the dashboard's Critical, Warning, and Healthy status colours, and a series in those hues would read as a status. The remaining five still pass after re-validation. In light mode, aqua and magenta fall below 3:1 contrast against the background, which is why labelled series always appear in the legend with their values. Repeating or generating hues past the fifth series would make series look alike, so more series (e.g. per-core CPU on a 16-core machine) get a shared de-emphasised grey. The caller can colour them explicitly, highlight a total in the primary colour, or split them across charts. Following the palette rather than the system accent colour keeps the reason for a fixed blue in [DC-48](#dc-48-minimal-sparklinewidget-built-early-for-the-dashboard-extended-in-phase-3): a red accent would read as Critical.

### DC-53: Performance view: a sidebar of resource pages that each own their history

**Decision:** `PerformanceView` is a sidebar (`QListWidget`) beside a `QStackedWidget` of four pages: CPU, memory, disk, and network. Each sidebar entry shows the page title and a short summary of the latest values. Each page derives from `PerformancePage` and owns the `RingBuffer` histories for its charts:
- **CPU:** a total utilization chart above a near-square grid of small per-core charts, one per logical processor, which scrolls vertically when the page is too small for it. When the number of cores in the samples changes, the grid is rebuilt and the per-core history restarts, because the core indices no longer line up.
- **Memory:** physical memory usage and commit charge as percentage charts. Commit charge is a percentage of the commit limit, and the limit can grow with the page file.
- **Disk:** one space-used chart with a labelled series per volume, in volume-name order.
- **Network:** one chart per adapter that is Up and can carry traffic: a hardware adapter, or a virtual adapter bound to IP (see [DC-59](#dc-59-network-page-charts-only-adapters-that-can-carry-traffic-networksamplehasipinterface-code-review-f-21)). Each chart has receive and send series on an auto-scaled byte-rate axis. Hardware adapters come first and are shown in a scroll area.

Disk volumes are keyed by volume name, and network adapters by `NetworkSample::interfaceId` (see [DC-64](#dc-64-network-adapters-identified-by-interface-luid-not-by-name-code-review-f-23)). A key that leaves the snapshots records gaps, and is dropped once its history holds no samples. An adapter still in the snapshot is kept even when it has no values, since a new adapter has no rate until its second sample. Network history is recorded for every adapter, including adapters that are down, so an adapter that comes up shows its history.

**Rationale:** Task Manager's layout (a resource list with one detail page at a time) gives each chart room. It also lets per-core charts use their own small multiples instead of 16 or more overlapping series, which would run out of distinguishable colours after five (see [DC-52](#dc-52-sparkline-series-colours-a-validated-five-hue-palette-without-status-hues)). The histories stay in the UI, as the dashboard's do, because the UI may not depend on `monitoring`. Keeping each history next to the page that draws it keeps `SystemSnapshot` a per-tick value. Keying by name or ID rather than position keeps a volume's or adapter's line continuous when others appear or disappear.

### DC-54: Snapshot forwarding: every view records every snapshot, and hidden views skip repainting (code review F-4)

**Decision:** `MainWindow::onSnapshotReady` forwards every snapshot to the dashboard and the performance view, whichever tab is current. Recording and display are separate steps. `PerformanceView::updateSnapshot` always appends to each page's history. It refreshes widgets only while the view is visible, and then only for the selected page and the sidebar. `showEvent` and a page change refresh from the recorded history. The Processes and Network views will follow the same pattern when they are implemented.

**Rationale:** Code review F-4 proposed updating only the visible tab. For views with history, that would leave gaps covering the time another tab was open, so the recording step must always run. Recording is only a few `RingBuffer` pushes per tick. The costs worth skipping are copying up to thousands of samples per chart into the widgets and painting them, and those are skipped while the view is hidden. Deciding visibility inside each view, rather than in `MainWindow`, keeps the rule next to the state it protects and also covers pages that are not selected within the view.

### DC-55: CPU base speed from `CallNtPowerInformation`, read once; no current clock speed

**Decision:** `CpuSample::baseSpeedMhz` carries the processor's rated clock speed. `CpuCollector`'s default constructor reads it once with `CallNtPowerInformation(ProcessorInformation)`, using a buffer sized for all active logical processors (`GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`), and reports the highest `MaxMhz` (`baseSpeedFromMaxMhz`). The value is `std::nullopt` when the call fails or every processor reports 0, as some virtual machines do. It feeds the Performance tab's "Base speed" readout. There is no current-speed readout.

**Rationale:** The rated speed does not change while the system runs, so one call at startup is enough. The highest value makes a hybrid processor report its performance cores. A current-speed readout was rejected: on the development machine the same structure's `CurrentMhz` equalled `MaxMhz` (4700 MHz) at idle, which is a known limitation of the field on current processors. Task Manager computes its "Speed" from the "% Processor Performance" counter, which would need PDH, and PDH is not used for core metrics ([DC-02](#dc-02-core-metrics-direct-win32-apis-over-pdh)). Showing no current speed is better than showing a wrong one. `PROCESSOR_POWER_INFORMATION` is documented but missing from the SDK headers, so `cpu_collector.cpp` declares it, as it already does for `SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION`.

### DC-56: Memory detail and system activity counts from `GetPerformanceInfo`, as fast collectors

**Decision:** `GetPerformanceInfo` supplies the Performance tab's memory detail and its process, thread, and handle counts:
- `MemoryCollector` calls it after `GlobalMemoryStatusEx` and adds `systemCacheBytes` (`SystemCache`; see [DC-63](#dc-63-memory-readouts-name-what-getperformanceinfo-reports-not-task-managers-figures-code-review-f-22)), `pagedPoolBytes`, and `nonPagedPoolBytes` (page counts times `PageSize`) to `MemorySample` as optional fields. If only this call fails, or the page size is 0, the memory sample is still reported, with these fields empty.
- A new `SystemActivityCollector` reports `SystemActivitySample` (process, thread, and handle counts) in the new `SystemSnapshot::activity` field. It returns `std::nullopt` on failure or for a process count of 0.

Both run on the scheduler tick as fast collectors. The call is wrapped once in `performance_info_reader.h`, and each collector has its own reader, and so its own `RepeatedFailureLog`.

**Rationale:** On the development machine (367 processes, about 200,000 handles) `GetPerformanceInfo` took about 19 µs per call, so it reads kernel counters rather than enumerating processes, and it fits the fast-collector budget. It is documented, has no registry or PDH dependency, and provides exactly the counts Task Manager shows, so it follows the direct-Win32 rule for core metrics. `GlobalMemoryStatusEx` stays the source of the core memory figures, as the rules require. Counting processes, threads, and handles from the Phase 4 `SystemProcessInformation` enumeration was rejected: it runs every 2–5 s on the slow-collector thread and does not exist yet, and these readouts should update at 1 Hz. The Phase 4 process list may still report its own totals. Calling `GetPerformanceInfo` from two collectors costs a second 19 µs call per tick. In return, memory detail stays in `MemorySample` and the counts get their own collector, and each collector remains independently testable. Merging them into one collector would either put process counts in a memory type or split one query's results across snapshot fields inside the scheduler.

### DC-57: Performance readouts: pure builders with stable names, shown in a `ReadoutGrid`

**Decision:** Each page shows numeric readouts next to its charts, in a `ReadoutGrid` with a dimmed name above each value:
- **CPU:** utilization, base speed, logical processors, processes, threads, handles, and up time.
- **Memory:** used, available, total, committed, system cache, paged pool, and non-paged pool (see [DC-63](#dc-63-memory-readouts-name-what-getperformanceinfo-reports-not-task-managers-figures-code-review-f-22)).
- **Disk:** used, total, and free space, one readout per volume.
- **Network:** under each adapter's chart, receive, send, link speed, type (hardware or virtual), adapter description, IPv4 and IPv6 addresses, DNS servers, and bytes received and sent.

The text comes from pure functions in `ui/performance_readouts.h` (`cpuReadouts()`, `memoryReadouts()`, `diskReadouts()`, `adapterReadouts()`). They return the same names in the same order whether or not the data is available, and "N/A" for missing values, including a link speed of 0. Readouts show "--" until the first snapshot, as the dashboard does. `ReadoutGrid::setReadouts()` updates the value labels in place while the names are unchanged, and rebuilds its cells only when they change (for example, a volume is added). Counts use comma thousands separators, clock speeds are in GHz, and link speeds are in decimal bits per second.

**Rationale:** Keeping the names fixed means a missing value does not change the layout, and the missing value is visible rather than quietly removed. Pure builders keep formatting and edge cases testable without widgets, as `dashboard_formatting.h` does for the dashboard. Updating labels in place avoids rebuilding widgets every second. Counts are grouped with a fixed separator rather than with `QLocale`, so tests do not depend on the machine's locale. Localized number formatting can come with the Phase 6 display-unit settings. Link speeds use decimal units because network rates are conventionally quoted that way, while byte quantities keep the binary units used elsewhere.

### DC-58: Performance pages fit an 800×600 window: re-flowing readouts and a scrolling per-core grid (code review F-20)

**Decision:**
- **Readouts:** `ReadoutGrid` lays out its labels with its own `QLayout` (`ReadoutLayout`, in `readout_grid.cpp`). The constructor's column count is a maximum: the layout uses as many columns as fit the width, reports a height for width, and has the widest readout as its minimum width. Spare width is shared so that columns are equally wide where their content allows. Each cell remembers the widest it has been since the names last changed, so a value that shrinks again does not bring a column back.
- **Scrolling:** the network page's scroll area, and a new scroll area around the CPU page's per-core grid, scroll vertically only. Per-core charts have a 12×12 minimum, so the 16 columns of a 256-core grid fit the page width.
- **Window size:** `MainWindow` opens at 1024×720, limited to 90% of the screen's available area. 800×600 is still the size the content must fit, and widget tests check it with 256 cores and two adapters that have long addresses and descriptions.

**Rationale:** A `QLabel` without word wrap cannot be narrower than its text. A fixed five-column grid of IPv6 addresses and adapter descriptions was 875 px wide on a 535 px page, so every adapter's readouts needed sideways scrolling. Doing the re-flow in a layout, rather than in `resizeEvent()` as the review suggested, means Qt's height-for-width handling sizes the scroll area's content and the pages' box layouts correctly. The labels are only moved, never recreated, so values are still updated in place. Wrapping or eliding values was rejected, because addresses must stay whole to be read and copied. Giving long values their own full-width rows was also rejected: it would be a special case inside a generic grid, while re-flow already gives long values fewer columns and short readouts more when there is room. Without the remembered widths, a transfer rate near a column boundary would change the column count every second.

The per-core grid scrolls instead of shrinking its charts further, because a chart only a few pixels tall shows nothing. Its minimum width still matters: a scroll area with horizontal scrolling turned off squeezes content that is too wide rather than widening the window, and it shows no scroll range, so the tests compare the content's minimum width with the viewport. At the larger default size, the network readouts usually get two or more columns instead of one. Limiting it to the screen keeps the window on screen on a 1366×768 laptop.

### DC-59: Network page charts only adapters that can carry traffic: `NetworkSample::hasIpInterface` (code review F-21)

**Decision:** `NetworkSample` has a `hasIpInterface` flag. The collector sets it when `GetAdaptersAddresses` lists the adapter, which it does only for adapters bound to IP. `chartedAdapters()` charts an Up adapter only if it is hardware or has an IP interface. Virtual adapters without one stay in the snapshots, and their history is still recorded.

**Rationale:** Windows keeps several hidden virtual adapters Up permanently, such as WAN Miniport (IP), (IPv6) and (Network Monitor). They have no IP interface and never carry traffic, yet on the review machine they took three of the network page's five charts. `MIB_IF_ROW2` has no "hidden" flag, and `ConnectorPresent` is false for the WAN Miniports and for VPN tunnels alike, so `GetIfTable2` cannot tell them apart. IP binding can, and the collector already correlates each row with `GetAdaptersAddresses`. The review also suggested testing `ipAddresses` for emptiness. That gives the same result in practice, because link-local addresses are kept, but an explicit flag states the reason, does not depend on an adapter having an address yet, and gives the Phase 5 network view the same distinction. Hardware adapters are charted without an IP interface, because their link can still carry traffic. While `GetAdaptersAddresses` fails, every adapter has the flag cleared, so virtual adapters leave the page until the next successful read and hardware adapters stay. That is acceptable for a chart list.

### DC-60: Dense sparklines: min/max decimation, a one-pixel cosmetic line, and an envelope fill

**Decision:** A chart is *dense* when it has more slots (its capacity) than its plot has pixel columns, for example a 30-minute history on an 800-pixel chart or on a small per-core chart. For a dense chart, `SparklineWidget` changes three things:
- **Decimation:** `sparklineSegments()` keeps only each pixel column's lowest and highest point, in time order. A segment then has at most two points per column. Gaps still split segments.
- **Line:** it is drawn with `drawPolyline()` and a 1-pixel cosmetic pen instead of the 1.5-pixel pen with round joins.
- **Fill:** it follows `upperEnvelope()`, one point per column at the column's highest value, instead of the zigzag line.

Charts that are not dense look exactly as before.

**Rationale:** With 30 minutes of history, a Release build took about 210 ms per tick on the CPU page (16 cores) and 145 ms on the network page. That was measured with synthetic worst-case data that swings across most of the range every 20 samples, and it is far too long for a 1 Hz UI thread. Decimation alone barely helped, because the cost was not the number of points. It was Qt's general stroker, which builds a round-joined outline for every vertex of a pen wider than one pixel, and the scanline cost of filling a polygon whose top edge zigzags. Qt's raster engine has a fast path for cosmetic pens up to one pixel wide, which cut the worst page to about 60 ms. The envelope fill, which covers the same pixels as filling under the zigzag because the line is drawn over each column's span, brought it to about 27 ms (see the measurements in [DC-61](#dc-61-30-minutes-of-performance-history-a-chart-window-selector-and-the-measured-tick-cost)). Min/max decimation is kept because it bounds the work by the chart's width, and unlike averaging it keeps one-sample spikes visible. A one-pixel line also suits a dense chart better, because a thicker one would merge neighbouring columns.

### DC-61: 30 minutes of performance history, a chart-window selector, and the measured tick cost

**Decision:** Each Performance page keeps 30 minutes of history (`PerformancePage::kHistoryCapacity` = 1800 samples at 1 Hz). A "Chart history" selector below the sidebar sets how much of it the charts show: 1 minute (the default, as in Task Manager), 5 minutes, or 30 minutes. `showHistory()` passes the chart only the newest window of samples and sets the chart's capacity to match. The selection is not saved yet, because persisting it belongs with the Phase 6 settings file. Windows are counted in samples, which equals seconds at the fixed 1 Hz sampling rate. A configurable refresh interval will need to convert them.

The tick cost was measured with a throwaway benchmark (not committed; [DC-66](#dc-66-a-committed-tick-cost-benchmark-run-with-buildps1--benchmark-code-review-f-25) replaces it with a committed one and newer figures) in a Release build (`build.ps1 -Release`) on the development machine, a 16-thread 4.7 GHz desktop. The benchmark shows a 1280x800 window on the Performance tab and records 1800 synthetic snapshots. It then times 20 more snapshots, each forwarded by `MainWindow` and followed by a synchronous repaint of the window. The data is a worst case: every series swings across most of its range every 20 to 30 samples.

| Page | 1 min | 5 min | 30 min |
| --- | --- | --- | --- |
| CPU, 16 cores | 6.2 ms | 9.8 ms | 26.9 ms |
| CPU, 64 cores | 12.1 ms | 15.7 ms | 20.4 ms |
| Memory | 2.4 ms | 4.9 ms | 8.2 ms |
| Disk | 1.3 ms | 1.3 ms | 1.6 ms |
| Network, 2 adapters | 3.8 ms | 12.5 ms | 36.6 ms |

The 64-core CPU page is cheaper at 30 minutes than the 16-core one because its per-core charts are smaller. A Release build of the app run on the same machine used about 41 MiB of working set.

**Rationale:** The roadmap asks for smooth 1 Hz updates with 5 to 30 minutes of history. At the default window, every page costs under 13 ms per second, about 1% of the UI thread. Even the synthetic worst case at 30 minutes stays under 40 ms, one short frame per second, and real data rarely swings that far on every sample. Reaching this needed the dense-chart painting in [DC-60](#dc-60-dense-sparklines-minmax-decimation-a-one-pixel-cosmetic-line-and-an-envelope-fill). Before it, the 30-minute CPU page took about 210 ms per tick, and the Debug build was about three times slower still. The history always holds 30 minutes, so switching windows shows data at once rather than starting empty. It costs about 29 KB per series (the `RingBuffer` stores each sample twice), about 2 MB for a 64-core CPU page. Passing only the visible window keeps an auto-scaled axis, such as network throughput, fitted to what is on screen rather than to a spike that has scrolled out of view. A known side effect is that a volume or adapter that disappears stays in the chart's legend, with gaps, until its 30-minute history is empty.

### DC-62: `build.ps1`: launch only with `-Run`, configure only when needed, and parallel tests

**Decision:** `build.ps1` builds (and tests, with `-Test`) without launching the app; `-Run` launches it. It runs CMake's configure step only when the build directory has no `build.ninja`, has not been configured by the script yet (a `build_ps1_configure.stamp` file in the build directory records the last successful configure), or a presets file is newer than that stamp. `-Configure` forces it. CTest runs with `--parallel` set to the number of logical processors.

**Rationale:** Builds and test runs far outnumber launches, especially for agents, so launching is the opt-in. Measured on the 16-thread development machine, an edit to one UI file followed by `-Test` took about 26 s: about 3 s re-configuring although nothing had changed, about 4 s compiling and linking, and about 18 s of tests. `gtest_discover_tests` runs each of the 362 tests in its own process at about 50 ms each, so process startup dominates and the tests parallelize well. Together the two changes cut that cycle to about 8 s (tests about 4 s), and a build with nothing to do from about 5 s to under 2 s. Skipping configure is safe because Ninja re-runs CMake itself when a `CMakeLists.txt` changes, and the vcpkg toolchain adds `vcpkg.json` and `vcpkg-configuration.json` to `CMAKE_CONFIGURE_DEPENDS`. Ninja does not track the presets files, hence the stamp. `CMakeCache.txt` cannot serve as the stamp, because CMake rewrites it only when its contents change. Ten consecutive parallel runs passed. The integration tests' timing checks allow up to a second, and the one test that writes a file uses its own directory, so the tests do not interfere with each other.

### DC-63: Memory readouts name what `GetPerformanceInfo` reports, not Task Manager's figures (code review F-22)

**Decision:** The memory page's "Cached" readout is renamed "System cache", and `MemorySample::cachedBytes` is renamed `systemCacheBytes`. It still shows `PERFORMANCE_INFORMATION::SystemCache`, and the domain comment now says what that is without claiming it matches Task Manager. "In use" is renamed "Used". It is still total minus available, so it includes the modified page list. The dashboard ("… of … used") and the sidebar summary already used that wording or no label, and are unchanged.

**Rationale:** Task Manager's "Cached" is the standby list plus the modified list, and its "In use" leaves the modified list out. `SystemCache` is neither. Microsoft documents it as the standby list plus the system working set, but it does not measure as that either. Two readings on the development machine, taken from the memory-list performance counters for comparison only:

| Reading | `SystemCache` | Standby | Standby + modified (Task Manager's "Cached") | Standby + system working set |
| --- | --- | --- | --- | --- |
| Code review F-22 | 30.59 GiB | 33.83 GiB | 34.37 GiB | 35.09 GiB |
| This fix | 34.84 GiB | 34.25 GiB | 34.42 GiB | 35.31 GiB |

The difference from Task Manager was 3.8 GiB in the first reading and 0.4 GiB in the second, and in different directions, so no correction can be applied to it. Reproducing Task Manager's figures needs the exact list sizes. `NtQuerySystemInformation(SystemMemoryListInformation)` provides them but requires `SeProfileSingleProcessPrivilege`, which unelevated users do not have. The `\Memory\Standby Cache*` and `\Memory\Modified Page List Bytes` counters would need PDH, which is not used for core metrics ([DC-02](#dc-02-core-metrics-direct-win32-apis-over-pdh)). The readout is kept rather than dropped because the size of the system cache is still useful, and the new name no longer promises a figure the tool cannot provide. "Used" is the plain description of total minus available. If elevated access is added (see "Elevated process termination" in the post-MVP roadmap), an elevated component could read `SystemMemoryListInformation` and provide Task Manager's breakdown.

### DC-64: Network adapters identified by interface LUID, not by name (code review F-23)

**Decision:** `NetworkSample::interfaceId` carries the key the collector already uses for rate baselines: the interface LUID, or the interface index when Windows reports no LUID, or a hash of the name when it reports neither. `NetworkPerformancePage` keys its histories and charts by it, and uses the names only for display. A renamed adapter keeps its chart and history, and its title changes.

**Rationale:** The page used to key by `adapterName`, which is the friendly name, the alias, or the description. Descriptions repeat ("WAN Miniport (IP)", "Microsoft Wi-Fi Direct Virtual Adapter"), and two adapters with the same name pushed into one history, which then held two samples per tick and drew both adapters as one line. Renaming a connection also restarted its history. The LUID is unique per interface and does not change on rename, and it was already computed at the platform boundary, so passing it on costs one field. It is an opaque `uint64_t` in the domain, so no Windows type crosses the boundary. The Phase 5 network view needs a stable adapter identity as well.

### DC-65: Y-axis gutter sized from caller-supplied label templates (code review F-24)

**Decision:** `SparklineWidget::setAxisLabelTemplates()` takes texts as wide as the widest Y-axis labels expected. The gutter is the widest of these templates and the current range's labels, so a label is never clipped. The network page gives its charts `chartByteRateAxisTemplates()`: the widest output of `formatChartByteRate()` in each unit up to GiB/s, produced by the formatter itself at one byte below each unit boundary (for example "1024.0 MiB/s"). Percentage charts need no templates, because their fixed range already gives constant labels.

**Rationale:** The gutter used to be as wide as the current range's labels ([DC-51](#dc-51-axis-labels-and-annotations-reserve-space-outside-the-plot)). On an auto-ranged chart the maximum label changes with traffic, and the review measured the plot's left edge moving by up to 19 pixels as a burst came and went. That reads as jitter on a chart that otherwise scrolls smoothly, and stacked network charts did not line up. Several templates are needed rather than one, because unit letters differ in width ("KiB/s", "MiB/s", "GiB/s"). Producing them from the formatter keeps them correct if the formatting changes. Measuring a few strings costs little next to painting the series. A formatter that reports its own widest output was rejected because `ValueFormatter` is a plain function, and the charts library would need a richer interface for a single caller. Units above GiB/s are left out: they would widen every network chart for rates above 1024 GiB/s, which no current adapter reaches, and such a label would still widen its gutter instead of being clipped.

### DC-66: A committed tick-cost benchmark, run with `build.ps1 -Benchmark` (code review F-25)

**Decision:** `benchmarks/performance_tick_benchmark.cpp` builds `system_monitor_benchmark`, an executable that is built with every preset but not registered with CTest. `build.ps1 -Benchmark` implies `-Release` and runs it after the build (and after the tests, with `-Test`). For each page it shows a 1280x800 `MainWindow` off screen (`WA_DontShowOnScreen`), records 1800 snapshots while the Performance tab is hidden, then selects the page and, for each chart window, times 20 ticks: `MainWindow::onSnapshotReady()` followed by `grab()` of the whole window. It prints the mean and maximum per tick as a Markdown table. The data is a fixed worst case: every series (total and per-core CPU, memory usage and commit, three volumes, receive and send for two adapters) is a sine wave between 5% and 95% of its range, with a period of 20 to 30 samples that differs per series. Throughput peaks at 100 MiB/s.

One run on the development machine (16 threads, 4.7 GHz), mean with maximum in parentheses. Over three runs, every mean stayed within 3 ms of these:

| Page | 1 min | 5 min | 30 min |
| --- | --- | --- | --- |
| CPU, 16 cores | 7.6 ms (9.6) | 12.5 ms (13.5) | 39.1 ms (40.1) |
| CPU, 64 cores | 14.3 ms (15.3) | 18.9 ms (20.3) | 34.1 ms (35.0) |
| Memory | 4.0 ms (4.7) | 9.5 ms (10.2) | 22.9 ms (24.9) |
| Disk, 3 volumes | 4.5 ms (5.0) | 23.8 ms (24.6) | 38.7 ms (48.6) |
| Network, 2 adapters | 4.9 ms (5.4) | 11.3 ms (12.3) | 13.3 ms (18.0) |

**Rationale:** The table in [DC-61](#dc-61-30-minutes-of-performance-history-a-chart-window-selector-and-the-measured-tick-cost) came from a benchmark that was not committed, so it could not be reproduced, and the review's rebuilt benchmark differed from it by up to 2x. That benchmark had already caught a 210 ms tick, so later changes to charts and pages need the same tool, and the Phase 4 process table will be the next large cost on the UI thread. An executable outside CTest suits timings, which depend on the machine and would make a flaky pass/fail test. Building it with every preset keeps it compiling in CI without running it. Recording while the tab is hidden uses the recording-only path ([DC-54](#dc-54-snapshot-forwarding-every-view-records-every-snapshot-and-hidden-views-skip-repainting-code-review-f-4)), so the 1800-sample warm-up takes seconds. Printing through spdlog follows the logging rule.

The new numbers support DC-61's conclusions with slightly higher figures. A tick at the default 1-minute window costs at most about 15 ms (14 ms for 64 cores, against "under 13 ms"). The worst case at 30 minutes averages about 40 ms, with single ticks up to about 49 ms. That is still one short frame per second. Individual pages differ from DC-61 because the data differs: every series here swings, including all three disk volumes, which real disk usage never does, so the disk page is far slower than DC-61's 1.6 ms. The network page is faster than DC-61's 36.6 ms. The throwaway benchmark's data is not recorded in enough detail to say why, which is the reason to commit this one.

### DC-67: Chart annotations at readout precision, named extremes, and stable volume colours (code review F-26)

**Decision:**
- **Annotation precision:** `SparklineWidget::setAnnotationFormatter()` formats the annotations (current value, min, and max) separately from the axis labels, and defaults to the value formatter. `PerformancePage::createHistoryChart()` takes both formatters. Percentage charts label the axis in whole percent ("8%", `formatChartPercent()`) and annotate with one decimal ("7.5%", `formatPercent()`), as the readouts do.
- **Named extremes:** when a chart has a legend and its primary series has a label, the min/max text names it: "Receive min 1.0 KiB/s  max 4.0 KiB/s". Otherwise it stays "Min …  Max …". `extremesText()` exposes the text for tests.
- **Volume colours:** `setSeriesColorSlot()` gives a series the default colour of another palette slot, still following the light or dark surface. `DiskPerformancePage` gives each volume the lowest free slot when it first appears and keeps it while the volume has history. Series stay in volume-name order.
- **Link speeds:** `formatLinkSpeed()` rounds to three significant digits before choosing the unit, and has a Tbps unit, so "1e+03 Mbps" becomes "1 Gbps".

**Rationale:** The annotation and the readout below it showed the same value at different precision ("8%" and "7.5%"), which looks like a disagreement. Whole percentages still suit the axis, where they are scale marks rather than readings, so the two get separate formatters instead of one shared precision. Byte rates already have one decimal and use the same formatter for both. The extremes describe series 0 only ([DC-50](#dc-50-multiple-series-share-one-time-axis-and-y-range-with-series-0-as-the-primary)), and with a legend nothing said so. Naming the series fixes that and keeps the extremes, which the network charts' Receive series needs, instead of dropping them from multi-series charts. Colours used to follow sort position, so a new volume changed the colour of every volume after it. A fixed `setSeriesColor()` override would lose the light and dark steps of [DC-52](#dc-52-sparkline-series-colours-a-validated-five-hue-palette-without-status-hues), so the widget takes a palette slot instead. Reusing the lowest free slot keeps colours within the five hues while there are at most five volumes. `QString::number(…, 'g', 3)` switches to an exponent once rounding reaches 1000, and choosing the unit after rounding avoids that at every boundary.

### DC-68: Shared UI constants and helpers: where each one lives (code review F-28)

**Decision:**
- `charts::kPercentRange` sits next to `YRange` in `sparkline_widget.h`.
- `kNotAvailable` ("N/A") and `usedMemoryBytes()` are in `dashboard_formatting.h`, which the dashboard and the Performance tab's helpers already include.
- `view_metrics.h` names the margins, spacing and title size the views share (`kViewMargin`, `kSectionSpacing`, `kItemSpacing`, `kTitlePointSize`). Values used by one file stay local named constants there, such as `kHistoryChartMinimumHeight` in `performance_page.cpp`.
- `chartedAdapters()` orders names with `QString::compare(…, Qt::CaseInsensitive)`.

**Rationale:** Each value had several copies that could drift apart. Moving them into an existing header that every user already includes avoids a new header for one or two declarations. `dashboard_formatting.h` already holds the formatting both tabs share (`formatPercent()`, `formatBytes()`). The charts library keeps its own "N/A", because `ui/charts` must not depend on `ui` ([DC-51](#dc-51-axis-labels-and-annotations-reserve-space-outside-the-plot)). One `usedMemoryBytes()` gives the dashboard card, the sidebar summary and the "Used" readout the same definition, which [DC-63](#dc-63-memory-readouts-name-what-getperformanceinfo-reports-not-task-managers-figures-code-review-f-22) documents. The layout metrics get their own header because no existing one is about layout, and sharing them keeps the tabs aligned when one changes. `localeAwareCompare()` made a pure function's result depend on the machine's locale, which the readouts already avoid for counts ([DC-57](#dc-57-performance-readouts-pure-builders-with-stable-names-shown-in-a-readoutgrid)). Case-insensitive comparison gives the same order for adapter names, which are almost always ASCII.

### DC-69: `build.ps1` retries a failed configure once from a clean cache

**Decision:** When `cmake --preset` fails and the build directory already had a `CMakeCache.txt`, `build.ps1` warns, removes `CMakeCache.txt` and `CMakeFiles`, and configures once more. `vcpkg_installed` and the Ninja build outputs are kept. A failure without an existing cache, or a second failure, stops the script as before. The configure stamp ([DC-62](#dc-62-buildps1-launch-only-with--run-configure-only-when-needed-and-parallel-tests)) is removed before configuring and written only after success, so a failed configure is always repeated on the next run.

**Rationale:** After a Visual Studio update, the Release build directory's cache recorded a compiler that no longer matched the preset's `cl`. CMake deleted the cache and re-ran itself, the re-run could not find vcpkg's packages, and the cache it left behind failed every later configure in the same way, including with `-Configure`. Deleting the two cache entries by hand fixed it. A cache created for another source directory (a moved or copied checkout) fails in a similar way, and the retry was tested with that case. The presets define every cache variable and the script passes none of its own, so a clean cache loses nothing. Keeping `vcpkg_installed` avoids restoring dependencies again, and in the test Ninja found nothing to rebuild afterwards. Retrying only once, and only when a cache existed, means a genuine error in a `CMakeLists.txt` still fails, after one extra configure of a few seconds. Deleting the whole build directory was rejected, because it would force a full rebuild and a dependency restore for a problem confined to the cache.

---

## Phase 4 — Process collector (`src/platform/windows/`)

### DC-70: Process list read into a kept, growing buffer through an injected query

**Decision:** `SystemProcessInformationReader` calls `NtQuerySystemInformation(SystemProcessInformation)`, resolved from `ntdll.dll`, in a resizing-buffer loop. When the call returns `STATUS_INFO_LENGTH_MISMATCH` or `STATUS_BUFFER_TOO_SMALL`, the buffer grows to the reported size plus an eighth (or doubles when no size is reported) and the call is repeated, up to five calls in all. The buffer starts at 256 KiB, is kept between reads, and never shrinks. A reported size above 64 MiB fails the read instead of being allocated. The query is a `std::function` that takes a `std::span<std::byte>` and returns the NTSTATUS as an `int32_t`, so unit tests can fake it without Windows headers. `resolveSystemProcessInformationQuery()` returns the real one.

**Rationale:** The list's size is not known in advance and changes between calls, so the API reports the size it needs and the caller retries. The headroom covers processes and threads created between the failed call and the retry, so one retry is normally enough. A desktop's list is a few hundred KiB: 256 bytes per process, 80 per thread, plus the image names. Keeping the buffer therefore costs little and saves an allocation and a failed call on every refresh. The 64 MiB cap is far above any real list, but it stops a corrupt reported size from exhausting memory, which would end the process because exceptions are not used. With the fake query, unit tests cover retry paths that a real machine rarely takes, as [architecture.md — Testing strategy](architecture.md#testing-strategy) asks for buffer resizing. An integration test starts the real query from a 1 KiB buffer to force the loop.

---

### DC-71: Process list parsed through a full `SYSTEM_PROCESS_INFORMATION` layout checked against `winternl.h`

**Decision:** `platform/windows/system_process_information.h` defines `FullSystemProcessInformation`, the complete entry layout with the field names Windows uses. `static_assert`s tie its size and field offsets to the partial `SYSTEM_PROCESS_INFORMATION` in `winternl.h`. `parseSystemProcessInformation()` follows `NextEntryOffset` from the start of the buffer. It copies each entry out with `memcpy` and turns it into a `ProcessInfo`, and returns `std::nullopt` if an entry does not fit in the buffer. Image names are read with their `Length` (they need not be null-terminated), and only when they lie inside the buffer; otherwise the name is empty. The field mapping:

- `privateBytes` is `PrivatePageCount`, the commit charge (Process Explorer's "Private Bytes", Task Manager's "Commit size").
- `ioReadBytes` and `ioWriteBytes` are `ReadTransferCount` and `WriteTransferCount`. They count all of the process's read and write I/O, including pipes and devices, not only disk. The domain field comments say so.
- CPU times are truncated from 100 ns units to whole milliseconds, the domain unit ([DC-14](#dc-14-cpu-times-stored-as-cumulative-milliseconds-not-percentages)).
- The creation time converts from the FILETIME epoch to `system_clock` with fixed arithmetic ([DC-08](#dc-08-processinfocreationtime-type-stdchronosystem_clocktime_point)).
- The Idle process (PID 0) has no image name in the list, and its `imageName` stays empty. Naming it is left to the UI.

UTF-16 to UTF-8 conversion moved to `platform/windows/utf8_conversion.h` as `wideToUtf8(std::wstring_view)`, written in portable C++. The network collector uses it too, in place of its own `WideCharToMultiByte` wrapper. Unpaired surrogates become U+FFFD, as with `WideCharToMultiByte`.

**Rationale:** `winternl.h` hides the creation time, user and kernel times, parent PID, and I/O counters in `Reserved` fields, and the collector needs all of them. A complete definition names them, and the `static_assert`s catch a layout mistake at compile time instead of as wrong numbers at run time. Copying each entry avoids reading through a cast pointer and any dependence on the buffer's alignment. The bounds checks stop a corrupt list from reading past the buffer. The parser's tests build lists from SDK types (`UNICODE_STRING`, `SYSTEM_THREAD_INFORMATION`), so they live in `tests/platform/`, which may not call Windows APIs, and the parser calls the conversion. A conversion in plain C++ keeps those tests free of OS calls and can be unit tested directly. `PrivatePageCount` is the field `VM_COUNTERS_EX` calls `PrivateUsage`, the documented commit charge. The transfer counts are the only I/O byte counters the list has, so the comments describe what they include rather than calling them disk I/O.

---

### DC-72: `ProcessDetailsStatus` replaces `ProcessInfo::accessDenied`

**Decision:** `ProcessInfo::accessDenied` is replaced by `ProcessDetailsStatus detailsStatus`, which says whether the image path and command line could be read and, if not, why:

- `NotQueried` (the default): the process is never opened (`lsass.exe`, `csrss.exe`), or the `ProcessInfo` did not come from a collector that reads details.
- `Available`: both details were read.
- `AccessDenied`: Windows denied opening the process or reading a detail.
- `Unavailable`: a detail could not be read for another reason, such as a process that exited before it was queried, or a kernel process with no image file or command line (System, Registry).

A detail that could be read is set whatever the status, so the status describes the query as a whole. This supersedes the flag in [DC-13](#dc-13-processinfoaccessdenied-flag-kept-in-band); the process still stays in the list.

**Rationale:** The flag was defined as "the process metrics could not be retrieved at all", which never happens: `NtQuerySystemInformation` returns the metrics of every process, protected ones included, without opening any ([DC-01](#dc-01-process-enumeration-ntquerysysteminformation-over-tool-help)). Only the details read by opening the process can be missing, and for several reasons the UI should tell apart. A process the monitor deliberately never opens is not one that refused access, and neither is a kernel process that has no command line. An enum is the error variant the domain rules ask for, and `NotQueried` as the default means a `ProcessInfo` never claims a query that did not happen.

---

### DC-73: Process details cached by `(PID, creation time)` with a domain `ProcessIdentity` key

**Decision:** `domain/process_identity.h` defines `ProcessIdentity` (`pid`, `creationTime`) with `ProcessIdentityHash`, and `ProcessInfo::identity()` returns it. `ProcessIdentityCache` (`platform/windows`, no Windows types) maps identities to `ProcessDetails`. `ProcessCollector` passes it every new list. For each process, the cache uses its entry if it has one and otherwise calls the injected `ProcessDetailsReader` once, caching the result whatever its status. It then erases every entry the list did not mention, using a generation counter instead of rebuilding the map. Without a details reader, every process's details stay `NotQueried`.

**Rationale:** Opening a process and reading its path and command line costs a few system calls per process, and the details cannot change while the process lives, so [architecture.md](architecture.md#windows-api-choices) asks for them to be read lazily and cached by `(PID, creation time)`. Keying by PID alone would show a previous process's path and command line for a new process that reused its PID. Erasing on every refresh bounds the cache by the process list and removes an exited process's entry before its PID can come back. Caching failures too means a process that denies access is not opened again every refresh; the cost is that a rare transient failure lasts until the process exits. The identity type is in `domain` because the CPU usage baselines and the Phase 4 process table need the same key. The cache takes the reader as a function, as the other collectors take their readers, so its tests cover PID reuse and eviction without Windows.

---

### DC-74: `lsass.exe` and `csrss.exe` recognised by image name and filtered before any details reader

**Decision:** `isNeverOpenedProcess()` (`platform/windows/process_access_policy.h`) matches `lsass.exe` and `csrss.exe` by image name, ignoring ASCII case. `ProcessCollector` wraps every details reader it is given with `skippingNeverOpenedProcesses()`, so the reader is never called for those processes. They get `ProcessDetailsStatus::NotQueried` with no path or command line, and keep every metric from the process list. The filter went in before the `OpenProcess` reader, so no commit contains code that would open them.

**Rationale:** Opening either process, even with `PROCESS_QUERY_LIMITED_INFORMATION`, trips credential-theft heuristics in most EDR products ([architecture.md — Antivirus and EDR considerations](architecture.md#antivirus-and-edr-considerations)). `NtQuerySystemInformation` already returns their metrics without a handle, so only the details are lost, and an unelevated query would usually be refused anyway. Filtering in the collector, rather than inside the Windows reader only, makes the rule hold for any reader and lets unit tests show with a fake that the reader never sees these processes. The Windows reader ([DC-75](#dc-75-process-details-read-with-a-limited-handle-checked-against-the-identity-and-logged-at-debug)) checks the name again just before `OpenProcess`, so code that calls it directly cannot open them either. Matching by name alone also skips any other process with one of these names. That costs only that process's path and command line, and never risks opening the real one, whose path does not need to be known.

---

### DC-75: Process details read with a limited handle, checked against the identity, and logged at debug

**Decision:** `makeWindowsProcessDetailsReader()` returns the production `ProcessDetailsReader`, which `ProcessCollector`'s default constructor uses. For each new process it:

1. Refuses `lsass.exe` and `csrss.exe` by name ([DC-74](#dc-74-lsassexe-and-csrssexe-recognised-by-image-name-and-filtered-before-any-details-reader)).
2. Opens the process with `PROCESS_QUERY_LIMITED_INFORMATION` only, held in a `std::unique_ptr` that calls `CloseHandle`.
3. Compares the creation time from `GetProcessTimes` with the identity's. On a mismatch, the details are `Unavailable`.
4. Reads the image path with `QueryFullProcessImageNameW`, starting at `MAX_PATH` and doubling up to 32,768 characters.
5. Reads the command line with `NtQueryInformationProcess(ProcessCommandLineInformation)` (class 60, which `winternl.h` does not name), resolved from `ntdll.dll` once when the reader is made. The buffer starts at 1 KiB and grows to the reported size, for up to three calls.

`ERROR_ACCESS_DENIED` and `STATUS_ACCESS_DENIED` map to `AccessDenied`; every other failure maps to `Unavailable`. Each detail keeps its own value, and the combined status is `AccessDenied` if either was denied ([DC-72](#dc-72-processdetailsstatus-replaces-processinfoaccessdenied)). Failures are logged at `debug`, and the architecture's logging table now lists them there.

On the development machine (Release, unelevated, 413 processes) the first `collect()`, which opens every process, took about 18 ms. Later ones took 3.5–6 ms: `NtQuerySystemInformation` itself 2.3–3.8 ms for a 1 MB list, parsing 0.1 ms, and the cache filling in the details about 1 ms. 254 processes returned both details, 155 denied access, 3 were never opened (`lsass.exe` and two `csrss.exe`), and 1 had none (the Idle process).

**Rationale:** `PROCESS_QUERY_LIMITED_INFORMATION` is the only access the security rules allow, and both queries need nothing more. `ProcessCommandLineInformation` reads the command line without `PROCESS_VM_READ`, which the rules forbid and which reading the PEB directly would need. Between reading the list and calling `OpenProcess`, a process can exit and its PID be reused. Without the creation-time check, the new process's path and command line would be cached under the old identity, the PID-alone mistake the identity rule exists to prevent. `GetProcessTimes` needs no access beyond what the handle already has. An unelevated monitor is refused by about a third of the processes on a desktop, every time it starts, so a warning per process would add about 150 warnings, and a log flush for each, to every start. The UI shows the status per process, and `debug` keeps the detail for diagnosis. The measured enumeration cost is higher than the 0.5–1.5 ms the architecture estimated, which confirms that it belongs on the slow-collector thread rather than the 1 Hz tick.

---

### DC-76: Per-process CPU usage computed in the collector, as a share of all logical processors

**Decision:** `ProcessInfo::cpuUsagePercent` is a `std::optional<float>`, set by `ProcessCollector` with the pure function `calculateProcessCpuUsage()`. The usage is the change in user + kernel time since the process's baseline, divided by the elapsed `steady_clock` time times the number of active logical processors in all processor groups, clamped to 0–100. Baselines are keyed by `ProcessIdentity` and replaced on every successful read, so exited processes drop out and a reused PID starts with no value. The value is `std::nullopt` the first time a process is seen, when no time has elapsed, when a process's CPU time went backwards, and when the processor count is unknown. The timestamp is taken right after the list is read, before the details are read for new processes. A failed read keeps the baselines, so the next success measures over the longer interval.

**Rationale:** The roadmap puts the calculation in the collector, which owns the previous sample. A rate kept beside the cumulative times ([DC-14](#dc-14-cpu-times-stored-as-cumulative-milliseconds-not-percentages)) spares the UI the bookkeeping and keeps it identical for every consumer. Dividing by every logical processor makes the column add up to the total CPU usage and matches Task Manager's Processes tab and this monitor's CPU card. A single-threaded process on 16 processors therefore peaks at 6.25%. Keying by identity keeps a new process from inheriting a previous process's baseline, which with PID alone could produce a huge or negative delta. `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)` is used because process CPU times cover every group, unlike `GetSystemTimes` ([DC-29](#dc-29-multi-processor-group-support-64-logical-cores-ntquerysysteminformationex-iteration-and-total-cpu-aggregation)). Taking the timestamp before the details are read keeps the up to 18 ms that the first details pass takes out of the interval. Windows credits CPU time at clock-tick granularity (about 15.6 ms), so a busy process can briefly exceed its share of the interval, hence the clamp. Millisecond CPU times lose nothing at that granularity.

**Extended by [DC-80](#dc-80-process-table-columns-io-rates-from-the-collector-and-na-for-missing-values):** the same pass now also computes each process's I/O read and write rates. `calculateProcessCpuUsage()` became `calculateProcessRates()`, and its baselines hold the I/O byte counters too.

---

### DC-77: A slow-collector thread in the scheduler, shared by processes and connectivity

**Decision:** `SamplingScheduler` owns a second `std::jthread` beside the tick thread. Every `slowInterval()` (2 seconds by default, `kDefaultSlowInterval`, adjustable with `setSlowInterval()`), it runs the registered process and connectivity collectors outside every lock. It then moves the results into a mutex-protected `SlowResults`. `sampleOnce()` runs only the fast collectors and copies the latest `SlowResults` into the snapshot under that mutex. Both threads wait on the scheduler's condition variable with their own stop tokens, so `stop()` interrupts either wait at once. Until the first slow refresh, snapshots have `processes == std::nullopt` and `ConnectivityLevel::Unknown`. Public `refreshSlowCollectors()` runs the slow collectors synchronously for unit tests and asserts that the scheduler is stopped. The whole connectivity collector moves to the slow thread, not only its polling fallback. `main.cpp` registers `ProcessCollector`. Resolves [D-1](known_deviations.md#d-1-sampling-scheduler-has-no-slow-collector-thread) and [D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread).

**Rationale:** A process refresh takes 3.5–6 ms on a 400-process desktop and about 18 ms the first time ([DC-75](#dc-75-process-details-read-with-a-limited-handle-checked-against-the-identity-and-logged-at-debug)), several times the roughly 1 ms the fast collectors share. Running it on the tick would delay every snapshot by that much and make the 1 Hz cadence uneven. One thread for all slow collectors is enough at this cadence and keeps a single place for the merge. Building the results before taking the mutex means the tick never waits for an enumeration, only for a move. Moving the whole connectivity collector, rather than only its fallback, follows the roadmap. It needs no new interface for a collector to say whether it is polling, and the tick then makes no connectivity call at all. The cost is that a notified change appears up to one slow interval later, which is acceptable for a status that changes rarely. Not waiting for the first slow refresh keeps `start()` and the first tick immediate. On the first snapshot connectivity reads Unknown, like the CPU sample, which has no baseline yet; the second snapshot has both. Each tick copies the latest process list into its snapshot, so a list is reused for about two ticks. Sharing it between snapshots without copying would change the snapshot's value type, so that waits until profiling of the process table shows a need.

---

### DC-78: Process integration tests: a suspended child process, exact read sets, and an elevation-aware access check

**Decision:** `tests/integration/process_tracking_test.cpp` tests the process collector end to end on the real machine:

- **Enumeration:** a `cmd.exe` child, created suspended with a command line unique to the run, appears with its exact identity, this process as its parent, and its real path and command line. After it is terminated and its handles closed, it leaves the list. The test polls for up to 5 seconds, because another process, such as an antivirus scanner, can briefly keep an exited process listed.
- **Cache behaviour:** a collector built from the real list reader and a counting wrapper around the real details reader reads exactly the identities of the first list, minus `lsass.exe` and `csrss.exe`. On the second collect it reads exactly the identities that are new, the child among them. The sets are compared exactly.
- **Access-denied handling:** every process's `detailsStatus` must match the details present (`Available` with both, `NotQueried` only for the never-opened names and with neither, `AccessDenied` or `Unavailable` with at most one). When the test runs unelevated, at least one process must deny access and still carry its name, creation time, and memory counters. When elevated (as on CI runners), that test is skipped.
- **Scheduler:** with the real collector registered, a snapshot from the running scheduler carries the process list, with this process's details read.

Paths are compared ignoring case, because `GetSystemDirectoryW` can return `C:\WINDOWS\system32` while `QueryFullProcessImageNameW` returns the on-disk case.

**Rationale:** A child created by the test is the one process whose identity, parent, path, and command line are known in advance. Creating it suspended means it never runs, needs no window, and cannot exit on its own during the test, and it shows that the command line is readable before the process has started. PID reuse cannot be forced on a real machine, so the unit tests cover it ([DC-73](#dc-73-process-details-cached-by-pid-creation-time-with-a-domain-processidentity-key)). Instead, the exact read sets show that the real cache reads each identity once. Which processes deny access depends on the account and elevation, so the access test asserts invariants that hold everywhere, and counts denials only where they are certain to occur. That follows the integration-test rule to tolerate varying environments.

---

## Phase 4 — Process explorer view (`src/ui/`)

### DC-79: Process table: a `QAbstractItemModel` with one row per `ProcessIdentity`, changed in place

**Decision:** The Processes tab shows a `QTreeView` over `ProcessModel`, a `QAbstractItemModel` subclass. Each row is a node owned by a map keyed by `ProcessIdentity`. An index's internal pointer is its node, and each node keeps its row number, so `indexOf()` needs no search. `updateProcesses()` removes the rows of processes that are no longer listed, gives the remaining rows their latest values, and appends the new processes with one `beginInsertRows()`. It never resets the model. A duplicate identity in a list adds one row. `MainWindow` forwards every snapshot to `ProcessesView`, which applies the list whether or not the tab is shown. A snapshot without a list keeps the last rows. The summary line above the table then reads "Processes: N/A (showing the last list read)", and "Processes: --" before the first list. The Idle process (PID 0) has no image name in the list ([DC-71](#dc-71-process-list-parsed-through-a-full-system_process_information-layout-checked-against-winternlh)), and the table names it "System Idle Process". `formatCount()` moved from `performance_readouts.h` to `dashboard_formatting.h`, where the formatting that several tabs share lives ([DC-68](#dc-68-shared-ui-constants-and-helpers-where-each-one-lives-code-review-f-28)).

**Rationale:** The model derives from `QAbstractItemModel` rather than `QAbstractTableModel` because the process tree needs parent and child rows in the same model, and a `QTreeView` shows a flat model as a table. Keying rows by identity rather than PID means a process that reuses a PID gets a new row, rather than the old process's row with new values. Changing rows in place keeps the selection, the scroll position, and every `QPersistentModelIndex` a view or proxy holds. A reset on every refresh would lose them, which [ui/AGENTS.md](../src/ui/AGENTS.md) forbids. The tab applies every list even while hidden, as every view records every snapshot ([DC-54](#dc-54-snapshot-forwarding-every-view-records-every-snapshot-and-hidden-views-skip-repainting-code-review-f-4)): the model is the tab's record of the process list, and a hidden `QTreeView` does not repaint. A failed read keeps the last rows rather than emptying the table, because treating it as an empty list would remove every row and add them all back on the next successful read. The summary line says that the rows are stale.

---

### DC-80: Process table columns: I/O rates from the collector, and N/A for missing values

**Decision:** The process table's columns are Name, PID, CPU, Memory, I/O, Network, Threads, and Handles.

- **CPU** is `cpuUsagePercent` with one decimal ([DC-76](#dc-76-per-process-cpu-usage-computed-in-the-collector-as-a-share-of-all-logical-processors)). **Memory** is the working set in binary units. **Threads** and **Handles** are counts with thousands separators.
- **I/O** is read plus write bytes per second. `ProcessInfo` gains `ioReadBytesPerSec` and `ioWriteBytesPerSec`, which `ProcessCollector` computes in the same pass as the CPU usage: `calculateProcessCpuUsage()` became `calculateProcessRates()`, and its baselines hold the I/O byte counters too. Each rate is `std::nullopt` the first time a process is seen, when no time has elapsed, or when its own counter went backwards. `calculateByteRate()` moved out of the network collector into `platform/windows/byte_rate.h`, which both collectors use. The cell shows the sum, or "N/A" when either rate is missing, and its tooltip shows each rate.
- The column is headed "I/O" rather than the roadmap's "Disk I/O". Its header tooltip says it counts files, pipes, and devices, not only disk.
- **Network** shows "N/A" until per-process network usage is collected with ETW, and its header tooltip says so.
- A missing value shows "N/A" in the palette's placeholder text colour. A measured zero shows "0.0%" or "0 B/s" in the normal colour.
- Numbers and their headers are right-aligned. Every header has a tooltip. Each value column is sized once to fit a template of its widest expected text, such as "1023.9 MiB/s", and the Name column takes the remaining width.
- On each update the model emits one `dataChanged()` over the value columns (CPU to Handles) of every row it kept. Name and PID never change for an identity.

**Rationale:** The process list reaches the UI with every 1 Hz snapshot but is refreshed only every 2 seconds ([DC-77](#dc-77-a-slow-collector-thread-in-the-scheduler-shared-by-processes-and-connectivity)), so consecutive snapshots often carry the same counters. The snapshot's timestamp is also the tick's, not the time the list was read. A rate computed in the UI would therefore alternate between zero and double the real value. The collector knows when each list was read, so it computes the I/O rates there, from the same baselines and timestamp as the CPU usage. Keeping read and write separate in the domain leaves both available to other consumers, while the table shows their sum, as the roadmap asks. Summing only when both rates are known avoids understating the total, the rule `sumActiveThroughput()` follows for adapters. The counters are `ReadTransferCount` and `WriteTransferCount`, which include pipe and device I/O ([DC-71](#dc-71-process-list-parsed-through-a-full-system_process_information-layout-checked-against-winternlh)), so "Disk I/O" would mislabel them. The Network column stays so the layout does not change when ETW arrives. Its cells say "N/A" rather than zero because no value is known, and they are dimmed so a column of placeholders does not draw the eye. Fixed widths from templates keep the columns still as values change, and avoid `QHeaderView::ResizeToContents`, which measures every row whenever the data changes.
