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

### Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)

**Decision:** All string fields in domain types use `std::string` with UTF-8 encoding. Wide-to-UTF-8 conversion happens at the `platform/windows` boundary.

**Rationale:** The domain layer must be platform-independent. `std::wstring` is a Windows-specific convention (16-bit code units); using it would leak a platform assumption into `src/domain/`. UTF-8 is the de facto standard encoding for cross-platform C++ and is natively supported by Qt (`QString::fromUtf8`), spdlog, and nlohmann/json. The Windows API wrapper functions in `platform/windows/` convert from `WCHAR*` / `std::wstring` to UTF-8 `std::string` using `WideCharToMultiByte(CP_UTF8, ...)` or equivalent. Supplementary-plane characters (e.g., emoji in filenames) round-trip correctly through UTF-8.

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

---

### `SamplingScheduler`: Pointer snapshotting during `sampleOnce()` to avoid holding `m_collectorMutex` during collections

**Decision:** `sampleOnce()` locks `m_collectorMutex` only long enough to copy raw collector pointers (`get()`), then invokes `collect()` on each collector outside the lock. The invariant that collectors may only be registered before `start()` or after `stop()` is enforced with assertions and documented.

**Rationale:** In Phase 2, real Windows API collectors may take 0.5–1.5 ms (e.g. process enumeration) or query network tables. Holding `m_collectorMutex` for the duration of all collections would unnecessarily serialize collector configuration and sampling, and block any caller wanting to inspect or modify scheduler registration. Releasing the lock before collection keeps critical sections minimal ($O(1)$ pointer copies) while maintaining strict safety under the documented stopped-state registration invariant.

---

## Phase 1 — Basic UI shell (`src/ui/`)

### UI module dependency isolation and composition root in `src/app/`

**Decision:** The `ui` module depends exclusively on `Qt6::Widgets` and `sysmon_domain`, with zero dependencies on `sysmon_monitoring` or Windows headers. Wiring between `SamplingScheduler` signals and `MainWindow` slots is handled strictly in `src/app/main.cpp` (the application composition root).

**Rationale:** Maintaining a one-way dependency flow prevents cyclic references and tight coupling. The UI layer only knows about domain values (`SystemSnapshot`). It does not know how or when data is collected, nor what thread model is used. By placing the signal/slot `Qt::QueuedConnection` wiring in `main.cpp`, `MainWindow` can be tested in isolation with mock snapshots, and collectors can be altered or replaced without recompiling any UI translation units.

---

### `test_main.cpp`: Test discovery without `QApplication` instantiation

**Decision:** In the test runner (`tests/test_main.cpp`), `QApplication` instantiation is bypassed when `--gtest_list_tests` is passed during CMake's `gtest_discover_tests` step.

**Rationale:** When CMake invokes the test binary with `--gtest_list_tests` at build time to discover tests, initializing `QApplication` would attempt to load platform plugins before they are fully deployed or in headless environments without an active display session, risking modal error dialogues or timeouts. Bypassing `QApplication` for list-only execution allows instantaneous and reliable test discovery, while full test execution creates `QApplication` and leverages the deployed platform plugin.

---

## Phase 2 — Real Windows collectors and dashboard (`src/platform/windows/`)

### Total CPU utilization formula: `(deltaKernel + deltaUser - deltaIdle) / (deltaKernel + deltaUser)`

**Decision:** Calculate total CPU utilization by subtracting `deltaIdle` from `(deltaKernel + deltaUser)` and dividing by `(deltaKernel + deltaUser)` over monotonic elapsed time.

**Rationale:** Windows `GetSystemTimes` returns cumulative idle, kernel, and user times in 100 ns `FILETIME` ticks across all logical processors. A key characteristic of Windows kernel accounting is that `lpKernelTime` already includes the execution time of the idle thread. Thus, total system capacity elapsed across all processors is `deltaKernel + deltaUser`, and active busy time is `(deltaKernel + deltaUser) - deltaIdle`. This ratio is mathematically invariant to CPU frequency scaling (Intel SpeedStep / AMD Cool'n'Quiet), processor group topologies, and core count. We guard against multi-core non-atomic counter jitter by clamping usage to $[0.0\%, 100.0\%]$. When no rate can be computed (non-positive elapsed time, a counter that went backwards, or no elapsed CPU time), `calculateCpuUsage` returns `std::nullopt` rather than 0% (see "No zero placeholders for CPU and memory" below).

---

### Encapsulation of Windows SDK headers strictly inside translation units

**Decision:** `cpu_collector.h` contains no `<Windows.h>` or Windows SDK types. All Windows types (`FILETIME`, `DWORD`, etc.) are converted to standard fixed-width integer types (`uint64_t`) in `cpu_collector.cpp`.

**Rationale:** Including `<Windows.h>` in a header exposes macros like `min`, `max`, `near`, `far`, and thousands of global symbols transitively to any module that includes it, violating the project rule that only `platform/windows` interacts with the Windows SDK. By confining `<Windows.h>` strictly to `.cpp` files, headers remain clean, lightweight standard C++20.

---

### Deterministic unit testing via injected time and clock readers

**Decision:** `CpuCollector` provides a dependency-injected constructor accepting custom `SystemTimesReader` and `SteadyClockReader` lambdas alongside its default Windows constructor, paired with a pure `calculateCpuUsage` function.

**Rationale:** Unit tests must be fast, deterministic, and free of OS dependencies per project rules. The injected constructor allows simulating edge cases such as exact idle/user/kernel ratios, zero elapsed intervals, clock jitter where idle temporarily appears greater than kernel, and API query failures without invoking the Windows kernel or relying on sleep timers. Real host integration tests in `tests/integration/` verify live `GetSystemTimes` execution separately.

---

### Per-core CPU utilization: `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` dynamic resolution and resizing-buffer pattern

**Decision:** Resolve `NtQuerySystemInformation` dynamically from `ntdll.dll` via `GetModuleHandleW` / `GetProcAddress` and query `SystemProcessorPerformanceInformation` (class 8) using a resizing-buffer loop. Per-core utilization is calculated by applying the same `calculateCpuUsage` logic to each core's `IdleTime`, `KernelTime`, and `UserTime`.

**Rationale:** `NtQuerySystemInformation` is the native NT kernel interface used by Task Manager and Process Explorer to query per-processor counters. In Windows, `ntdll.dll` is mapped into every user-mode process at process creation, so resolving the function pointer dynamically via `GetProcAddress` avoids static import dependencies while ensuring universal compatibility. The returned `SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION` structure includes idle time within `KernelTime` for each individual core, mirroring the aggregate behavior of `GetSystemTimes`. The resizing-buffer loop pattern starts with an estimated allocation equal to the detected logical core count and automatically handles `STATUS_INFO_LENGTH_MISMATCH` (0xC0000004) if processor topology or active counts change.

---

### Multi-processor-group support (>64 logical cores): `NtQuerySystemInformationEx` iteration and total CPU aggregation

**Decision:** Use `NtQuerySystemInformationEx` to iterate through all active processor groups (`0 .. GetActiveProcessorGroupCount() - 1`), with fallback to `NtQuerySystemInformation` for Group 0 if `Ex` resolution or runtime calls fail. On multi-group systems (>64 cores), compute total CPU usage by aggregating `idleTime`, `kernelTime`, and `userTime` across all cores rather than relying solely on `GetSystemTimes`.

**Rationale:** On 64-bit Windows, processor affinities are limited to 64 bits per group, so high-core machines (>64 logical processors) are partitioned into multiple processor groups. On such machines, `GetSystemTimes` and legacy `NtQuerySystemInformation` only report statistics for the primary group of the calling thread, rendering workloads in secondary groups invisible. Calling `NtQuerySystemInformationEx` with `USHORT group` in the input buffer retrieves each group's cores sequentially, guaranteeing that `CpuSample::coreUsagePercents` captures all logical cores in continuous processor index order. Furthermore, aggregating all cores' counter deltas to compute `totalUsagePercent` on multi-group systems ensures accurate system-wide utilization metrics regardless of how the OS scheduler distributes threads across groups.

---

### CPU collector integration testing: Invariant-based assertions and synthetic load sensitivity

**Decision:** Design host integration tests around system and mathematical invariants ($0.0 \le \text{usage} \le 100.0$, topology matching `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`, non-NaN/Inf) and synthetic workload sensitivity rather than asserting fixed utilization values.

**Rationale:** Unlike deterministic unit tests with injected data, host integration tests execute against live Windows kernel APIs where background OS services, indexing, and power-saving C-states cause continuous utilization fluctuations. Asserting fixed numbers produces flaky tests. Verifying invariants across multiple consecutive real-time samples, observing load increases during controlled multi-threaded synthetic loops, and validating live signal delivery through `SamplingScheduler` proves the collector operates correctly on real hardware under variable load without false positives.

---

### Memory sampling: `GlobalMemoryStatusEx` and commit charge calculation

**Decision:** Sample physical and virtual memory using `GlobalMemoryStatusEx`. Compute `commitCurrent` as `ullTotalPageFile - ullAvailPageFile`. Clamp memory usage percent defensively to $[0.0\%, 100.0\%]$.

**Rationale:** `GlobalMemoryStatusEx` is an ultra-fast kernel query (< 1 microsecond) with zero external or registry dependencies. In `MEMORYSTATUSEX`, `ullTotalPhys` and `ullAvailPhys` report physical RAM. `ullTotalPageFile` represents the current system commit limit (physical RAM plus current pagefile allocation), and `ullAvailPageFile` represents the remaining commit space available to processes before allocations fail. Subtracting available pagefile from total pagefile yields the active system commit charge (`commitCurrent`), matching Task Manager's commit metric ("Committed x / y GB"). Defensive boundary clamping protects against transient kernel counter jitter.

---

### Disk space sampling: `GetDiskFreeSpaceExW` strictly filtered to `DRIVE_FIXED`

**Decision:** Enumerate drive letters via `GetLogicalDrives()`, inspect each drive root using `GetDriveTypeW()`, and call `GetDiskFreeSpaceExW` only for drives returning `DRIVE_FIXED`.

**Rationale:** Calling disk space APIs on removable drives (`DRIVE_REMOVABLE`), optical media (`DRIVE_CDROM`), or remote shares (`DRIVE_REMOTE`) can cause blocking network timeouts or trigger Windows system modal dialogs ("Insert disk into drive..."), freezing the background scheduler tick. Restricting collection strictly to `DRIVE_FIXED` ensures fast, non-blocking queries of all local SSDs and HDDs. If `GetDiskFreeSpaceExW` fails for a specific drive (e.g. unformatted volume or BitLocker locked partition), that drive is skipped gracefully without interrupting collection for other healthy drives.

---

### Deterministic testing of memory and disk collectors via reader injection

**Decision:** Provide dependency-injected constructors in `MemoryCollector` and `DiskCollector` accepting reader callbacks (`MemoryStatusReader`, `LogicalDrivesReader`, `DiskSpaceReader`) alongside pure calculation functions `calculateMemorySample` and `calculateDiskSample`.

**Rationale:** In accordance with project rules, unit tests must be fast, deterministic, and free of OS dependencies. Injected readers allow testing boundary conditions — such as zero total memory/disk space, free space exceeding total due to quota configurations, 100% capacity, API query failures, and multi-drive volume sets — without altering host configuration or creating synthetic drive partitions. Integration tests in `tests/integration/` independently verify live Windows API queries on real host hardware.

---

### Network adapter traffic & throughput: `GetIfTable2` with RAII `FreeMibTable` and delta rates

**Decision:** Use `GetIfTable2` to read 64-bit cumulative byte counters (`InOctets`, `OutOctets`), negotiated link speed, and operational status. Wrap allocated `MIB_IF_TABLE2` memory in a custom RAII deleter calling `FreeMibTable`. Compute instantaneous throughput rates (`inBytesPerSec`, `outBytesPerSec`) from counter deltas divided by elapsed monotonic time (`std::chrono::steady_clock`).

**Rationale:** `GetIfTable2` is the modern Windows IP Helper interface that returns 64-bit octet counters without registry or PDH dependencies. Unlike legacy 32-bit `GetIfTable`, 64-bit counters will not roll over under modern gigabit and 10 GbE traffic within days. RAII wrapping via `std::unique_ptr<MIB_IF_TABLE2, MibTableDeleter>` guarantees memory is safely released on all return paths. Throughput rates are computed per adapter baseline over measured monotonic durations. A rate is `std::nullopt`, never 0, when it cannot be computed: the first sample for an adapter, zero elapsed time, or a counter that went backwards (e.g. an interface reset), where the delta across the reset is unknown. Each direction is evaluated independently, so a reset of one counter does not hide the other direction's rate. A genuinely idle link still reports 0. See [known deviation D-6](known_deviations.md#d-6-network-throughput-reports-zero-when-no-baseline-exists).

---

### Adapter identity & addresses: Correlating `GetIfTable2` with `GetAdaptersAddresses`

**Decision:** Query `GetAdaptersAddresses` (with `AF_UNSPEC` and `GAA_FLAG_INCLUDE_PREFIX`) to retrieve friendly names, device descriptions, IPv4/IPv6 addresses, and DNS servers, correlating them with `GetIfTable2` rows using `NET_LUID::Value` (falling back to `IfIndex`). Filter out loopback interfaces (`IF_TYPE_SOFTWARE_LOOPBACK`) and NDIS filter interfaces (`InterfaceAndOperStatusFlags.FilterInterface`). Carry `InterfaceAndOperStatusFlags.HardwareInterface` into `NetworkSample::isHardwareInterface`.

**Rationale:** While `GetIfTable2` provides high-precision counters and link state, it lacks network addresses and DNS configuration. `GetAdaptersAddresses` provides comprehensive address and DNS information. Correlating both APIs via the 64-bit `InterfaceLuid.Value` ensures exact, collision-free pairing between interface counters and IP addresses across all physical, virtual, and Wi-Fi adapters. Filtering `IF_TYPE_SOFTWARE_LOOPBACK` removes local pseudo-interfaces (`127.0.0.1` / `::1`) from user-facing monitor tables.

`GetIfTable2` also returns one row per NDIS lightweight filter bound to an adapter (for example "Ethernet-WFP Native MAC Layer LightWeight Filter-0000" and "Ethernet-QoS Packet Scheduler-0000"). These rows are Up and carry the same counters as the adapter, so a sum over all Up rows counted the physical traffic several times ([code review F-9](code_reviews/phase_2.md#f-9-network-throughput-total-counts-the-same-traffic-several-times)). They mean nothing to a user and Task Manager does not show them, so they are dropped at the platform boundary. Virtual adapters such as VPN tunnels and Hyper-V vEthernet are kept, because the Phase 5 network view should list them, but they are marked non-hardware: their traffic also crosses a physical adapter, so totals must not include them.

---

### Adapter details cached and re-read only on change

**Decision:** `NetworkCollector` reads `GetIfTable2` on every tick but keeps the `GetAdaptersAddresses` results (names, descriptions, IP addresses, DNS servers) in an `AdapterDetailsCache`. The cache re-reads them when an address or interface change notification (`NotifyUnicastIpAddressChange`, `NotifyIpInterfaceChange`) has marked it stale, when `GetIfTable2` shows an interface that was not present at the last refresh, or when 30 seconds have passed. A failed re-read clears the details and is retried on the next tick. The production reader, `WindowsAdapterReader`, lives in `network_collector.cpp` and is owned by the collector. The refresh policy is a separate class with no Windows types so `tests/unit/` can test it.

**Rationale:** On the review machine `GetAdaptersAddresses` took about 2 ms per call, compared with about 0.7 ms for `GetIfTable2`. That alone exceeded the fast-collector budget, and it was spent re-reading data that changes only when the network configuration does ([code review F-10](code_reviews/phase_2.md#f-10-getadaptersaddresses-runs-on-every-scheduler-tick)). The notifications are an event-driven source under the architecture's rules: the callbacks only set the stale mark, the registrations are RAII members declared after the cache so they are cancelled first, and the cache starts stale so the first sample is complete. The stale mark is cleared before re-reading, so a change reported during a re-read triggers another one. No IP Helper notification reports DNS server changes, so the 30-second maximum age catches those, and it keeps details current if registration fails. The rejected alternative was refreshing only when the set of interfaces changes, which misses address changes on an existing adapter (for example a DHCP renewal with a new lease).

---

### Repeated collector failures logged once, with the recovery

**Decision:** Every OS query a collector repeats on each tick reports failures through a `RepeatedFailureLog` (`platform/windows/repeated_failure_log.h`). The first failure after a success is logged at the level the call site chooses, consecutive repeats are logged at `debug`, and the first success afterwards is logged at `info` with the failure count. The state lives with the query: in the default reader closures (`mutable` lambdas), in `WindowsAdapterReader`, or per volume for `GetDiskFreeSpaceExW`.

**Rationale:** Collectors run every second, so a persistent failure such as a BitLocker-locked fixed volume logged about 86,000 warnings a day. Because the logger flushes on `warn`, each of those also forced a disk write, and the 15 MB of rotating logs pushed out the useful history within a day or two ([code review F-11](code_reviews/phase_2.md#f-11-persistent-failures-are-logged-on-every-tick)). Logging only when the state changes follows the connectivity collector's change-only logging. It keeps the log readable and still leaves the full detail available at `debug`. Keeping the state beside each query, rather than in one shared object, lets a failing volume and a healthy one report independently. It also keeps the injected test readers unchanged.

---

### Deterministic testing of network collection via reader injection

**Decision:** Inject `NetworkAdaptersReader` and `SteadyClockReader` lambdas into `NetworkCollector`, supported by a pure `calculateNetworkSamples` function managing historical baselines and purging disconnected adapters.

**Rationale:** Simulating multiple network interfaces, varying link speeds, counter rollovers, IP/DNS configurations, loopback filters, and baseline eviction across elapsed intervals requires deterministic control over input data and monotonic timestamps. Injected readers allow comprehensive unit testing without network privileges or hardware manipulation, while host integration tests verify real Windows IP Helper API calls.






---

### Connectivity status: `NotifyNetworkConnectivityHintChange` with a synchronously seeded cache

**Decision:** `ConnectivityCollector` seeds a mutex-protected `ConnectivityStatus` cache with `GetNetworkConnectivityHint` in its constructor, then registers `NotifyNetworkConnectivityHintChange` (with `InitialNotification = TRUE`). `collect()` returns the cached value on the scheduler tick. Registration goes through an injected `ConnectivitySubscriber` that returns an RAII `ConnectivitySubscription`. The production `WindowsConnectivitySubscription`, defined in the `.cpp`, calls `CancelMibChangeNotify2` in its destructor. The subscription is the collector's last member, so it is destroyed before the mutex and cache. If registration fails at runtime, the collector falls back to polling `GetNetworkConnectivityHint` in `collect()` (see [known deviation D-2](known_deviations.md#d-2-connectivity-fallback-polls-on-the-scheduler-thread)). Each change of status is logged once at `info`.

**Rationale:** Connectivity changes rarely, so event-driven updates avoid a kernel round-trip every tick while keeping `collect()` cheap. Seeding before registration makes the first `collect()` accurate without waiting for the asynchronous initial callback, and ordering it before registration means a newer callback value can never be overwritten by the seed; the initial notification still covers a change between the two. `CancelMibChangeNotify2` waits for in-flight callbacks, so destroying the handle first guarantees the callback never touches a destroyed cache. The Windows subscription class lives in the `.cpp` so no Windows types appear in `connectivity_collector.h`. Injecting the subscriber, like the reader, lets unit tests deliver notifications and verify seeding order, cancellation on destruction, and the polling fallback (resolving [known deviation D-5](known_deviations.md#d-5-notification-callback-wiring-has-no-unit-test)). Logging only on change follows the architecture's `info` level for connectivity state changes without repeating the same state every tick.

---

### Explicit `ConnectivityLevel::Unknown` and optional `isMetered`

**Decision:** Add `ConnectivityLevel::Unknown` as the default level and make `ConnectivityStatus::isMetered` a `std::optional<bool>`. The collector reports `Unknown` / `std::nullopt` before the first successful read, after a failed read, and when Windows reports `NetworkConnectivityLevelHintUnknown`, `NetworkConnectivityLevelHintHidden`, or `NetworkConnectivityCostHintUnknown`. A failed read does not keep the previous value.

**Rationale:** The guidelines forbid substituting a default for missing data. With only `None`, "we could not determine connectivity" was indistinguishable from "offline", and a retained previous value would present stale data as current. `Unknown` is an enum value rather than wrapping the whole status in `std::optional` so the scheduler, snapshot, and UI keep a single value type and the UI can render the state directly (for example "Unknown" or a dimmed indicator).

---

### Windows-to-domain connectivity mapping in a separate, testable header

**Decision:** The pure conversions `toConnectivityLevel`, `toIsMetered`, and `toConnectivityStatus` live in `platform/windows/connectivity_hint_mapping.h/.cpp`, which exposes Windows SDK types, and are unit tested directly.

**Rationale:** The mapping is the part most likely to be wrong and is impossible to exercise from a host integration test, because the host's connectivity state cannot be forced. Keeping it separate keeps `connectivity_collector.h` free of Windows types. The tests live in the separate `tests/platform/` target (`system_monitor_platform_tests`), which may include Windows SDK headers but makes no API calls, so `tests/unit/` stays free of Windows headers (resolving [known deviation D-4](known_deviations.md#d-4-a-unit-test-includes-windows-sdk-headers)). A separate target was chosen over mirroring SDK constants as plain integers, which would lose the compile-time link to the real SDK values. Note that `netioapi.h` must be reached through `<iphlpapi.h>`, and `CancelMibChangeNotify2` is only declared when `<ws2tcpip.h>` is included first.

---

### System uptime: `GetTickCount64` as a fast collector with an optional snapshot field

**Decision:** `UptimeCollector` returns `std::chrono::milliseconds` from `GetTickCount64` and runs on the scheduler tick as a fast collector. `SystemSnapshot::uptime` is `std::optional<std::chrono::milliseconds>`, empty when no uptime collector is registered.

**Rationale:** `GetTickCount64` is a trivial, non-failing call with a 64-bit counter, so unlike `GetTickCount` it does not wrap after 49.7 days. Its 10–16 ms resolution is irrelevant for displaying uptime. It counts time spent in sleep and hibernation, which is what users expect from "uptime". `QueryUnbiasedInterruptTime` was rejected because it excludes sleep time. The snapshot field is optional so a missing collector is explicit rather than a zero uptime (per the domain rules). Uptime is computed from a monotonic tick counter, not by subtracting a boot timestamp from wall-clock time, so it is unaffected by clock changes.

---

### No zero placeholders for CPU and memory: optional samples end to end

**Decision:** `ICpuCollector` and `IMemoryCollector` return `std::optional<CpuSample>` / `std::optional<MemorySample>`, and `SystemSnapshot::cpu` / `::memory` are optional. `CpuCollector` returns `std::nullopt` when both queries fail and on the sample that only establishes its baseline. `calculateCpuUsage` returns `std::nullopt` for non-positive elapsed time, any counter that went backwards, or zero elapsed CPU time. Per-core usages are all-or-nothing: an empty `coreUsagePercents` means "unavailable" (after a core-count change or an uncomputable core), never zero-filled. `calculateMemorySample` returns `std::nullopt` for a zero physical-memory total, and `MemoryCollector` returns it when `GlobalMemoryStatusEx` fails. Likewise, `calculateDiskSample` returns `std::nullopt` for a zero-capacity volume and `DiskCollector` skips it, as it already skipped volumes whose query fails ([D-8](known_deviations.md#d-8-zero-capacity-disk-volumes-report-0-usage)). The dashboard shows "N/A" for missing values. Resolves [known deviation D-7](known_deviations.md#d-7-memory-and-cpu-data-report-zeros-when-missing).

**Rationale:** The domain rules forbid representing missing data as zero. A 0% CPU or 0 GiB memory reading is indistinguishable from a real idle system or a broken query, and it would also mislead the upcoming threshold-based health status (a missing sample must not read as "healthy"). A genuine 0% (a fully idle interval) is still reported as 0%. Rejecting deltas across a counter reset also fixes a latent bug: if only one counter went backwards, the old saturating subtraction could report a meaningless rate such as 100%. Per-core values are all-or-nothing because a partially filled list would misalign core indices.

---

### Optional collections: a failed query is not an empty result

**Decision:** `IDiskCollector`, `INetworkCollector`, and `IProcessCollector` return `std::optional<std::vector<...>>`, and `SystemSnapshot::disks`, `::networks`, and `::processes` are optional. `std::nullopt` means no collector is registered or the query failed. An empty vector means the query succeeded and found nothing. `DiskCollector` returns `std::nullopt` when `GetLogicalDrives` fails, and also when fixed drives exist but none of them could be read. `NetworkCollector` returns it when `GetIfTable2` fails. The dashboard shows "N/A" for missing network data and reserves "No active adapter" for a successful read with no hardware adapter Up.

**Rationale:** With plain vectors a failed query looked the same as a machine with no volumes or no adapters, and the network card stated "No active adapter" when nothing had been observed. That is the same missing-data-as-default problem that D-6, D-7, and D-8 removed for single values ([code review F-12](code_reviews/phase_2.md#f-12-empty-disk-and-network-vectors-hide-collector-failures)). `IProcessCollector` has no implementation yet but takes the same shape now, so the Phase 4 process collector starts with it. A partial disk result, where some volumes are readable, is still reported as a vector, because the readable volumes are real data. The unreadable ones are skipped and logged, as before.

---

### Health evaluation: domain types, a pure evaluator in `monitoring`, and the result carried in the snapshot

**Decision:** `domain/health_status.h` defines `HealthLevel` (Unknown, Healthy, Warning, Critical) and `SystemHealth` (cpu, memory, disk, network, overall). `monitoring/health_evaluator.h` provides the pure function `evaluateHealth(snapshot, HealthThresholds)`, and `SamplingScheduler` calls it at the end of each `sampleOnce()` to fill `SystemSnapshot::health`. Thresholds are strictly "above": warning above 90% and critical above 95% for memory and disk. CPU is critical only above 98%, because short full-load bursts are normal. Disk health is the worst fixed volume. Network health comes from connectivity: InternetAccess is Healthy, any lesser level is Warning. Overall is Critical if any component is Critical, else Warning if any is Warning, else Unknown if any is Unknown, else Healthy. Thresholds are set on the scheduler (`setHealthThresholds`, stopped-only like collector registration) so the Phase 6 settings file can supply them later.

**Rationale:** The architecture places the "health/connectivity evaluator" in `monitoring`, and `domain/AGENTS.md` lists health states as domain types. The domain target is header-only, so it holds only the types. The UI may not depend on `monitoring`, so it cannot call the evaluator. Carrying the result in the snapshot keeps the UI a pure renderer and makes health available to future consumers (tray icon, alerts) without duplicating logic. Unknown is a first-class level so missing data is never reported as Healthy, but a known problem still surfaces when other components lack data. Health is evaluated from each instantaneous sample with no smoothing or hysteresis, so CPU can flicker between levels under bursty load. Sustained-threshold rules belong with the planned alert rules (see the roadmap's alert-rules section).

---

### Dashboard cards: `ResourceCard` widgets fed by pure formatting helpers

**Decision:** The dashboard is an overall "System status" line plus five `ResourceCard` widgets (CPU, memory, disk, network, uptime) in a three-column grid with equal column stretch. Each card has a title, a large value, a detail line, a colour-coded health status label (hidden for uptime, which has no health dimension), and a fixed-height slot reserved for a mini sparkline. All text comes from pure functions in `ui/dashboard_formatting.h`, which are unit tested without widgets. Before the first snapshot a card shows "--". When a snapshot lacks the data it shows "N/A", never a zero. Specific rules:
- Byte quantities use binary (IEC) units (KiB, MiB, GiB).
- Disk shows the fullest volume.
- Network shows total throughput over hardware adapters that are Up. Virtual adapters (VPN tunnels, virtual switches) are left out because their traffic also crosses a hardware adapter. It shows "N/A" if any active hardware adapter has no rate yet, because a partial sum would understate traffic, and "No active adapter" when no hardware adapter is Up. Connectivity is the detail line.
- String literals are ASCII only (no arrows or middle dots) because the MSVC build does not pass `/utf-8`.

**Rationale:** A reusable card keeps the five resources visually consistent and reserves room for the upcoming mini sparkline so adding it does not re-lay out the dashboard. Keeping formatting and aggregation in pure functions keeps `DashboardView` thin and makes edge cases (unit boundaries, missing rates, no volumes) testable. Health comes precomputed in `SystemSnapshot::health`, so the UI only renders it, in line with `src/ui/AGENTS.md`. Binary units match how Windows reports memory and disk sizes. Other tabs are still placeholders, so snapshot forwarding to them remains deferred (code review F-4).

---

### `RingBuffer<T>` lives in `domain` so the UI can use it

**Decision:** Move `RingBuffer<T>` from `monitoring` to `domain` (`domain/ring_buffer.h`, namespace `sysmon::domain`) with no change to its behaviour. The dashboard's mini sparklines keep their own short presentation history in `RingBuffer`s owned by the UI. Longer histories (e.g. for the Performance view or alert rules) may still live in `monitoring`, using the same type.

**Rationale:** The UI must not depend on `monitoring` (`src/ui/AGENTS.md`), but sparklines need a bounded history with the contiguous-span access `RingBuffer` was designed for. `RingBuffer` is header-only and uses only the standard library, so it already meets the domain's dependency rules, and it had no production users, so the move is cheap. Alternatives considered:
- A UI-only `std::deque` history would duplicate `RingBuffer` with a less efficient design.
- Carrying history inside every `SystemSnapshot` is cheap for the dashboard but grows with Phase 3's longer, per-core histories.
- A new header-only `src/common/` module is semantically cleaner, since `domain` otherwise holds data types. It is deferred until a second shared utility justifies a new module; see the "Code organization" item in the roadmap.

---

### Minimal `SparklineWidget` built early for the dashboard, extended in Phase 3

**Decision:** Build a minimal single-series `SparklineWidget` in `src/ui/charts/` (its own `sysmon_ui_charts` library, linked by `sysmon_ui`) for the Phase 2 dashboard, rather than deferring mini sparklines to Phase 3. It has these properties:
- **Geometry:** a pure `sparklineSegments()` function maps samples to polyline segments and is unit tested without painting. The newest sample sits at the right edge, so a partial history grows in from the right.
- **Missing data:** samples are `std::optional<float>`, and a missing sample breaks the line (a visible gap) instead of plotting zero.
- **Y range:** fixed (0–100% for CPU, memory, and disk) or auto-scaled to the data with 10% headroom (network).
- **Ownership:** the caller owns the history. `DashboardView` keeps a 60-sample `domain::RingBuffer<std::optional<float>>` per card (one minute at 1 Hz) and passes a view of it with `setSamples()` each tick. The network sparkline charts combined in + out throughput.
- **Colour:** the line is a fixed neutral blue, not the system accent colour. On systems with a red accent, the accent would read as the "Critical" status colour.

**Rationale:** Roadmap item "each card shows a mini sparkline" belongs to Phase 2. The minimal widget covers exactly what the dashboard needs, and Phase 3 extends it (grid, multiple series, axis labels, annotations) rather than replacing it. Rendering from a caller-supplied view keeps the widget reusable for the Performance view's longer histories. Gaps follow the project rule against representing missing data as zero. Copying 60 samples per chart per tick is negligible.
