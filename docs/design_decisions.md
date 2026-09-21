# Design Decisions

Rationale for non-obvious implementation choices made during development.
Each entry links to the relevant roadmap phase and source files.

---

## Windows API choices

### Process enumeration: `NtQuerySystemInformation` over Tool Help

**Decision:** Use `NtQuerySystemInformation(SystemProcessInformation)` for process enumeration, not `CreateToolhelp32Snapshot`.

**Rationale:** `CreateToolhelp32Snapshot` internally calls `NtQuerySystemInformation`, strips out CPU, memory, and I/O metrics, and forces the caller to make hundreds of supplementary `OpenProcess` calls to reconstruct data the kernel already returned in that single call. On a system with ~300 processes this produces 300+ extra kernel transitions and takes 15–45 ms compared to 0.5–1.5 ms for the direct call. Beyond performance, `CreateToolhelp32Snapshot` denies access to metrics for 30–50% of processes on an unelevated caller because each `OpenProcess` can fail individually; `NtQuerySystemInformation` returns complete CPU, memory, and I/O data for all processes including protected and elevated ones. `NtQuerySystemInformation` is declared in `<winternl.h>`, has been ABI-stable since Windows 2000, and is the API used internally by Task Manager, Process Explorer, and System Informer.

---

### Core metrics: direct Win32 APIs over PDH

**Decision:** Use `GetSystemTimes`, `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`, and `GetIfTable2` for core metrics. PDH is reserved only for advanced per-instance counters (e.g. per-physical-disk I/O rates) where no direct Win32 alternative exists.

**Rationale:** PDH has two structural problems that make it unsuitable for core metrics. First, PDH counter names are locale-dependent; `PdhAddCounterW` with a hard-coded English path fails on non-English Windows installations unless `PdhAddEnglishCounterW` is used, but even then PDH reads counter definitions from the registry — which is frequently corrupted on long-lived installations, causing `PDH_CSTATUS_NO_OBJECT` or `PDH_CSTATUS_NO_COUNTER` errors at runtime. The direct Win32 APIs (`GetSystemTimes` etc.) have zero registry dependency. Second, for per-process metrics PDH assigns index suffixes to processes that share a name (`chrome#0`, `chrome#1`) and silently re-indexes when one terminates; any open PDH query handle will silently read data from a different process after re-indexing, producing corrupt data with no error signal. `NtQuerySystemInformation` has no such concept — processes are identified by PID and creation time.

---

### Connectivity status: `GetNetworkConnectivityHint` over ad-hoc probes

**Decision:** Use `GetNetworkConnectivityHint` (and `NotifyNetworkConnectivityHintChange` for async updates) to determine network connectivity state.

**Rationale:** Ad-hoc ICMP/TCP probes require choosing a target, have non-trivial latency (100–2000 ms), consume bandwidth, and produce a binary online/offline result that misses important states like captive portals and metered connections. `GetNetworkConnectivityHint` returns a structured `NL_NETWORK_CONNECTIVITY_LEVEL_HINT` — None, LocalAccess, ConstrainedInternetAccess (captive portal), or InternetAccess — along with cost information (metered vs. unmetered) and roaming state. This is a direct C-style kernel API with no COM overhead. The companion notification API delivers change events without polling. Requires Windows 10 version 2004 or later, which is within the project's target baseline.

---

### WMI restricted to startup hardware inventory

**Decision:** WMI is queried at most once at application startup for static hardware information (`Win32_ComputerSystem`, `Win32_Processor`, `Win32_BaseBoard`). WMI is never polled for real-time metrics.

**Rationale:** WMI is an out-of-process COM call to `WmiPrvSE.exe`. Each query incurs 50–500 ms of latency and measurable CPU cost on the host. Polling it for metrics that change frequently (CPU usage, memory) would make the monitor a significant load source, contradicting the low-impact goal. WMI repository corruption is also common on long-lived Windows installations and manifests as silent data errors or hung calls. For the one legitimate use case — reading static hardware labels at startup — the latency is acceptable because it occurs once before the UI is shown. All real-time metrics use direct Win32 APIs.

---

### ETW deferred to post-MVP

**Decision:** ETW (Event Tracing for Windows) is excluded from the MVP. It will be reconsidered only when per-process network bandwidth tracking or event-level diagnostics are a product requirement.

**Rationale:** ETW kernel trace sessions require administrator privileges, making them incompatible with the unelevated-by-default security posture. Windows enforces a hard system-wide limit of 64 concurrent ETW sessions, shared with EDR products, antivirus, and system loggers; exceeding this limit returns `ERROR_NO_SYSTEM_RESOURCES` and the session fails to start. Buffer sizing and flush tuning are non-trivial and highly workload-dependent. None of the MVP metrics require ETW — per-process CPU, memory, and I/O are fully available through `NtQuerySystemInformation`. The complexity and privilege cost are not justified until a specific feature (per-process network bandwidth) cannot be built without it.

---

### Network profile names: `INetworkListManager` lazy and cached

**Decision:** The COM-based Network List Manager (`INetworkListManager`) is used only to retrieve a friendly network name (e.g. "Home-Wi-Fi") for display. It is queried at most once per network change event, never on every refresh tick.

**Rationale:** COM initialization carries per-thread cost and `INetworkListManager` calls cross into a system service. For a value that changes only when the user switches networks, querying it on every 1-second tick would be unnecessary overhead. All connectivity state (level, cost, roaming) and all throughput data come from `GetNetworkConnectivityHint` and `GetIfTable2`, which are C-style kernel APIs. The NLM is used purely for the human-readable label that those APIs do not provide.

---

## Phase 1 — Domain types (`src/domain/`)

### CMake library type: `INTERFACE` (header-only)

**Decision:** `sysmon_domain` is declared as a CMake `INTERFACE` library with no compiled sources.

**Rationale:** Every domain type is a plain aggregate struct or `enum class`. There are no method bodies, no `.cpp` translation units, and nothing to link. An `INTERFACE` target is the idiomatic CMake choice in this situation: it propagates the `src/` include path transitively to any downstream target via `target_link_libraries`, so consumers never need to repeat `target_include_directories`. A `STATIC` library would produce an empty archive and add a link step with zero benefit.

---

### `ProcessInfo::creationTime` type: `std::chrono::system_clock::time_point`

**Decision:** The process creation timestamp is stored as `std::chrono::system_clock::time_point`, not `uint64_t` raw FILETIME or `steady_clock::time_point`.

**Rationale:** The source value is a Windows `FILETIME` — a 100-nanosecond tick count since 1601-01-01, which is a calendar epoch. `system_clock` is the C++ clock that maps to calendar time, making the conversion at the platform boundary lossless and semantically correct. `steady_clock` is reserved for `SystemSnapshot::timestamp` where monotonic elapsed-time arithmetic is needed (CPU %, network throughput rates). Storing a raw `uint64_t` would leak a Windows type into the domain layer, which must remain platform-free.

---

### `SystemSnapshot::timestamp` type: `std::chrono::steady_clock::time_point`

**Decision:** `SystemSnapshot::timestamp` uses `steady_clock`, not `system_clock`.

**Rationale:** The timestamp exists solely for rate calculations (CPU usage %, network bytes/sec) between consecutive snapshots. `steady_clock` is monotonic and unaffected by system clock adjustments or DST transitions, making delta calculations reliable. It carries no calendar meaning; the UI must use `system_clock::now()` separately if it needs a human-readable time to display.

---

### `NetworkSample` stores cumulative byte counters, not rates

**Decision:** `inBytesTotal` and `outBytesTotal` are cumulative totals as returned by `GetIfTable2`, not pre-computed bytes/sec rates.

**Rationale:** Rate computation requires dividing a byte delta by a precisely measured time delta. That measurement belongs in the collector, which owns the `steady_clock` baseline from the previous tick. Storing a rate in the domain type would mean the type carries a value that depends on when it was last queried — breaking the immutable-snapshot guarantee. Cumulative counters are also correct across tick-interval jitter, whereas a rate pre-computed with an assumed interval would drift if the scheduler tick is late.

---

### `OperationalStatus` enum: all 7 `IF_OPER_STATUS` values

**Decision:** `OperationalStatus` includes all seven values from the Windows `IF_OPER_STATUS` enum (`Up`, `Down`, `Testing`, `Unknown`, `Dormant`, `NotPresent`, `LowerLayerDown`), not a trimmed subset.

**Rationale:** A 1:1 mapping between the domain enum and the Windows API enum makes the platform-boundary conversion trivial and keeps the domain type self-documenting. Collapsing values (e.g. merging `Dormant` and `NotPresent` into a generic `Inactive`) would lose information that the UI or future alert logic might need. The domain module has no Windows dependency — having the same *shape* as a Windows enum does not violate the no-Windows-headers rule.

---

### `std::optional` for `ProcessInfo::imagePath` and `::commandLine`

**Decision:** Image path and command line are `std::optional<std::string>`, not empty strings.

**Rationale:** These fields are acquired lazily — only on the first detection of a new `(pid, creationTime)` pair — and may be permanently unavailable for protected or system processes even with `PROCESS_QUERY_LIMITED_INFORMATION`. An empty string is ambiguous: it could mean "not yet queried", "query failed", or "the process genuinely has no path". `std::nullopt` is unambiguous: this data is not available. The UI can branch on `has_value()` to render a clear indicator rather than displaying a blank field.

---

### `ProcessInfo::accessDenied` flag kept in-band

**Decision:** `ProcessInfo` includes an `accessDenied` bool rather than being omitted from the snapshot or replaced with an error variant at the `vector<ProcessInfo>` level.

**Rationale:** Even when a process cannot be fully introspected, `NtQuerySystemInformation` returns its name and PID. Omitting the process from the snapshot entirely would silently hide it from the user (wrong: Task Manager shows all processes). A separate error list alongside `vector<ProcessInfo>` would force all UI code to merge two collections. The in-band flag keeps the snapshot a single coherent list and lets the UI render a partial row — image name visible, metrics showing `—` — which is the same pattern used by Task Manager and Process Explorer.

---

### CPU times stored as cumulative milliseconds, not percentages

**Decision:** `ProcessInfo::cpuUserTimeMs` and `::cpuKernelTimeMs` are cumulative totals in milliseconds, not pre-computed CPU percentages.

**Rationale:** CPU percentage for a process requires comparing the CPU time delta between two snapshots against the wall-clock delta between those same snapshots. This calculation belongs in the collector or view model, which tracks both baselines. Storing a percentage in the domain struct would mean the value is meaningless without knowing the interval over which it was measured — and that interval is not part of the struct. Cumulative milliseconds are the raw truth; any derived value can be computed correctly from them.

---

## Phase 1 — Monitoring infrastructure (`src/monitoring/`)

### `RingBuffer<T>`: Mirrored contiguous storage for zero-copy `std::span`

**Decision:** `RingBuffer<T>` pre-allocates $2 \times \text{capacity}$ contiguous storage and mirrors every push to both `pos` and `pos + capacity`.

**Rationale:** A traditional circular buffer wraps around the end of its storage array, requiring either linearizing elements into a temporary buffer (incurring heap allocation or copying) or forcing callers to consume two disjoint slices (`span1` and `span2`). Chart widgets (e.g. sparklines) and consumer models require a single contiguous `std::span<const T>` representing samples in chronological order. By pre-allocating twice the capacity and writing each sample to `pos` and `pos + capacity`, any window of length $\le \text{capacity}$ starting at the oldest element's index is guaranteed to be contiguous in memory. This achieves $O(1)$ zero-copy contiguous span access while strictly ensuring zero heap allocations on `push()` after construction.

---

### `SamplingScheduler`: Responsive cancellation via `std::condition_variable_any` with `std::stop_token`

**Decision:** The scheduler thread tick loop uses `std::condition_variable_any::wait_until` with `std::stop_token` rather than `std::this_thread::sleep_for` or a polling sleep loop.

**Rationale:** A standard `sleep_for(m_interval)` cannot be interrupted until the interval expires, delaying application shutdown by up to 1000 ms. A polling loop (e.g. sleeping 10 ms repeatedly) wastes CPU cycles and adds wake jitter. In C++20, `std::condition_variable_any` natively accepts a `std::stop_token` in `wait_until`. If a stop is requested (e.g. when `SamplingScheduler::stop()` or the destructor is called), the condition variable wakes immediately and exits the thread. Furthermore, calculating `nextTick = tickStart + m_interval` prevents interval drift across ticks when collection work takes measurable time.

---

### `ICollector<T>`: Dual-layer interface and C++20 concept

**Decision:** Provide `template<typename T> class ICollector` with a pure virtual `collect()` method for dynamic polymorphism in `SamplingScheduler`, paired with a `Collector<C, T>` C++20 concept.

**Rationale:** `SamplingScheduler` must store heterogeneous collectors (CPU, memory, disk, etc.) whose concrete implementations can be swapped between synthetic implementations (for testing and early phases) and real Windows collectors (in later phases). Making `SamplingScheduler` an un-templated `QObject` holding `std::unique_ptr<ICollector<T>>` keeps Qt signal/slot metadata clean and avoids template bloat. Meanwhile, the `Collector<C, T>` concept enables compile-time verification in tests and non-virtual template contexts, ensuring consistency across static and dynamic collection code.

