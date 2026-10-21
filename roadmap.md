# Verdandi — Roadmap

This roadmap expands the scope and milestones from [architecture.md](docs/architecture.md) into actionable implementation tasks. Each phase builds on the previous one. Tasks within a phase can often be worked on in parallel.

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
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines.

---

## Phase 1 — Domain types and synthetic data pipeline

Define the core data types and prove the end-to-end data flow (scheduler → snapshot → UI) using synthetic/fake data before connecting to real Windows APIs.

### Domain types (`src/domain/`)

- [x] Define `CpuSample`: total usage percent, per-core usage percentages, core count.
- [x] Define `MemorySample`: total bytes, available bytes, usage percent, commit limit, commit current.
- [x] Define `DiskSample`: per-volume total bytes, free bytes, usage percent.
- [x] Define `NetworkSample`: per-adapter in/out bytes (cumulative), link speed, operational status, adapter name.
- [x] Define `ConnectivityStatus`: connectivity level enum (None, LocalAccess, ConstrainedInternetAccess, InternetAccess), metered flag.
- [x] Define `ProcessInfo`: PID, creation time, parent PID, image name, image path (optional), command line (optional), CPU user/kernel times, working set, private bytes, I/O read/write bytes, thread count, handle count, access-denied flag.
- [x] Define error/access types: represent missing or inaccessible data explicitly using `std::optional` or error variants.
- [x] Write unit tests for all domain types (construction, equality, serialization where needed).

### Monitoring infrastructure (`src/monitoring/`)

- [x] Define `ICollector` interface (or concept) with a `collect()` method returning a typed result.
- [x] Implement `RingBuffer<T>` — fixed-capacity circular buffer for time-series samples. Unit test capacity limits, overwrite behavior, and iteration.
- [x] Implement `SamplingScheduler` — owns a `std::jthread`, ticks at a configured interval, calls registered collectors, assembles `SystemSnapshot`, emits via Qt signal. Uses `std::stop_token` for clean shutdown.
- [x] Implement a `SyntheticCpuCollector` and `SyntheticMemoryCollector` that return fake data with realistic variation (e.g., sine wave CPU usage). These are for validating the pipeline and remain useful for testing and demos.
- [x] Wire the scheduler to the synthetic collectors. Write tests verifying snapshot assembly, timing, and shutdown.

### Basic UI shell (`src/ui/`)

- [x] Create a `QMainWindow` subclass with a tabbed layout (`QTabWidget`) for Dashboard, Performance, Processes, and Network views.
- [x] Create placeholder `QWidget` subclasses for each tab.
- [x] Wire the `SystemSnapshot` signal from the scheduler to the main window via `Qt::QueuedConnection`. Verify the snapshot arrives on the UI thread.
- [x] Display raw synthetic values (CPU %, memory %) as text labels on the dashboard placeholder to prove the pipeline works.
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines.

---

## Phase 2 — Real Windows collectors and dashboard

Replace synthetic collectors with real Windows API calls and build the dashboard view.

### CPU collector (`src/platform/windows/`)

- [x] Implement total CPU usage using `GetSystemTimes`. Calculate usage percent from idle/kernel/user time deltas over monotonic elapsed time.
- [x] Implement per-core CPU usage using `NtQuerySystemInformation(SystemProcessorPerformanceInformation)`. Resolve the function pointer dynamically from `ntdll.dll`.
- [x] Handle multi-processor-group systems (>64 logical cores) correctly.
- [x] Write integration tests that run on the host and tolerate variable load.

### Memory and disk collector (`src/platform/windows/`)

- [x] Implement memory sampling using `GlobalMemoryStatusEx`.
- [x] Implement disk space sampling using `GetDiskFreeSpaceExW` for all fixed drives.
- [x] Write integration tests.

### Network collector (`src/platform/windows/`)

- [x] Implement adapter throughput using `GetIfTable2`. Use 64-bit counters. Filter loopback. Calculate bytes/sec from deltas.
- [x] Implement adapter addresses and DNS servers using `GetAdaptersAddresses`.
- [x] Implement connectivity status using `GetNetworkConnectivityHint`. Set up `NotifyNetworkConnectivityHintChange` for event-driven updates.
- [x] Free resources correctly (`FreeMibTable`).
- [x] Write integration tests tolerant of varying network environments.

### Dashboard view (`src/ui/`)

- [x] Design a dashboard layout with resource cards: CPU, memory, disk, network, uptime.
- [x] Each card shows current value, a mini sparkline, and a brief status label.
- [x] Display system uptime using `GetTickCount64`.
- [x] Display basic health status derived from resource thresholds (e.g., memory >90% → warning).
- [x] Connect real collector data through the snapshot pipeline to the dashboard widgets (see [code review Finding F-4](docs/code_reviews/phase_1.md#f-4-mainwindowonsnapshotready-only-updates-dashboardview) for snapshot distribution across views and visible-tab update optimization).
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines. Check [known_deviations.md](docs/known_deviations.md) for open entries (D-1 and D-2 are planned for Phase 4). See the [Phase 2 code review](docs/code_reviews/phase_2.md).

---

## Phase 3 — Sparkline charts and performance views

### Custom sparkline widget (`src/ui/charts/`)

- [x] Implement `SparklineWidget` as a `QWidget` subclass overriding `paintEvent`. (Minimal single-series version built early for the Phase 2 dashboard.)
- [x] Draw a `QPainterPath` polyline from ring buffer data.
- [x] Fill below the line with a `QLinearGradient`.
- [x] Draw a scrolling grid background offset by the sample index.
- [x] Support configurable Y-axis range (0–100% for CPU/memory, auto-scale for network throughput).
- [x] Support multiple series on one chart (e.g., per-core CPU).
- [x] Display axis labels, current value, and min/max annotations.
- [x] Test with synthetic data at various buffer sizes (60, 300, 1800 samples).

### Performance tab (`src/ui/`)

- [x] Create a performance view with full-size sparkline charts for CPU (total and per-core), memory (usage and commit), disk (space per volume), and network (throughput per adapter). Extend `MainWindow::onSnapshotReady` to forward snapshots to this view, and decide on visible-tab-only updates (see [code review Finding F-4](docs/code_reviews/phase_1.md#f-4-mainwindowonsnapshotready-only-updates-dashboardview)).
- [x] Show detailed numeric readouts alongside each chart (e.g., speed, processes, uptime, handles for CPU; total, cached, paged pool for memory).
- [x] Use a left sidebar or selector to switch between CPU, memory, disk, and network detail views.
- [x] Verify smooth 1 Hz updates with 5–30 minutes of history. Re-measure with `.\scripts\build.ps1 -Benchmark` (see [DC-66](docs/design_decisions.md#dc-66)).
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines. See the [Phase 3 code review](docs/code_reviews/phase_3.md); fix F-20 to F-22 before starting Phase 4.

---

## Phase 4 — Process explorer

### Process collector (`src/platform/windows/`)

- [x] Implement process enumeration using `NtQuerySystemInformation(SystemProcessInformation)`. Dynamically resolve from `ntdll.dll`. Use the resizing-buffer loop pattern.
- [x] Parse the linked list of `SYSTEM_PROCESS_INFORMATION` structs to extract PID, parent PID, creation time, image name, thread count, handle count, working set, private working set, private bytes, I/O counters, and CPU times.
- [x] Implement the process identity cache keyed by `(PID, creation time)`.
- [x] On new process detection, query `QueryFullProcessImageNameW` and `NtQueryInformationProcess(ProcessCommandLineInformation)` via `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)`. Cache the result.
- [x] Skip opening handles to `lsass.exe` and `csrss.exe` to avoid EDR false positives. Use data from `NtQuerySystemInformation` directly for these.
- [x] Calculate per-process CPU usage percent from CPU time deltas and monotonic elapsed time.
- [x] Run the process collector on a slow-collector thread (every 2–5 seconds). Move the connectivity collector (including its polling fallback) onto the same thread. Resolves [D-1 and D-2](docs/known_deviations.md).
- [x] Write integration tests verifying enumeration, cache behavior, and access-denied handling.

### Process explorer view (`src/ui/`)

- [x] Create a process table using `QTreeView` + a custom `QAbstractItemModel` subclass.
- [x] Display columns: Name, PID, CPU %, Memory (private working set, as Task Manager shows it; see [DC-79](docs/design_decisions.md#dc-79)), Disk I/O (read + write bytes/sec), Network (placeholder until ETW), Threads, Handles.
- [x] Implement a `QSortFilterProxyModel` for column sorting and text filtering.
- [x] Implement process tree view: build parent-child relationships from parent PID. Allow toggling between flat and tree views.
- [x] Efficiently diff/update the model on each snapshot rather than rebuilding the entire model.
- [x] Highlight new processes briefly (e.g., green flash) and mark terminated processes before removal.
- [x] Display an access indicator for processes where path/command-line could not be queried.

### Process termination (unelevated only)

- [x] Implement `TerminateProcess` for processes the current user owns.
- [x] Show a confirmation dialog before termination with the process name, PID, and a warning.
- [x] Handle failure gracefully (process already exited, access denied) with a user-facing message.
- [x] Do not attempt to terminate elevated or system processes in the MVP.
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines. See the [Phase 4 code review](docs/code_reviews/phase_4.md); fix F-29 before starting Phase 5.

---

## Phase 5 — Network view

- [x] Create a network tab showing all non-loopback adapters in a list or card layout. Adapters that Windows hides, and virtual adapters that are not bound to IP, are listed with "Show all adapters" ([DC-96](docs/design_decisions.md#dc-96)).
- [x] Per adapter: name/alias, type (Ethernet/Wi-Fi/etc.), operational status, link speed, IP addresses (IPv4 and IPv6), DNS servers ([DC-96](docs/design_decisions.md#dc-96)).
- [x] Per adapter: live throughput sparklines (inbound and outbound bytes/sec). One minute per card; `.\scripts\build.ps1 -Benchmark` measures the tab's tick cost ([DC-98](docs/design_decisions.md#dc-98)).
- [x] Display overall connectivity status from `GetNetworkConnectivityHint`: connectivity level, metered/unmetered, roaming. Render `ConnectivityLevel::Unknown` and an unknown metered state (`isMetered == std::nullopt`) explicitly rather than as offline or unmetered ([DC-39](docs/design_decisions.md#dc-39)).
- [x] Optionally display friendly network name from NLM (lazy, cached query). Shown per adapter and in the connectivity section, with the network category ([DC-100](docs/design_decisions.md#dc-100)).
- [x] Update connectivity status reactively via `NotifyNetworkConnectivityHintChange` rather than polling. A notified change wakes the slow-collector thread, so it reaches the next snapshot ([DC-77](docs/design_decisions.md#dc-77)).
- [x] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines. See the [Phase 5 code review](docs/code_reviews/phase_5.md); fix F-35 to F-37 before starting Phase 6.

---

## Maintenance — design decisions and tests

Every phase adds to both. [design_decisions.md](docs/design_decisions.md) has 108 entries in 256 KB, and entries grew from about 150 words on average in the first 48 to about 430 in the 60 after them. The suite has 775 tests in 63 files. Do this before Phase 6, so the settings work starts from the new structure.

### Design decisions

- [x] Give every entry a short, stable anchor (`<a id="dc-89"></a>`) and change the links in code, `architecture.md`, `roadmap.md`, `known_deviations.md` and `docs/code_reviews/` to `#dc-89`. Links use the full title slug today, so merging or retitling an entry breaks them. Add a check (for example to `build.ps1 -CheckFormat`) that every `DC-xx` link resolves. Do this first; the items below merge and retitle entries. Every heading has its anchor, every link names one, and `build.ps1 -Format` and `-CheckFormat` fail on a link that does not resolve and on a cited `DC-xx` that is not an entry ([DC-114](docs/design_decisions.md#dc-114)).
- [x] Decide the entry model. Today the log is append-only: 36 notes ("Superseded in part", "Extended by", "Amended", …) patch earlier entries, so learning how End process works means reading four entries in turn. Recommended: *living entries*, where an entry states the current decision and is rewritten when it changes, with a one-line history (phase, code-review findings) and git for the rest. This replaces the rule at the top of the file that superseded entries stay in place. Done as recommended: an entry ends with a **History:** line, a merged or retired entry stays as a one-line stub, and `git log -L ':DC-76\::docs/design_decisions.md'` lists an entry's versions. The top of the file and the rule in AGENTS.md and GEMINI.md say so. The notes in DC-18, 21, 46, 71, 76 and 111 are folded in; the items below rewrite the other entries with notes ([DC-115](docs/design_decisions.md#dc-115)).
- [x] Group entries by topic instead of by phase (platform APIs, domain model, scheduling and threading, each collector area, charts, each tab, build and tooling, testing), and give each entry its **History:** line with the phase and code-review finding ([DC-115](docs/design_decisions.md#dc-115)). The phase headings already misfile entries: the one for code review F-32 sits among the process collector's, and those for F-29, F-30 and F-33 under the explorer view. Done: 18 topics, each with a line that says what it covers, and the entries in ID order within each, so a new entry goes at the end of its topic, as the rule in AGENTS.md and GEMINI.md now says. Every entry ends with its history line, and DC-70's correction note is folded in ([DC-116](docs/design_decisions.md#dc-116)).
- [x] Merge each chain into one entry per topic, with the chain's notes folded in, leaving the other IDs as one-line stubs that point to it. Candidates:
  - process details status and never-opened processes: DC-13, 72, 74, 94;
  - termination checks: DC-87, 90; End process UI: DC-88, 89, 93;
  - process table: DC-79 to 84, 91, 92 (at most three entries: model and updates, sorting and filtering, tree and highlights);
  - connectivity: DC-39, 40, 77, 99, 101; network names: DC-06, 100, 107;
  - adapter data: DC-35, 36, 59, 64, 103, 108; Network tab: DC-96, 97, 106;
  - sparkline widget: DC-48 to 52, 60, 65, 67;
  - missing data as `std::optional`: DC-12, 40, 43, 44;
  - testing approach: reader injection (DC-27, 33, 38) and integration tests (DC-30, 78).

  Done: 34 entries are now stubs, merged into DC-12, DC-13, DC-27, DC-30, DC-35, DC-39, DC-48, DC-77, DC-79, DC-81, DC-82, DC-87, DC-88, DC-96 and DC-100. The lowest ID under a topic keeps the decision. The merged entries keep the test lists, benchmark tables and UI wording that the next item moves out, so several run to 1,500–2,100 words until then. A chain that spans topics became one entry under each: the slow-collector thread stays in DC-77 under the scheduler, apart from the connectivity collector in DC-39, and DC-06 stays the Windows API choice, with its note folded in. A stub whose decision went to two entries names both. Links and citations name the merged entries, except in `docs/code_reviews/`, and `build.ps1 -Format` and `-CheckFormat` now fail on one that names a stub ([DC-114](docs/design_decisions.md#dc-114), [DC-115](docs/design_decisions.md#dc-115)).
- [x] Move content that is not a decision out of the entries:
  - Benchmark tables (DC-61, 66, 79, 95, 98, 104) go to a new `docs/performance.md`: one current baseline per tab with the environment lines `build.ps1 -Benchmark` prints, and a short dated history. `architecture.md` quotes figures in two places and should link there instead.
  - Test inventories ("Tests:" bullets and paragraphs in DC-13, 35, 79, 82, 87, 88, 96, 100, 102, 104 and 105) are dropped. The test names list the cases; an entry names at most the test suite.
  - UI wording copied from the code (the message table and dialog texts in DC-88, the Memory column's tooltips in DC-79, and the texts for a process without a name in DC-13) is replaced by the name of the formatter function.
  - Build-script behaviour (DC-62, 69, and DC-23's mechanics) goes to [build_system.md](docs/build_system.md); the entries keep only the rationale.

  Done: [performance.md](docs/performance.md) says what each benchmark measures and how to record a run, and has a baseline of all three tabs from three runs on 2026-10-18 with the environment lines, and a dated history of the earlier tables. `architecture.md` links there in three places, the process table's figures included. DC-66 describes both benchmarks, with its notes folded in, and DC-61 and DC-95 no longer promise figures in their titles. `build_system.md` now has the configure stamp, the retry's limits, which files `-Format` covers and how `bootstrap.ps1` installs clang-format. The test lists are dropped, except where how something is tested is a decision of its own, such as DC-87's unelevated children. DC-13, DC-79 and DC-88 name the functions and constants that give their texts, such as `processTerminationMessage()` ([DC-117](docs/design_decisions.md#dc-117)).
- [x] Retire entries that restate a rule kept elsewhere or record where code lives, leaving stubs: the one that keeps Windows headers out of headers (an AGENTS.md rule), the one on where shared UI constants live, and those on plain text and text colours once their rationale is in the coding guidelines' "Text from outside the program" and "Text colours" sections, which already state the rules. Done: the four are stubs titled "Retired", each with a line that says where its rule is. The two text sections have their reasons, with the measurement behind each choice. The Windows-header entry no longer held as written, because eight `platform/windows` headers include Windows headers, so "Windows API wrapping" now says which headers may: none that another module or `tests/unit/` includes. The headers that the shared-constants entry listed already say what they hold, and the charts' own "N/A" now has a comment that says why. Links and code comments name the guidelines instead ([DC-117](docs/design_decisions.md#dc-117)).
- [x] Set a format and a budget, at the top of the file and in the design-decision rule of AGENTS.md and its copy in GEMINI.md: **Decision** (what was chosen), **Rationale** (why, and the alternatives rejected), optional **Consequences**, then the **History:** line; about 250 words; no test lists, benchmark tables, UI copy, or measurements beyond the one that justifies the choice. [DC-117](docs/design_decisions.md#dc-117) already says where that content goes; extend it with the format and the budget. Done: the top of the file, DC-117 and a rule of its own in AGENTS.md and GEMINI.md give the four fields, each at the start of a line, and about 250 words without the heading and the History line. The budget is per entry, merged ones included: a merged entry gives each of its parts a sentence. `build.ps1 -Format` and `-CheckFormat` now also fail on a field that is missing, unknown or out of order, on text before the Decision or after the History line, and on a stub of more than one line. They do not count words. All 79 entries passed as they were. DC-117 is rewritten within its own budget, from about 620 words to about 290, and drops its list of what the earlier items moved, which the History lines and git keep ([DC-117](docs/design_decisions.md#dc-117)).
- [x] Bring the entries over the budget within it ([DC-117](docs/design_decisions.md#dc-117)). On 2026-10-18, 28 of the 79 entries ran over 300 words, not counting the heading and the History line: DC-48, DC-88, DC-96, DC-35, DC-79, DC-87, DC-82 and DC-100 had 1,100 to 1,900 each; DC-115, DC-13, DC-77, DC-39, DC-81 and DC-12 550 to 850; and DC-30, DC-76, DC-85, DC-71, DC-58, DC-46, DC-75, DC-114, DC-53, DC-98, DC-57, DC-63, DC-116 and DC-66 310 to 480. Most of the excess describes how the code carries out the choice, such as DC-48's account of each annotation and colour step, which the code and its comments already say. Much of the rest gives every alternative and measurement where the one that settled the choice is enough. Rewrite them longest first, keeping each decision and the reason it turns on, and add **Consequences:** only where a cost is not in the Rationale. Done: the 28 now have 195 to 300 words each, and the 79 entries average about 205 instead of 380, so `design_decisions.md` is down from 243 KB to 152 KB. The steps that the code's comments already describe are gone, such as the order of DC-87's checks and DC-88's dialog signals, and each entry keeps at most the one measurement that settled its choice. A merged entry gives each part a sentence, so each stub's line still names something its entry says, such as DC-48's extremes named after the primary series. Six entries have a **Consequences:** line for a cost the Rationale left out: DC-13 (the never-opened check trusts the name it is given), DC-58, DC-63, DC-76 (the CPU column adds up to less than the total), DC-87 (with UAC off, nothing can be ended) and DC-100 (shutdown waits for a stuck read). DC-115 drops its list of what the earlier items applied, which the History lines and git keep. DC-117 says the entries now average about 200 words, and its History line has this item. DC-77's History line now names the known deviations D-1 and D-2, which its Decision named before; no other History line changes, because no decision did.

### Tests

- [x] Add `tests/support/` with shared test-data builders and harnesses, and remove the copies: six different `adapter()` builders (in the adapter throughput history, dashboard formatting, network formatting, network view, performance formatting and performance view tests), four `process()` builders, two each of `valueOf()`, `snapshotWith()`, `populatedSnapshot()` and `demandingSnapshot()`, two `FakeSubscription` classes, and the snapshot-waiting loop that the five `*Integration.SchedulerPipeline_*` tests and `ProcessTrackingIntegration.RealHost_SchedulerWithRealCollector_SnapshotsCarryProcessList` each repeat ([DC-109](docs/design_decisions.md#dc-109)).
- [x] Test each rule at the layer that owns it. When a test is merged or removed, name in the commit message the test that still covers its behaviour.
  - Adapter order: `listedAdapters()` and `chartedAdapters()` both sort with `isListedBefore()`, yet the order is tested through each of them as well as through `compareNaturally()`. Test `isListedBefore()` once, and each list function only for which adapters it keeps.
  - Connectivity: six `ConnectivityCollector.Collect_*` tests (one per level, metered, unknown cost) check that the collector passes on a value from its reader that it never inspects. The mapping is `ConnectivityHintMapping`'s job; one pass-through test is enough.
  - UI wording: view tests assert the same strings as the formatter tests, e.g. "Do you want to end explorer.exe (PID 800)?" in both `processes_view_test.cpp` and `process_formatting_test.cpp`, so a change of wording breaks both. View tests compare with the formatter's output and check wiring, icons and modality.
  - Scheduler pipeline: replace the five `*Integration.SchedulerPipeline_*` tests with one that registers every real collector as `main.cpp` does and checks each snapshot field. Both use `registerSystemCollectors()` ([DC-110](docs/design_decisions.md#dc-110)).
- [x] Remove tests of no project behaviour: the domain `DesignatedInit_FieldsRoundTrip` and `EnumValues_AreDistinct` tests (10) check aggregate initialization and enum values, which the compiler guarantees. Keep the default-value tests that encode decisions, such as `ConnectivityLevel::Unknown`. Decide whether the synthetic collectors, which only tests use, move from `src/monitoring/` to `tests/support/` and lose their own tests. They are removed, and tests use fixed-value fakes in `tests/support/fake_collectors.h` ([DC-111](docs/design_decisions.md#dc-111)).
- [x] Make per-value tests of mappings and thresholds table-driven (`TEST_P`, or a loop with `SCOPED_TRACE`), as `SparklineBufferSize` already is: `ConnectivityHintMapping` (14 tests), `HealthEvaluator.EvaluateUsage_*` (6), `AdapterTypeMapping` (6), `ProcessElevation` (5) and `ProcessAccessPolicy.IsNeverOpenedProcess_*` (5). Each is one loop with `SCOPED_TRACE` over a table of named cases ([DC-112](docs/design_decisions.md#dc-112)).
- [x] Split the largest files by concern: `process_model_test.cpp` (1,198 lines, 52 tests: flat updates and signals, tree, highlights, roles), `sampling_scheduler_test.cpp` (901 lines: tick, slow-collector thread, event-driven collectors) and `sparkline_widget_test.cpp` (774 lines: pure geometry, then the widget).
- [x] Decide how `build.ps1 -Test` runs the tests. `gtest_discover_tests` starts a process per test, and each `system_monitor_tests` process creates a `QApplication` and loads its plugins. On 2026-10-16 (Debug), the 775 tests took 194 s of process time, 12.6 s on 16 threads, while the two test binaries ran all of them in one process each in about 9.3 s in total. [DC-62](docs/design_decisions.md#dc-62) measured about 50 ms per process; most now take 0.1–0.5 s. Options: parallel gtest shards (`GTEST_TOTAL_SHARDS`) started by `build.ps1`, or one CTest test per suite. Check first that the tests pass in any order (`--gtest_shuffle`). They run as GoogleTest shards, one CTest test each, in 2.1 to 2.6 s instead of 11.7 s ([DC-113](docs/design_decisions.md#dc-113)).
- [x] Add these rules to the testing section of [coding_guidelines.md](docs/coding_guidelines.md): shared builders in `tests/support/`, each rule tested at one layer, table-driven tests for mappings, no tests of aggregate initialization. Fix its fake-collector example, which returns a `CollectResult` type that does not exist; collectors return `std::optional` samples.
- [x] Remove the `Collector<C, T>` concept from `src/monitoring/collector.h`. No production code uses it. The scheduler holds `std::unique_ptr<ICollector<T>>`, and every collector derives from an `ICollector<T>` alias, whose `override` already checks `collect()`'s signature at compile time. Its only users were the `static_assert`s in the deleted synthetic collector tests. [DC-111](docs/design_decisions.md#dc-111) moved them onto the fakes in `tests/support/fake_collectors.h`, so the concept still compiles against something. It was not removed then because it is production code outside the test items, and [DC-18](docs/design_decisions.md#dc-18) records the interface and the concept as one decision. Recommended: delete the concept, the `<concepts>` include and the two `static_assert`s, and note in DC-18 that the interface alone remains. If a template ever needs to take collectors without virtual calls, write a concept for that use then. Done as recommended; DC-18 and DC-111 have notes.

### Coverage and test statistics

- [x] Add a coverage build that measures line and branch coverage. OpenCppCoverage, the usual free tool for MSVC builds, reports lines only, so compile with clang-cl and LLVM's source-based coverage (`-fprofile-instr-generate -fcoverage-mapping`). Visual Studio's "C++ Clang tools for Windows" component already has clang-cl, `lld-link`, `llvm-profdata`, `llvm-cov` and the profile runtime (22.1.3 on 2026-10-17). Add a `coverage` configure preset that instruments only the project's targets; the vcpkg packages stay as MSVC built them, and clang-cl code links against them. Where clang-cl rejects the code or warns, fix the code rather than silencing it. Make `bootstrap.ps1` install the component, and check that the `windows-latest` runner has it. First check whether Microsoft's `dotnet-coverage` reports branches for native code; if it does, the build can keep MSVC as its only compiler.

  Done: it does not. `dotnet-coverage` instruments only .NET modules, and Visual Studio's native coverage (`Microsoft.CodeCoverage.Console`) counts blocks and lines, so the build compiles with clang-cl. The `coverage` preset takes the `clang-cl.exe` of the Visual Studio installation whose environment is active, not one from `PATH`, and its `SYSMON_COVERAGE` option instruments every target of the project, tests included, so that every copy of a header's inline function is instrumented. vcpkg restored all 15 packages from the binary cache. clang-cl rejected two constructs that MSVC accepts. One was the default argument `= {}` of `NetworkProfileCollector`'s nested `ReadIntervals` inside its own class, which the default constructor now passes. The other was `static_cast<PROCESSINFOCLASS>(60)`, outside that enumeration's range, which is now a `ULONG` passed through a function-pointer type of its own. It also gave about 310 warnings, all about initializers that leave out a member without a default member initializer. The domain types, and the other structs concerned, now give each such member `{}`, and the Windows SDK structs are value-initialized and then given their size. The [coding guidelines](docs/coding_guidelines.md#default-member-initializers) and AGENTS.md state the rule. A clean build has no warnings, and the 33 shards pass in 2.4 s. `build.ps1 -Coverage` needed to exist to build the preset, so it already builds it and runs the tests: each test process writes `%p_%m.profraw` to `build\coverage\profiles`, and `%m` makes a process that reuses an earlier one's ID merge into its file instead of overwriting it. Merged by hand, the profiles gave 92.8% of the 6,139 lines in `src/` and 87.1% of its 2,178 branches. `bootstrap.ps1` adds the component (`Microsoft.VisualStudio.Component.VC.Llvm.Clang`) through the Visual Studio Installer, and no longer installs LLVM for clang-format, which the component has. The `windows-latest` runner, Windows Server 2025 with Visual Studio Enterprise 2026 18.10, has it ([DC-118](docs/design_decisions.md#dc-118)).
- [x] Make `build.ps1 -Coverage` report coverage. It already builds the `coverage` preset and runs the tests, with one raw profile per process ([DC-118](docs/design_decisions.md#dc-118)). Merge the profiles with `llvm-profdata merge` and report on `src/` alone, leaving out test, moc and vcpkg sources. `llvm-cov` warns that a few functions have mismatched data: inline functions that one binary compiles but never calls, which it records with hash 0. Each is counted from the binary that calls it, so the warning can be ignored or filtered out. Print line and branch coverage per module (`app`, `domain`, `monitoring`, `platform/windows`, `ui`, `ui/charts`) and in total, and write `llvm-cov show`'s HTML, which marks every line and branch of each file. The integration tests call real Windows APIs, so `platform/windows` coverage depends on the host and can differ between this machine and CI. Document the report in the script's help, [build_system.md](docs/build_system.md) and the build commands in AGENTS.md and its copies.

  Done: when the tests pass, `build.ps1 -Coverage` merges the profiles into `build\coverage\coverage.profdata`, prints the line and branch coverage of each directory under `src\` and in total, and writes `llvm-cov show`'s HTML to `build\coverage\html`, with each line's count and each branch's true and false counts. The report reads the application as well as the two test executables, so `main.cpp`, which no test runs, counts as not covered instead of being absent: on 2026-10-19, `src\` had 92.6% of 6,155 lines and 87.1% of 2,180 branches covered, and `app` 41.5% of its lines. Given `src` with a lower-case drive letter, as VS Code's terminal gives it, `llvm-cov` matched no file and reported every one, tests and moc files included, because it compares paths case-sensitively; so the script chooses the files under `src\` from `llvm-cov export` itself, regardless of case, and gives `llvm-cov show` the directory as the coverage data spells it. The mismatched-data warning is left out; with the application read, it also names `main()`, which has no profile. A template's instantiations are titled with MSVC's decorated names: `llvm-cxxfilt` leaves them as they are, and `undname` does not read standard input, as `llvm-cov` needs a demangler to. The coverage of `platform/windows` varies between runs on one machine, not only between machines: three runs gave 83.4% to 84.1% of its lines, because `process_details_reader.cpp` reads the details of whichever processes are running. Deciding whether coverage can fail the build needs to allow for that.
- [x] Write each shard's results: `sysmon_add_sharded_tests()` sets `GTEST_OUTPUT=json:<dir>/<target>_shard_<index>.json` with the shard index in the name, so that shards running in parallel never write the same file. GoogleTest's JSON gives each test's suite, name, result, time, and source file and line, which is enough to count unit, platform and integration tests by directory.

  Done: each shard writes to `test_results` in the build directory, in every preset, so CTest run directly, as in CI, writes the reports too ([DC-119](docs/design_decisions.md#dc-119)). Given only a directory, GoogleTest would choose a name that no file has yet when the process starts and write the file when it ends, so shards running together would choose the same one. A shard that crashes writes no report, so `build.ps1` removes the directory before the tests run, and an earlier run's report, or one from a build with more shards, is not read as this run's. On 2026-10-19 the 33 reports held 717 tests, each once and as many as the executables list: 623 in `tests\unit`, 30 in `tests\platform` and 64 in `tests\integration`. Two were skipped, the process terminator's integration tests that only an elevated test run can start their child for, and none is disabled. GoogleTest gives a test's `time` as a string of seconds with a unit, such as `"1.405s"`, and its `file` as the absolute path the compiler was given, so the dashboard should compare the path regardless of case, as the coverage report does.
- [x] Generate a dashboard in the same run: one self-contained HTML page, `build/coverage/index.html`, with inline SVG charts and no external scripts, so it opens from disk. A PowerShell 5.1 script writes it from the GoogleTest JSON, `llvm-cov export -summary-only` and a history file of one JSON line per run (date, commit, counts, time, coverage). The page shows:
  - tests by kind (unit, platform, integration) and by suite, with how many passed, failed, were skipped or are disabled;
  - the total time and the slowest tests (the times come from the instrumented clang-cl build, so compare them only with each other);
  - line and branch coverage per module and in total, and the least-covered files, each linking to its annotated source;
  - the trend of the test count, time and coverage, one point per run.

  Done: `scripts/coverage_dashboard.ps1` aggregates the reports and the summary, which `build.ps1` now saves as `build\coverage\coverage_summary.json`, and puts them as JSON into `scripts/coverage_dashboard.html`, whose inline script draws the tables and the SVG charts. The page has a row of figures with the change since the previous run, the tests by kind as a bar and a table, the failed, skipped and disabled tests with their messages, the 10 slowest tests, the tests by suite as a table that sorts by any column, the coverage of each directory and of each file (the 15 least covered first, all on request), each linking to `llvm-cov show`'s page, and three trend charts (tests; time added up and CTest's from start to end; line and branch coverage) with a tooltip per run and a table of the runs. It follows the colour scheme of the system. Each run whose tests pass first adds a line to `build\coverage\history.jsonl` with its date, commit and whether the working tree had changes, counts by kind, times, and covered and total lines and branches; a run whose tests fail adds none. `build.ps1` writes the page even when the tests fail, without coverage, then fails: with a failing test and a test that ended its process, both added for the check and removed, the page listed the failure with its message and named the shard that wrote no report, which it finds by comparing the reports with what `ctest --show-only=json-v1` says each shard was told to write. On 2026-10-19 the page was about 78 KB; it showed 717 tests whose times added up to about 20 s, which CTest ran in 2.3 s, and it opened from disk in Edge with no script errors, in both colour schemes and at phone width without scrolling sideways ([DC-120](docs/design_decisions.md#dc-120)).
- [x] Decide where the history lives, so the trend survives deleting `build/` and can be seen without building. Recommended: a CI job runs `build.ps1 -Coverage` on each push to `main`, appends the run to `history.jsonl` on a `gh-pages` branch, and publishes the dashboard with GitHub Pages, which is free for this public repository. Only that job gets `contents: write`; the others keep `read`. On pull requests the job uploads the dashboard as a workflow artifact and writes the change in coverage against `main` to the job summary. Local runs keep their own history in `build/coverage/`.

  Done as recommended ([DC-121](docs/design_decisions.md#dc-121)). A `Coverage` job in `ci.yml` runs `build.ps1 -Coverage` on each push to `main` and each pull request. First `scripts/coverage_pages.ps1 -Fetch` clones the `gh-pages` branch, or starts an empty clone when there is none yet, and `build.ps1`'s new `-CoverageHistoryPath` gives the dashboard the clone's `history.jsonl` instead of `build\coverage\history.jsonl`, which local runs keep using. After a passing run on `main`, `-Publish` replaces the clone's files with the dashboard and `llvm-cov`'s HTML, keeps the history that the run added to, adds `.nojekyll`, commits as `github-actions[bot]` and pushes; GitHub Pages serves the branch at <https://dundergg.github.io/system-monitor/> once it is turned on for it. The dashboard script also writes `build\coverage\summary.md`: the failures, or a table of the line and branch coverage and the test count beside those of the previous run in the history, which the job adds to its summary. On a pull request that run is `main`'s last, and the dashboard goes into the `coverage-dashboard` artifact. Only this job has `contents: write`, and its checkout keeps no token, so only the publishing step can push. Its steps run in Windows PowerShell 5.1, in which the scripts were tried: two rounds of fetching, a coverage run and publishing against a local bare repository made two commits on `gh-pages`, a history of two lines and a summary that compared them, and left `build\coverage\history.jsonl` unchanged, and all 101 links from the published dashboard to annotated sources resolved. Git for Windows' `core.autocrlf` would have checked the site out with CRLF line endings, so the clone turns it off. The workflow first runs when this is pushed. The job should take about as long as `Build and Test`, which on 2026-10-18 spent 48 of its 53 minutes configuring, because vcpkg built the dependencies instead of restoring them from a binary cache.
- [x] Decide whether coverage can fail the build. Recommended: report only, until the history shows how much coverage varies between runs. Then fail a pull request that lowers line or branch coverage of `src/` by more than one percentage point against `main`. A fixed minimum would either fail at once or stay silent until coverage had fallen far below today's.

  Done: report only, as recommended. `build.ps1 -Coverage` and the `Coverage` job fail only when the build or a test fails, until CI's history shows how much coverage varies on the runner ([DC-122](docs/design_decisions.md#dc-122)). That history does not exist yet: on 2026-10-19 the commit that adds the job had not been pushed, and there was no `gh-pages` branch. The only spread measured is this machine's: three runs of the same sources covered 5,685 to 5,700 of `src\`'s 6,155 lines (0.24 points) and 1,890 to 1,899 of its 2,180 branches (0.41 points). The later check is a fixed minimum instead of a limit on the fall from `main`, so that coverage cannot drift down in small steps, and the minimum is raised as coverage improves. It runs on pushes to `main` as well as on pull requests: none of the 154 commits came in through a pull request, so a check on pull requests alone would never run.
- [ ] Make the `Coverage` job fail when the line or branch coverage of `src/` falls below a fixed minimum ([DC-122](docs/design_decisions.md#dc-122)). Wait until the history on `gh-pages` has about ten runs on `main`; runs of commits that change nothing under `src/` or `tests/` show how much coverage varies by chance on the runner. Set each minimum to a whole percentage at least one point below the lowest run, so that chance alone never fails a run. This machine's runs on 2026-10-19 would give 91% of lines and 85% of branches. Fail the job on pushes to `main` and on pull requests, after the publishing step, so that a run below the minimum still enters the history and the dashboard. Recommended: `build.ps1 -Coverage` takes the minimums as parameters, so that a local run can check them too, and `summary.md` names them beside the coverage. Update DC-122, [build_system.md](docs/build_system.md) and the build commands in AGENTS.md and its copies. Raise the minimums whenever coverage has risen by a point or more.
- [ ] Raise the coverage of `src/`, starting with `platform/windows`. On 2026-10-19 a local run covered 92.5% of `src\`'s 6,155 lines and 87.0% of its 2,180 branches. `platform\windows` held 332 of the 457 lines and 182 of the 283 branches that no test ran, and covered 84.0% of its lines and 76.8% of its branches. Five of its files hold 259 of those lines and 142 of those branches: `cpu_collector.cpp` (102 lines, 44 branches), `process_terminator.cpp` (57, 23), `network_collector.cpp` (38, 35), `process_details_reader.cpp` (31, 20) and `network_profile_collector.cpp` (31, 20). What they miss is the readers' handling of a Windows call that fails: an error code, a buffer that is still too small after resizing, and fallbacks such as the CPU collector's whole path from `NtQuerySystemInformationEx` to `NtQuerySystemInformation`. A real machine seldom takes these paths, so the integration tests do not reach them, and the collectors' unit tests replace the readers whole ([DC-27](docs/design_decisions.md#dc-27)). Recommended: where a reader does more than one call and a check, pass it the Windows function, as the CPU collector's query helpers already take theirs inside an anonymous namespace, and test it in `tests/platform/` with a fake that fails, asks for a larger buffer or returns data; then extend DC-27 to that layer. Leave out a path whose injection would cost more code than it tests, such as `GetModuleHandleW` failing for `ntdll.dll`. `process_terminator.cpp`'s refusals of an elevated process run only in an elevated test run, which CI's may be, since GitHub's Windows runners run as an administrator, so compare with CI's report before writing tests for them. A path that unit tests cover also stops depending on the machine, which narrows the spread that [DC-122](docs/design_decisions.md#dc-122) waits for. Outside `platform\windows`, `parseLogLevel()` in `app_logging.cpp` has no test, and `SamplingScheduler::setInterval()` and `interval()` have no caller until the settings of Phase 6. Aim for 90% of `platform\windows`'s lines and 85% of its branches, which would bring `src\` to about 94.5% and 89.9%. Then set or raise the minimums of the item above.

### CI

- [ ] Restore vcpkg's packages from a cache in CI. On 2026-10-18 the `Build and Test` job spent 48 of its 53 minutes in its Configure step, where vcpkg builds what it cannot restore, and the run before took about as long in total; the `Coverage` job ([DC-121](docs/design_decisions.md#dc-121)) builds the same packages again on its own runner. [build_system.md](docs/build_system.md) says the workflow has "GitHub Actions-backed binary caching", but `run-vcpkg@v11` leaves the cache to vcpkg's `x-gha` provider, which vcpkg removed in its 2025-06-13 release ([vcpkg-tool#1662](https://github.com/microsoft/vcpkg-tool/pull/1662)), and the pinned vcpkg commit bootstraps the 2026-07-27 tool. Most likely each runner writes the packages to vcpkg's default local cache and loses them with the runner. Check first, in the Configure step's log, whether vcpkg restores or builds `qtbase`, and what `VCPKG_BINARY_SOURCES` is. Recommended: set `VCPKG_BINARY_SOURCES` to `clear;files,<dir>,readwrite` in both jobs, which `run-vcpkg` then leaves as it is, and keep `<dir>` with `actions/cache`, with a restore key of the key's prefix. vcpkg names each archive by its package's ABI hash, so a cache restored under an older key still serves every package that did not change, and the two jobs, which build the same `x64-windows` packages with MSVC, can share it. The key must change with `vcpkg.json`, `vcpkg-configuration.json` and the runner's MSVC version: the compiler is part of every ABI hash, so after a Visual Studio update an unchanged key would restore archives that no longer match and never save the rebuilt ones. Check the cache's size against the repository's 10 GB limit; GitHub also evicts a cache unused for 7 days, so a quiet week costs one full build. The alternative vcpkg recommends, NuGet on GitHub Packages, keeps packages indefinitely but needs `packages: write`, a NuGet source configured with the token, and more setup. Then correct build_system.md, and record the Configure step's time with a warm cache.

---

## Phase 6 — Settings and polish

- [x] Implement the JSON settings file: refresh interval, display units, enabled dashboard panels, log level.

  Done: `domain::Settings` (`src/domain/settings.h`) holds the four, and its defaults are what the monitor does without a file: a refresh interval of 1 s, binary byte units, network throughput in bytes per second, every dashboard card, and `info`. The display units are binary (KiB) or decimal (KB) byte units, and network throughput in bytes per second or in bits per second, which are always decimal, as link speeds are. The interval may be 0.5 to 10 s. The new `sysmon_persistence` library (`src/persistence/settings_file.h`), which has no Qt or Windows headers, writes the settings as JSON and reads them one setting at a time: a setting that is missing keeps its default, an interval outside the limits takes the nearer one, and any other invalid value or unknown key keeps the default and is returned as a problem for the caller to log, with the line and column of a syntax error. A save writes a temporary file that then replaces the file, so a save that fails leaves the previous file whole ([DC-123](docs/design_decisions.md#dc-123)). Nothing reads or writes the file yet. The "Load settings at startup" item below chooses where it lives, loads it and saves it on change. Applying it needs the byte and rate formatters to take the display units, the dashboard to hide the cards that are off, `SamplingScheduler::setInterval()` to take the interval, a mapping from `domain::LogLevel` to spdlog's levels, and a rule for whether `--log-level` overrides the file's level.
- [x] Create a settings dialog (`QDialog`) for editing preferences.

  Done: the main window has a File menu with Settings... and Exit. Settings... opens `ui::SettingsDialog` (`src/ui/settings_dialog.h`), window-modal, on a copy of `MainWindow::settings()`: the refresh interval in a spin box in seconds, the log level, the byte and network throughput units, and a check box for each dashboard card, with OK, Cancel and Restore Defaults. OK makes the dialog's settings the window's, and if they differ from the window's, `MainWindow` emits `settingsChanged()`. The spin box shows the interval to a tenth of a second with a decimal point whatever the locale, and an interval the user leaves as shown keeps the milliseconds it was given ([DC-124](docs/design_decisions.md#dc-124)). Nothing calls `setSettings()` or connects `settingsChanged()` yet, so the dialog opens with the defaults and its changes take no effect until the item below.
- [x] Load settings at startup with defaults and validation. Save on change. `main.cpp` passes the loaded settings to `MainWindow::setSettings()`, and saves and applies them on `MainWindow::settingsChanged()`.

  Done: `main.cpp` reads `settings.json` from the user's roaming application data folder (`%APPDATA%\System Monitor\System Monitor`) before it configures the logger, logs whether there was a file and each problem in it, and leaves a file with problems as it is until a setting changes. `--log-level` now has no default: given, it sets the level of that run over the file's, and is not saved. `app::SettingsService` (`src/app/settings_service.h`) receives each `settingsChanged()`: it applies the refresh interval to the scheduler, gives the logger the log level only if that changed, so the option's level holds while other settings change, and saves the file; a save that fails is logged and shown in a message box, and the settings apply until the monitor closes. The slow collectors run every 2 s, or every refresh interval if that is longer ([DC-127](docs/design_decisions.md#dc-127)). `MainWindow::setSettings()`, which OK in the dialog also goes through, applies what the views show: the dashboard hides the cards that are off, and the others fill the grid in order ([DC-125](docs/design_decisions.md#dc-125)); every size and rate is in the display units, and each view shows its latest values in new units at once ([DC-126](docs/design_decisions.md#dc-126)); and the charts' time labels and the "Chart history" windows tell the time their samples cover at the refresh interval, since the histories stay counted in samples ([DC-61](docs/design_decisions.md#dc-61)).
- [x] Enable per-monitor DPI awareness in the application manifest (`src/app/system_monitor.manifest`, added for the UTF-8 code page in [code review F-19](docs/code_reviews/phase_2.md#f-19-log-file-path-is-converted-to-the-ansi-code-page)).

  Done: the manifest sets `dpiAwareness` to `PerMonitorV2` ([DC-128](docs/design_decisions.md#dc-128)), and the linker merges it, with the UTF-8 code page and its default `asInvoker`, into the app, the tests and the benchmarks; `mt.exe` showed all three settings in the built `system_monitor.exe`. Qt 6.11 already asked for this awareness at run time, so the windows scale as before. Its `qt.qpa.window` log now shows that it finds the awareness set and keeps it, without a warning; given `-platform windows:dpiawareness=1`, it logs that `SetProcessDpiAwarenessContext()` was denied, and the process stays PerMonitorV2. The new integration test `ApplicationManifestIntegration.DpiAwareness_IsPerMonitorV2` reads the setting from the process's activation context; with the element removed from the manifest it failed with `ERROR_SXS_KEY_NOT_FOUND`, although the process was still PerMonitorV2 through Qt. On 2026-10-20 a throwaway test, removed afterwards, rendered every tab from real data at Qt scale factors of 1, 1.25, 1.5 and 2 (`QT_SCALE_FACTOR`): text, chart lines and the Processes tab's access icons were sharp, and nothing was clipped. This machine's monitors are at 100% and 150%; Windows' own 125% and 200% were not tried.
- [x] Implement sleep/resume detection (`WM_POWERBROADCAST`) — discard first sample and reset rate baselines on resume.

  Done: `app::PowerEventFilter` (`src/app/power_event_filter.h`), a Qt native event filter that `main.cpp` installs on the application, passes `WM_POWERBROADCAST`'s `PBT_APMSUSPEND` and `PBT_APMRESUMEAUTOMATIC`, which `platform::powerTransitionFromMessage()` reads, to `SamplingScheduler::onSystemSuspending()` and `onSystemResumed()` ([DC-129](docs/design_decisions.md#dc-129)). From the suspend to the resume neither scheduler thread collects, so no sample from before the sleep is the baseline of one after it, even when a thread runs again before the resume message arrives. The CPU, network and process collectors are now `monitoring::IRateCollector`s, whose `resetBaselines()` makes the next collection only record new baselines. After the resume, each thread calls it on its collectors before it collects again: the tick discards that snapshot and emits the next, a refresh interval later, and the slow-collector thread refreshes the process list at once and keeps it, with no rates until its next refresh, as at startup. A resume without a suspend resets the baselines too. The integration test `PowerEventFilterIntegration.PowerBroadcastToAWindow_SuspendsAndResumesTheScheduler` sends both messages to a Qt window and checks that they reach the scheduler; the machine was not put to sleep.
- [ ] Profile CPU and memory usage of the monitor itself on a lower-end system, including the UI tick cost from `.\scripts\build.ps1 -Benchmark`. Optimize if needed.
- [ ] Set `requestedExecutionLevel` to `asInvoker` explicitly in `src/app/verdandi.manifest`. (The MSVC linker already adds `asInvoker` by default; stating it in the manifest documents the requirement.)
- [ ] Perform phase code review to ensure all code committed in this phase complies with architecture, rules, and guidelines.

---

## Future phases (post-MVP)

> These features are on the roadmap but not scheduled. Each can be picked up independently once the MVP is stable.

The *Seen in* lines, and the monitors named in items, come from a comparison on 2026-10-17 with Task Manager, Resource Monitor, Process Explorer, TCPView, Autoruns, System Informer, Process Lasso, TrafficMonitor, HWiNFO, LibreHardwareMonitor, GlassWire, CrystalDiskInfo, Glances, Mission Center and btop. Items that need access beyond the current rules say so. Features of those monitors that conflict with the rules are listed under [Considered and not planned](#considered-and-not-planned).

### Elevated process termination

- [ ] Create a small helper executable that accepts a PID and action via command-line arguments.
- [ ] Launch the helper via `ShellExecuteEx` with the `runas` verb to trigger a UAC prompt.
- [ ] Show a shield icon in the process explorer next to actions that require elevation.
- [ ] Investigate letting the monitor end the user's elevated processes itself when it runs elevated, which the MVP refuses even then ([DC-87](docs/design_decisions.md#dc-87)).
- [ ] Use the same helper for the other actions below that need administrator rights (services, the machine's startup entries, closing a connection, and drive health if it needs it), so the monitor has one elevation path.

### Processes tab: columns and grouping

> *Seen in: Task Manager, Process Explorer, System Informer.*

- [ ] Let the user choose, order and size the columns from the header's context menu, and keep them and the sort order in the settings file.
- [ ] Add columns from data the process list already carries, which need no process handle: session ID, base priority, start time, total CPU time, peak working set, virtual size and page faults.
- [ ] Add columns that need the limited handle the details reader opens ([DC-75](docs/design_decisions.md#dc-75)): user name (`TokenUser`, as the termination check reads it), elevated, integrity level, architecture (`GetProcessInformation(ProcessMachineTypeInfo)`, which also tells x64 emulation on Arm64 apart), and GDI and USER objects (`GetGuiResources`, which needs only `PROCESS_QUERY_LIMITED_INFORMATION` but works only for processes in the monitor's own session). Read the values that never change once per identity, with the image path. Measure the cost before reading changing values, such as the GDI objects, on every refresh.
- [ ] Show the file description and company from the image's version resource (`GetFileVersionInfoExW`, which needs no process handle), cached by path. Offer the description as the displayed name, as Task Manager does ("Google Chrome" for `chrome.exe`).
- [ ] Add a "Verified signer" column (Process Explorer): `WinVerifyTrust`, including catalog-signed system files, on a worker thread, cached by path and file time, and with revocation checks that do not go to the network.
- [ ] Show each executable's icon in the Name column. `QFileIconProvider` gets the shell icon from the image path without Windows headers in `ui`; cache the icons by path.
- [ ] Group processes as Task Manager does: apps (processes with a visible top-level window), background processes and Windows processes, with an app's processes folded under one row that shows their totals. Get window ownership from `EnumWindows` and `GetWindowThreadProcessId` on the slow-collector thread. Task Manager's rule for "Windows processes" is not documented, so decide on one.
- [ ] Add a Status column: "Suspended" when every thread waits with the Suspended wait reason (from the thread entries that follow each process in the `SystemProcessInformation` buffer), "Not responding" when a top-level window fails `IsHungAppWindow`, and "Efficiency mode" (check which access `GetProcessInformation(ProcessPowerThrottling)` needs).
- [ ] Shade the CPU, memory and I/O cells by how high the value is, and show the system total in each header ("23% CPU"), as Task Manager does. The shading must keep the text contrast that the [coding guidelines](docs/coding_guidelines.md#text-colours) require.
- [ ] Colour rows by kind, as Process Explorer does: services, the user's own processes, packaged apps and suspended processes, with a legend and within the same contrast.
- [ ] Extend the filter, which matches names and PIDs today, to the description, the company and the command line.
- [ ] Find a window's process, like Process Explorer's crosshair: drag a target onto any window to select its process (`WindowFromPoint`, `GetWindowThreadProcessId`).

### Process details

> *Seen in: Process Explorer, System Informer.*

- [ ] A details pane or dialog for the selected process: image path, command line, parent, start time, user, integrity level and hosted services, with CPU, memory and I/O history charts built from the snapshots the process model already receives.
- [ ] A thread list from the thread entries in the process list: thread ID, state, wait reason, priority, CPU time and usage, and context switches. Start addresses and stacks are out of scope (see [Considered and not planned](#considered-and-not-planned)).

### Process actions

> *Seen in: Task Manager, Process Explorer, Resource Monitor, System Informer.*

- [ ] End process tree: end the selected process and its descendants, found through the tree's `(PID, creation time)` parents ([DC-82](docs/design_decisions.md#dc-82)), each through `terminateOwnedProcess()`. One confirmation says how many processes will be ended and which will be refused.
- [ ] Find which processes use a file. Restart Manager (`RmStartSession`, `RmRegisterResources`, `RmGetList`) returns each one as an `RM_UNIQUE_PROCESS`, a PID and a start time, which matches the process identity. Accept a dropped file or one picked in a file dialog. Process Explorer's and System Informer's handle search answer the same question, but need access the monitor does not request.
- [ ] Open file location (`SHOpenFolderAndSelectItems`), show the file's Properties dialog (`SHObjectProperties`), copy the name, path or command line, and Search online, which opens the browser only when the user picks it.
- [ ] Run new task (Task Manager's File menu), with "Run as administrator" through `ShellExecuteExW` and the `runas` verb.
- [ ] Restart Windows Explorer, as Task Manager offers when `explorer.exe` is selected: end the user's `explorer.exe` within the checks of [DC-87](docs/design_decisions.md#dc-87), then start it again.

### Process control: priority, efficiency mode, affinity and suspend

> *Seen in: Task Manager, Process Explorer, Resource Monitor, Process Lasso.*

Each action needs an access right beyond `PROCESS_QUERY_LIMITED_INFORMATION`, so it extends the exception that termination has in AGENTS.md and architecture.md. It keeps the identity, ownership and elevation checks of [DC-87](docs/design_decisions.md#dc-87), and gets a design decision.

- [ ] Set the priority class (`SetPriorityClass`, which needs `PROCESS_SET_INFORMATION`). Realtime needs a privilege an unelevated user lacks, and Windows quietly sets High instead, so say so.
- [ ] Efficiency mode, applied as Task Manager applies it: `SetProcessInformation(ProcessPowerThrottling)` with execution-speed throttling, plus `IDLE_PRIORITY_CLASS` (`PROCESS_SET_INFORMATION`).
- [ ] CPU affinity: `SetProcessAffinityMask` within one processor group, and CPU sets (`SetProcessDefaultCpuSetMasks`) across groups on systems with more than 64 logical processors ([DC-29](docs/design_decisions.md#dc-29)).
- [ ] Suspend and resume (Resource Monitor, Process Explorer): `NtSuspendProcess` and `NtResumeProcess` from `ntdll.dll`, with `PROCESS_SUSPEND_RESUME`. Never offer it on the monitor's own row, and warn that other processes waiting on a suspended one can hang.
- [ ] Once the actions above exist, remember a priority or affinity per executable and apply it again when the program starts (Process Lasso's persistent rules).

### System tray

- [ ] Add a `QSystemTrayIcon` with a context menu (Open, Exit).
- [ ] Minimize to tray on window close (configurable).
- [ ] Show mini resource summary in the tray tooltip.
- [ ] Add a "Start minimized" setting.
- [ ] Draw the tray icon from live data, such as a small CPU history graph redrawn each tick (Process Explorer). Offer more icons (memory, disk I/O, network, GPU), each its own `QSystemTrayIcon`, chosen in settings.
- [ ] Show the upload and download rates in the tooltip, as TrafficMonitor does, and open the window on a left click.
- [ ] Add a "Start with Windows" setting that writes the current user's `Run` key, which needs no elevation.

### Compact views and window behaviour

> *Seen in: Task Manager, TrafficMonitor, Process Explorer.*

- [ ] A small always-on-top window with chosen readouts and sparklines (Task Manager's summary view, TrafficMonitor's floating window), which can optionally let mouse clicks pass through to the windows below.
- [ ] An "Always on top" option for the main window.
- [ ] Update speeds including Paused, with Refresh now (F5) while paused, as Task Manager offers. Phase 6's refresh-interval setting is the starting point. Decide whether pausing stops the collectors or only the views, and keep the rate baselines correct either way, as the sleep and resume item does.
- [ ] Run a single instance: starting the monitor again brings the running one to the front (`QLocalServer` and `QLocalSocket`, so it stays in Qt).
- [ ] Remember the window's size, position and state, and the last tab and Performance page, or open a default start page chosen in settings (Task Manager).
- [ ] An optional global hotkey that shows the window (`RegisterHotKey`).
- [ ] Draw the title bar in the client area, as Task Manager does: the icon, the name and the caption buttons in one row, with room for a search box or actions. Keep the native frame instead of `Qt::FramelessWindowHint`, which loses the resize borders, the shadow, Aero Snap, the Snap Layouts flyout over the maximize button, the system menu (Alt+Space) and the caption buttons' accessibility. Try `Qt::ExpandedClientAreaHint` first (Qt 6.9); the Qt 6.11.1 that vcpkg builds declares it, but its documentation names no platforms, so check what it does on Windows. Otherwise remove the caption in `WM_NCCALCSIZE` and answer `WM_NCHITTEST` with `HTCAPTION`, the resize edges, and `HTMAXBUTTON` over the maximize button, which Snap Layouts needs. Either way the Windows code lives in `platform/windows`, behind a native event filter as `app::PowerEventFilter` is, since `ui` includes no Windows headers. A library such as QWindowKit is the alternative to weigh. Decide where the File menu's Settings... and Exit go (Task Manager has no menu bar), and whether to add the Mica backdrop (`DWMWA_SYSTEMBACKDROP_TYPE`), which needs the widgets' backgrounds to be transparent. The window title stays set, since the taskbar, Alt+Tab and screen readers read it. Builds on the rename's icon and window title; the caption buttons follow the theme once dark mode exists.

### Dark mode and theme support

- [ ] Detect the Windows system theme (light/dark) at startup.
- [ ] Apply a dark stylesheet/palette when dark mode is active.
- [ ] Add a setting to override: Light / Dark / Follow System.
- [ ] Ensure sparkline charts and all custom widgets respect the active theme.
- [ ] Add support for customizing the theme colors for all widgets, including sparklines and adapter cards.
- [ ] Add support for exporting and importing theme settings, so users can share or backup their custom themes.
- [ ] Investigate font options.

### Accessibility

> *Seen in: Task Manager.*

- [ ] Give the custom widgets (`SparklineWidget`, `ResourceCard`, `ReadoutGrid`, the adapter cards) accessible names and values through `QAccessibleInterface`, so that Narrator reads a chart's current value and range.
- [ ] Make every action reachable from the keyboard, with a logical tab order and shortcuts for the filter (Ctrl+F) and for switching tabs (Ctrl+Tab).
- [ ] Follow Windows contrast themes: use the system colours in the charts and in the dimmed and status text while a contrast theme is on ([coding guidelines](docs/coding_guidelines.md#text-colours)).
- [ ] Test with Narrator and with Windows text size at 200%.

### Alert rules and notification history

- [ ] Define alert rules in settings: metric, threshold, direction, cooldown.
- [ ] Evaluate rules on each snapshot tick.
- [ ] Show toast notifications via `QSystemTrayIcon::showMessage` or Windows notification API.
- [ ] Maintain an in-memory alert history viewable in the UI.
- [ ] Alert on a process that stays above a CPU or memory threshold for a set time (a runaway process), naming it by its identity.
- [ ] Alert on network events: an adapter going down, connectivity falling to local access or none, and a new listening port (with the connections table under [Network](#network-connections-wi-fi-data-usage-and-latency)).
- [ ] Keep an event timeline beside the alert history: process starts and exits (time, name, PID, parent and command line), adapter and connectivity changes, and alerts (System Informer's log, GlassWire's timeline). The process model already finds starts and exits for its highlights ([DC-82](docs/design_decisions.md#dc-82)).

### Persistent event and metric history

> **Name:** Urd (Old Norse *Urðr*). Verdandi, whose name the monitor takes, is the Norn of the present, "what is happening now"; her sisters are Urd, "what has happened", and Skuld, "what shall be". The history shows the past beside the live views, so it takes the Norn of the past's name, which keeps the feature's name within the app's own story. Urd's well, where the Norns live under the world tree, also suits the store that the history is kept in.

- [ ] Add SQLite via vcpkg behind the `IMetricsStore` interface.
- [ ] Batch-insert samples every 10–30 seconds on a background thread.
- [ ] Implement retention policies: downsample older data, expire after configurable duration.
- [ ] Add a history view or extend sparkline charts to show historical data ranges.
- [ ] Record the top processes by CPU, memory and I/O for each interval, so the history view can say which process was busy at a past time. Task Manager's App history keeps similar totals per app.

### Startup applications and services

- [ ] Enumerate startup entries from the registry (`Run`, `RunOnce`) and startup folder.
- [ ] Enumerate Windows services via `EnumServicesStatusEx`.
- [ ] Display in a dedicated tab with name, status, startup type.
- [ ] Enable and disable startup entries as Task Manager does, through the `StartupApproved` keys under `Explorer`. The current user's entries need no elevation; the machine's go through the elevated helper.
- [ ] Show each entry's publisher and signature status (`WinVerifyTrust`, shared with the process table's signer column), and flag entries whose file is missing (Autoruns).
- [ ] List the scheduled tasks that run at logon or startup, through the Task Scheduler COM API (`ITaskService`), as Autoruns does.
- [ ] Show which services each process hosts, in a column or a tooltip on `svchost.exe` rows (Process Explorer, Task Manager). `EnumServicesStatusExW(SC_ENUM_PROCESS_INFO)` gives each running service's PID without opening the process. Link each service to its process row.
- [ ] Start, stop and restart services (Task Manager, System Informer). Most services need administrator rights for this, so it goes through the elevated helper.

### Users and sessions

> *Seen in: Task Manager.*

- [ ] A Users tab: the signed-in sessions (`WTSEnumerateSessionsW`), each with its user's processes and their CPU, memory and I/O totals, grouped by each process's session ID and user. An unelevated monitor cannot read other users' tokens, so their user names show as unknown and their processes group by session.

### GPU monitoring

- [ ] Query GPU utilization and memory via NVML (NVIDIA), ADL (AMD), or `D3DKMTQueryStatistics`.
- [ ] Add GPU performance cards and sparklines.
- [ ] Prefer the vendor-neutral `D3DKMT*` functions, which `gdi32.dll` exports and Task Manager uses, to NVML and ADL, which add vendor DLLs. Their structures are in the WDK's `d3dkmthk.h`, not the SDK, so declare them in `platform/windows`, as `cpu_collector.cpp` declares the structures the SDK omits ([DC-55](docs/design_decisions.md#dc-55)).
- [ ] Chart the utilization of each engine (3D, Copy, Video Decode, Video Encode, Compute) and the dedicated and shared memory of each adapter, as Task Manager's GPU pages do.
- [ ] Add per-process GPU usage, engine, and dedicated and shared memory as process table columns (`D3DKMTQueryStatistics` with a process handle; check that `PROCESS_QUERY_LIMITED_INFORMATION` is enough).
- [ ] Show temperature, fan speed and power from `D3DKMTQueryAdapterInfo(KMTQAITYPE_ADAPTERPERFDATA)` (`D3DKMT_ADAPTER_PERFDATA`, WDDM 2.4 and later), with the throttling and maximum temperatures from `D3DKMT_ADAPTER_PERFDATACAPS`. Integrated GPUs often report none, which shows as unavailable.
- [ ] Monitor NPUs: Task Manager added NPU utilization and NPU memory columns in 2026. Find compute-only adapters with DXCore and show them like GPUs.

### Disk activity and drive health

> *Seen in: Task Manager, Resource Monitor, CrystalDiskInfo, Mission Center.*

- [ ] Show each physical disk's read and write rates, active time and average response time, as Task Manager's disk pages do. `IOCTL_DISK_PERFORMANCE` on each `\\.\PhysicalDriveN` returns the byte, count, time and idle counters they come from. That is a direct alternative to the PDH counters that [DC-02](docs/design_decisions.md#dc-02) and architecture.md's API table set aside for per-disk I/O. Check that it works unelevated on a handle opened without read access, and measure it before making it a fast collector.
- [ ] Map volumes to disks (`IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS`), so each disk page lists its volumes and their space. Show the disk's model, bus type, SSD or HDD (`IOCTL_STORAGE_QUERY_PROPERTY` with `StorageDeviceProperty` and `StorageDeviceSeekPenaltyProperty`), and whether it holds the system volume or a page file.
- [ ] Add a disk-activity card with a sparkline to the dashboard, beside disk space.
- [ ] Show NVMe drive health: temperature, percentage used, available spare, media errors and power-on hours, from the health log page (`IOCTL_STORAGE_QUERY_PROPERTY` with `StorageDeviceProtocolSpecificProperty`). Check whether it works unelevated; if it does not, read it through the elevated helper on request. SATA SMART data needs ATA pass-through, which needs administrator rights.

### CPU and memory detail

> *Seen in: Task Manager, Process Explorer, HWiNFO.*

- [ ] Show the CPU topology from `GetLogicalProcessorInformationEx`: sockets, cores, logical processors, L1, L2 and L3 cache sizes, and NUMA nodes, with a per-node view of the per-core grid. Mark performance and efficiency cores on hybrid processors from each core's `EfficiencyClass`.
- [ ] Show kernel time on the CPU charts (Task Manager's "Show kernel times"), from the kernel and idle times the CPU collector already reads.
- [ ] Show system-wide rates, as Process Explorer's System Information does: context switches, page faults and I/O operations per second from `NtQuerySystemInformation(SystemPerformanceInformation)`, and interrupt and DPC time from the per-processor performance information. `winternl.h` hides most of both structures, so check the layouts as [DC-71](docs/design_decisions.md#dc-71) does.
- [ ] Show the memory composition (in use, modified, standby, free), as Task Manager's bar does. `NtQuerySystemInformation(SystemMemoryListInformation)` gives the lists; check whether it needs a privilege an unelevated monitor lacks.
- [ ] Show the memory hardware (speed, slots used, form factor) from the SMBIOS memory-device records (`GetSystemFirmwareTable('RSMB')`), which needs no WMI.
- [ ] Show each page file's size, use and peak (`NtQuerySystemInformation(SystemPageFileInformation)`).

### Battery and power

> *Seen in: Mission Center, btop, HWiNFO, iStat Menus.*

- [ ] Add a battery card and Performance page, shown only when the system has a battery: charge, on AC or battery, time remaining and battery saver (`GetSystemPowerStatus`).
- [ ] Show the charge or discharge rate, voltage, full-charge and design capacity (the wear) and cycle count, from `IOCTL_BATTERY_QUERY_INFORMATION` and `IOCTL_BATTERY_QUERY_STATUS` on the battery devices SetupAPI lists (`GUID_DEVCLASS_BATTERY`), not from WMI.
- [ ] Show the power source and the effective power mode (`PowerRegisterForEffectivePowerModeNotifications`), received as notifications under the rules for [event-driven collectors](docs/architecture.md#event-driven-collectors-os-callback-threads).

### Network: connections, Wi-Fi, data usage and latency

> *Seen in: TCPView, Resource Monitor, System Informer, Task Manager, TrafficMonitor, GlassWire.*

- [ ] A connections table: TCP connections and listening ports and UDP endpoints, for IPv4 and IPv6, each with its owning process (`GetExtendedTcpTable` with `TCP_TABLE_OWNER_PID_ALL`, `GetExtendedUdpTable` with `UDP_TABLE_OWNER_PID`), collected on the slow-collector thread. The tables give only a PID, so match it against the process list read closest in time, and show the process as unknown once it has gone.
- [ ] Resolve remote addresses to host names on request, or behind a setting, because each lookup sends a query to the DNS server. Cache the names.
- [ ] Close a TCP connection (TCPView, System Informer) through the elevated helper: `SetTcpEntry` needs administrator rights and handles IPv4 only.
- [ ] Show Wi-Fi details for wireless adapters, as Task Manager does: SSID, signal quality, band and channel, PHY type and link rates (`WlanQueryInterface`), updated through `WlanRegisterNotification` under the event-driven collector rules. Since Windows 11 24H2, desktop apps get these only while location access is on for them, so show "Location access is off" rather than blanks.
- [ ] Count the data used per adapter since the monitor started, from the `GetIfTable2` byte deltas, and per day and month once [persistent history](#persistent-event-and-metric-history) exists, so that totals survive restarts and counter resets (TrafficMonitor, GlassWire).
- [ ] Add a latency chart: round-trip time and loss to the default gateway and to an optional host, from `IcmpSendEcho2Ex` and `Icmp6SendEcho2` on a worker thread. `GetAdaptersAddresses` gives the gateway with `GAA_FLAG_INCLUDE_GATEWAYS`. The host is off by default because it sends traffic out of the network. The result never sets the connectivity level, which stays with `GetNetworkConnectivityHint` ([DC-03](docs/design_decisions.md#dc-03)).

### Hardware sensors

> *Seen in: HWiNFO, HWMonitor, LibreHardwareMonitor, TrafficMonitor (through LibreHardwareMonitor).*

- [ ] Decide whether to show CPU and motherboard temperatures, fan speeds and voltages, and record the decision. Reading them needs a kernel driver. Microsoft Defender has flagged WinRing0, the driver these tools shipped, since 2025, and they are moving to PawnIO. Any such driver conflicts with running unelevated and with the antivirus and EDR rules in architecture.md. Without a driver, the monitor can still show GPU temperature ([GPU monitoring](#gpu-monitoring)), drive temperature ([Disk activity and drive health](#disk-activity-and-drive-health)) and battery data ([Battery and power](#battery-and-power)). An alternative to investigate is reading the values another monitor already publishes, such as HWiNFO's shared memory; LibreHardwareMonitor publishes through WMI, which the monitor does not poll.
- [ ] Investigate possibility of sending sensors data to Stream Deck.
  - [ ] We would also need a Stream Deck plugin to receive and display the sensor data.

### ETW-powered diagnostics

- [ ] Implement an ETW real-time trace session consumer.
- [ ] Subscribe to `Microsoft-Windows-Kernel-Network` for per-process network bandwidth.
- [ ] Add short "diagnostic recording" sessions with start/stop and export.
- [ ] Handle ETW session limits (`ERROR_NO_SYSTEM_RESOURCES`) and permission requirements gracefully.
- [ ] Break each process's network traffic down by remote address and port, as Resource Monitor's Network Activity list does, and fill the Processes tab's Network column ([DC-79](docs/design_decisions.md#dc-79)).
- [ ] Show which files each process reads and writes, with the response times, from the kernel's disk I/O events (Resource Monitor's Disk tab).
- [ ] Show hard faults per second for each process (Resource Monitor's Memory tab).

### CSV/JSON export

- [ ] Export current snapshot or ring buffer history as CSV or JSON.
- [ ] Add export actions to the UI (file dialog).
- [ ] Log chosen metrics to a CSV file while the monitor runs, with an interval and a size limit, written on a background thread (HWiNFO's sensor logging).
- [ ] Copy a Performance page's readouts as text with Ctrl+C, as Task Manager does.
- [ ] Optionally serve the current snapshot in the OpenMetrics text format on `127.0.0.1`, for Prometheus and Grafana (Glances, `windows_exporter`). Off by default; `QTcpServer`, from the Qt Network module the build already includes, is enough.

### Packaging and distribution

> *Seen in: Process Explorer and System Informer (portable, no installation), winget.*

- [ ] Publish a portable release: a zip with the executable and the Qt files it needs ([DC-104](docs/design_decisions.md#dc-104)) that runs without installation, and a portable mode that keeps the settings file next to the executable.
- [ ] Sign the release binaries in CI, as architecture.md's antivirus section asks, and publish a winget manifest.
- [ ] Add an Arm64 build (Process Explorer ships one). It needs the vcpkg `arm64-windows` triplet and a check of the structures `platform/windows` declares itself.

### Architecture diagrams (PlantUML)

- [ ] Add `docs/diagrams/` directory with a shared PlantUML skin/style file for visual consistency.
- [ ] Class diagrams covering the key module boundaries: `domain` types, `monitoring` interfaces and scheduler, `platform/windows` adapters, `ui` view models.
- [ ] Sequence diagram for the main data-flow path: scheduler tick → fast collectors → snapshot assembly → Qt queued signal → view model update.
- [ ] Sequence diagram for slow-collector integration: process enumeration thread → mutex-protected merge → snapshot assembly.
- [ ] Sequence diagram for application startup and shutdown (thread lifecycle, `std::stop_token` cancellation).
- [ ] Generate SVG output from `.puml` sources and embed the rendered diagrams in `docs/architecture.md`.
- [ ] Add a CI step that runs PlantUML on every `.puml` file and fails the build if the generated SVGs are out of date.

### Code organization

- [ ] Create a header-only `src/common/` module (`verdandi_common`) for dependency-free utilities shared across modules, and move `RingBuffer<T>` there from `domain`. Trigger: do this when a second general-purpose utility (e.g. a thread-safe latest-value holder for collectors, or shared RAII helpers) needs to be shared across modules, so `domain` stays limited to data types. Update the module rules and architecture module table accordingly.

### DevEnv
- [x] Add a PowerShell bootstrap script that detects or installs build prerequisites, sets up vcpkg and its binary cache, and verifies the toolchain.

### General
> Whatever does not fit into the above categories

- [ ] Add a Release badge when we have released a first release. Also update the README with a download section.
- [x] Draw the application icon.

  Done: `resources/verdandi.ico` holds the V-trace design, chosen on 2026-10-21 from four concepts: a chart line that runs flat, dips into a V and runs flat again, amber on a dark indigo tile. `scripts/generate_icon.ps1` draws it with GDI+ at 16, 20, 24, 32, 40, 48, 64, 96 and 256 px, with the 16, 20 and 24 px images placed on the pixel grid. It also writes the 256 px design as `resources/verdandi.svg` and `resources/verdandi_256.png`, and `resources/social_preview.png`, a 1280 x 640 px banner for GitHub's social preview, which is uploaded by hand in the repository's settings ([DC-130](docs/design_decisions.md#dc-130)). Windows' `LoadImageW` loads every size, and `DrawIconEx` blends their edges over light and dark taskbar colours as expected. On the dark taskbar the tile shows little contrast, but the amber V stands out. Nothing builds the icon into the executable yet; the item below does.
- [x] Rename the application to Verdandi, and give the executable and its windows the icon and the new title.

  Verdandi (Old Norse *Verðandi*) is the Norn of the present, "what is happening now". It replaces "System Monitor", which is generic, and whose short form `sysmon` is also the name of Sysinternals' Sysmon. On 2026-10-21 searches found no software trademark or monitor named Verdandi. Each step below is a commit of its own, in this order. A step renames its names everywhere they appear: in the code, the tests, the scripts and every doc, the module AGENTS.md files included, so that no commit leaves a doc naming what no longer exists. The code reviews under `docs/code_reviews/` and the Done notes in this roadmap are records of their time and keep the old names.

  - [x] Show the new name: the window title, the save-failure message box, "Close System Monitor" and "... is System Monitor itself" in the Processes tab, with their tests and doc comments, and the coverage report's `-project-title` in `build.ps1`.

    Done: the texts say "Verdandi". The window title and the save-failure message have no tests. The executable is still `system_monitor.exe` until the step after next, so the close question's examples keep that name.

  - [x] Keep the settings and the log under the new name: `QCoreApplication`'s application and organization names, which choose the folders that hold `settings.json` and the log, now `%APPDATA%\System Monitor\System Monitor` and its `%LOCALAPPDATA%` twin; the log file `system_monitor.log` and the logger's name; and the folder names in the settings integration tests. Choose the organization name: an empty one gives `%APPDATA%\Verdandi`. No release has shipped, so a settings file in the old folder need not be moved.

    Done: the application is named "Verdandi" and has no organization name, so `settings.json` is in `%APPDATA%\Verdandi` and the log, `verdandi.log`, from the logger `verdandi`, in `%LOCALAPPDATA%\Verdandi` ([DC-131](docs/design_decisions.md#dc-131)). The temporary folders, file and child process command line that the tests create are named after Verdandi too.

  - [x] Rename the executable from `system_monitor.exe` to `verdandi.exe` and its manifest to `verdandi.manifest`, with the manifest's paths in the test and benchmark CMake files, the application's path in `build.ps1`, and the tests and comments that name the executable.

    Done: the CMake target that builds the application is `verdandi`, and `build.ps1 -Run` and the coverage report use `verdandi.exe`. The docs name `verdandi.manifest`; those that name the test and benchmark executables keep their names until the build is renamed.

  - [x] Compile `resources/verdandi.ico` into the executable from a `.rc` file, so that Explorer and the taskbar show it. Qt's Windows platform plugin gives its windows the executable's icon resource named `IDI_ICON1`, if there is one; check that it reaches the title bar, or call `QApplication::setWindowIcon()`.

    Done: `src/app/verdandi.rc` compiles the icon, with its nine sizes, into `verdandi.exe` as `IDI_ICON1` ([DC-132](docs/design_decisions.md#dc-132)). It reaches the title bar without `setWindowIcon()`: the running window's class has the 32 and 16 px icons from the resource, no icon is set on the window itself, and Explorer shows the same 32 px icon.

  - [x] Rename the `sysmon::` namespace to `verdandi::`. The diff only renames, apart from the lines that `build.ps1 -Format` rewraps because the name is longer.

    Done: 688 lines in 234 source files, and the namespace in AGENTS.md, GEMINI.md, coding_guidelines.md and design_decisions.md. `-Format` rewrapped three lines. With the rename reversed, every changed file matches the previous commit, whitespace aside.

  - [x] Rename the build: the `sysmon_*` CMake targets and functions, the `SYSMON_COVERAGE` option and its preset, the CMake project and its messages, the test and benchmark executables (`system_monitor_tests`, `system_monitor_platform_tests`, `system_monitor_benchmark`, `system_monitor_process_benchmark`), `vcpkg.json`'s `name`, and the paths and help in `build.ps1` and `bootstrap.ps1`. Like the namespace's, this diff only renames.

    Done: the targets and functions are `verdandi_*` and `add_verdandi_*`, the option is `VERDANDI_COVERAGE`, the project and `vcpkg.json`'s package are `Verdandi` and `verdandi`, and the executables are `verdandi_tests`, `verdandi_platform_tests`, `verdandi_benchmark` and `verdandi_process_benchmark`. The integration test that names its own process now names `verdandi_tests.exe`.

  - [x] Name the application Verdandi in the docs' prose (README.md, AGENTS.md, GEMINI.md, `.github/copilot-instructions.md`, CONTRIBUTING.md, architecture.md, build_system.md, coding_guidelines.md and this roadmap's title), and explain the name in README.md: Verdandi as the Norn of the present, and why it replaced System Monitor.

    Done: the docs' titles and opening lines name Verdandi, and README.md's "The name" section explains it. "System Monitor" is left only where a text names the old name: DC-131's old folder, this item and the Done notes.

  - [x] Rename the GitHub repository `DunderGG/system-monitor` to `DunderGG/verdandi`, by hand in its settings. Then update the clone steps and layout in CONTRIBUTING.md, the coverage dashboard's address in README.md, AGENTS.md, GEMINI.md, `.github/copilot-instructions.md` and build_system.md, and the local clone's remote. GitHub redirects a renamed repository, but not its Pages site, so the dashboard moves from <https://dundergg.github.io/system-monitor/> to <https://dundergg.github.io/verdandi/>. The `gh-pages` branch, and with it the dashboard's trend, moves with the repository.

    Done: the repository is `DunderGG/verdandi`, with its `main` and `gh-pages` branches. CONTRIBUTING.md clones it into `verdandi`, the docs link to the dashboard at <https://dundergg.github.io/verdandi/>, which the next coverage run on `main` publishes, and the local clone's `origin` is the new address. CI needs no change, because `coverage_pages.ps1` takes the repository from `GITHUB_REPOSITORY`.

### Considered and not planned

> These features of other monitors conflict with the rules in AGENTS.md and architecture.md. Reconsider one only with a design decision that changes the rule.

- DLL and module lists, memory regions, thread stacks, environment variables, strings and memory dumps (Process Explorer, System Informer, Task Manager's "Create memory dump file"): each reads another process's memory, which needs `PROCESS_VM_READ`.
- Handle lists and handle search (Process Explorer, System Informer): naming another process's handles needs `PROCESS_DUP_HANDLE`. Restart Manager answers the usual question, which process holds a file (see [Process actions](#process-actions)).
- Replacing Task Manager (Process Explorer, System Informer): this sets a `Debugger` value for `taskmgr.exe` under Image File Execution Options. That needs administrator rights, and it is a persistence technique that EDR products flag.
- A readout embedded in the taskbar (TrafficMonitor): Windows has no API for it, so it relies on the taskbar's undocumented windows. The tray icon and the small always-on-top window meet the same need.
- VirusTotal lookups (Process Explorer, System Informer): these send a hash of every executable to a third-party service. If ever added, it should only be an explicit action on one file.

### Issues found while testing

- [ ] Uptime seems incorrect, not sure what it is actually showing.
