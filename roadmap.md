# System Monitor — Roadmap

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
- [ ] Retire entries that restate a rule kept elsewhere or record where code lives, leaving stubs: DC-26 (an AGENTS.md rule), DC-68 (where shared UI constants live), and DC-102 and DC-105 once their rationale is in the coding guidelines' "Text from outside the program" and "Text colours" sections, which already state the rules.
- [ ] Set a format and a budget, at the top of the file and in the design-decision rule of AGENTS.md and its copy in GEMINI.md: **Decision** (what was chosen), **Rationale** (why, and the alternatives rejected), optional **Consequences**, then the **History:** line; about 250 words; no test lists, benchmark tables, UI copy, or measurements beyond the one that justifies the choice. [DC-117](docs/design_decisions.md#dc-117) already says where that content goes; extend it with the format and the budget.

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

- [ ] Add a coverage build that measures line and branch coverage. OpenCppCoverage, the usual free tool for MSVC builds, reports lines only, so compile with clang-cl and LLVM's source-based coverage (`-fprofile-instr-generate -fcoverage-mapping`). Visual Studio's "C++ Clang tools for Windows" component already has clang-cl, `lld-link`, `llvm-profdata`, `llvm-cov` and the profile runtime (22.1.3 on 2026-10-17). Add a `coverage` configure preset that instruments only the project's targets; the vcpkg packages stay as MSVC built them, and clang-cl code links against them. Where clang-cl rejects the code or warns, fix the code rather than silencing it. Make `bootstrap.ps1` install the component, and check that the `windows-latest` runner has it. First check whether Microsoft's `dotnet-coverage` reports branches for native code; if it does, the build can keep MSVC as its only compiler.
- [ ] Add `build.ps1 -Coverage`. It builds the `coverage` preset and runs the tests with `LLVM_PROFILE_FILE` set to one raw profile per process (`%p`), because the shards run in parallel ([DC-113](docs/design_decisions.md#dc-113)). It then merges the profiles with `llvm-profdata merge` and reports on `src/` alone, leaving out test, moc and vcpkg sources. Print line and branch coverage per module (`app`, `domain`, `monitoring`, `platform/windows`, `ui`, `ui/charts`) and in total, and write `llvm-cov show`'s HTML, which marks every line and branch of each file. The integration tests call real Windows APIs, so `platform/windows` coverage depends on the host and can differ between this machine and CI. Document the option in the script's help, [build_system.md](docs/build_system.md) and the build commands in AGENTS.md and its copies.
- [ ] Write each shard's results: `sysmon_add_sharded_tests()` sets `GTEST_OUTPUT=json:<dir>/<target>_shard_<index>.json` with the shard index in the name, so that shards running in parallel never write the same file. GoogleTest's JSON gives each test's suite, name, result, time, and source file and line, which is enough to count unit, platform and integration tests by directory.
- [ ] Generate a dashboard in the same run: one self-contained HTML page, `build/coverage/index.html`, with inline SVG charts and no external scripts, so it opens from disk. A PowerShell 5.1 script writes it from the GoogleTest JSON, `llvm-cov export -summary-only` and a history file of one JSON line per run (date, commit, counts, time, coverage). The page shows:
  - tests by kind (unit, platform, integration) and by suite, with how many passed, failed, were skipped or are disabled;
  - the total time and the slowest tests (the times come from the instrumented clang-cl build, so compare them only with each other);
  - line and branch coverage per module and in total, and the least-covered files, each linking to its annotated source;
  - the trend of the test count, time and coverage, one point per run.
- [ ] Decide where the history lives, so the trend survives deleting `build/` and can be seen without building. Recommended: a CI job runs `build.ps1 -Coverage` on each push to `main`, appends the run to `history.jsonl` on a `gh-pages` branch, and publishes the dashboard with GitHub Pages, which is free for this public repository. Only that job gets `contents: write`; the others keep `read`. On pull requests the job uploads the dashboard as a workflow artifact and writes the change in coverage against `main` to the job summary. Local runs keep their own history in `build/coverage/`.
- [ ] Decide whether coverage can fail the build. Recommended: report only, until the history shows how much coverage varies between runs. Then fail a pull request that lowers line or branch coverage of `src/` by more than one percentage point against `main`. A fixed minimum would either fail at once or stay silent until coverage had fallen far below today's.

---

## Phase 6 — Settings and polish

- [ ] Implement the JSON settings file: refresh interval, display units, enabled dashboard panels, log level.
- [ ] Create a settings dialog (`QDialog`) for editing preferences.
- [ ] Load settings at startup with defaults and validation. Save on change.
- [ ] Add an application icon and window title.
- [ ] Enable per-monitor DPI awareness in the application manifest (`src/app/system_monitor.manifest`, added for the UTF-8 code page in [code review F-19](docs/code_reviews/phase_2.md#f-19-log-file-path-is-converted-to-the-ansi-code-page)).
- [ ] Implement sleep/resume detection (`WM_POWERBROADCAST`) — discard first sample and reset rate baselines on resume.
- [ ] Profile CPU and memory usage of the monitor itself on a lower-end system, including the UI tick cost from `.\scripts\build.ps1 -Benchmark`. Optimize if needed.
- [ ] Set `requestedExecutionLevel` to `asInvoker` explicitly in `src/app/system_monitor.manifest`. (The MSVC linker already adds `asInvoker` by default; stating it in the manifest documents the requirement.)
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
- [ ] Shade the CPU, memory and I/O cells by how high the value is, and show the system total in each header ("23% CPU"), as Task Manager does. The shading must keep the text contrast of [DC-105](docs/design_decisions.md#dc-105).
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
- [ ] Follow Windows contrast themes: use the system colours in the charts and in the dimmed and status text while a contrast theme is on ([DC-105](docs/design_decisions.md#dc-105)).
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

- [ ] Create a header-only `src/common/` module (`sysmon_common`) for dependency-free utilities shared across modules, and move `RingBuffer<T>` there from `domain`. Trigger: do this when a second general-purpose utility (e.g. a thread-safe latest-value holder for collectors, or shared RAII helpers) needs to be shared across modules, so `domain` stays limited to data types. Update the module rules and architecture module table accordingly.

### DevEnv
- [x] Add a PowerShell bootstrap script that detects or installs build prerequisites, sets up vcpkg and its binary cache, and verifies the toolchain.

### Considered and not planned

> These features of other monitors conflict with the rules in AGENTS.md and architecture.md. Reconsider one only with a design decision that changes the rule.

- DLL and module lists, memory regions, thread stacks, environment variables, strings and memory dumps (Process Explorer, System Informer, Task Manager's "Create memory dump file"): each reads another process's memory, which needs `PROCESS_VM_READ`.
- Handle lists and handle search (Process Explorer, System Informer): naming another process's handles needs `PROCESS_DUP_HANDLE`. Restart Manager answers the usual question, which process holds a file (see [Process actions](#process-actions)).
- Replacing Task Manager (Process Explorer, System Informer): this sets a `Debugger` value for `taskmgr.exe` under Image File Execution Options. That needs administrator rights, and it is a persistence technique that EDR products flag.
- A readout embedded in the taskbar (TrafficMonitor): Windows has no API for it, so it relies on the taskbar's undocumented windows. The tray icon and the small always-on-top window meet the same need.
- VirusTotal lookups (Process Explorer, System Informer): these send a hash of every executable to a third-party service. If ever added, it should only be an explicit action on one file.

### Issues found while testing

- [ ] Uptime seems incorrect, not sure what it is actually showing.
