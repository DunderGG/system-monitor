# Code Review — Phase 6 Completion and Release Readiness

**Date:** 2026-10-22
**Scope:** The whole codebase at `f77fa04`, because Phase 6 is the roadmap's last phase before a release: every source file under `src/` (142 files, about 16,200 lines), the tests (92 files, about 16,600 lines), the benchmarks, the scripts, the CMake files, CI and the documentation. The work since the Phase 5 review (55 commits, `4ee1043`..`f77fa04`, 288 files) was read line by line: the maintenance items on tests, design decisions and coverage; Phase 6's settings file, settings dialog, settings at startup, display units, dashboard panels, DPI awareness and sleep and resume; and the rename to Verdandi. Earlier code was read again in full against the current rules, and the built executables were inspected as a release would ship them.
**Reviewed against:** [architecture.md](../architecture.md), [coding_guidelines.md](../coding_guidelines.md), [design_decisions.md](../design_decisions.md), [known_deviations.md](../known_deviations.md), [roadmap.md](../../roadmap.md), [AGENTS.md](../../AGENTS.md) and the module `AGENTS.md` files
**Previous reviews:** [Phase 1](phase_1.md) (F-1 to F-8), [Phase 2](phase_2.md) (F-9 to F-19), [Phase 3](phase_3.md) (F-20 to F-28), [Phase 4](phase_4.md) (F-29 to F-34), [Phase 5](phase_5.md) (F-35 to F-44). Finding IDs continue from there.

---

## Overall Assessment

The codebase is in good shape for a first release, apart from one defect in how the executable is built. The design has held through every phase, and the code follows its rules with very few exceptions:

- **Architecture:** Every module boundary holds. `domain` and `persistence` include neither Qt nor Windows headers, `platform/windows` includes no Qt and no `monitoring` header but `collector.h`, and `ui` includes no Windows, `monitoring`, `platform`, `persistence` or `app` header. The eight `platform/windows` headers that expose SDK types are included only by platform sources, `tests/platform/` and `tests/integration/`. Phase 6 fits the design: the settings types are in `domain`, the file is read and written in `persistence` with the standard library and nlohmann/json alone, `ui` edits and applies what the views show, and `app` applies the rest and saves.
- **Threading:** Data crosses threads only as `SystemSnapshot` values. The tick thread, the slow-collector thread, the network-name worker and the OS callbacks take their locks in the documented order, and the sleep-and-resume pause is checked before every collection on both scheduler threads.
- **Rules:** No C++ file throws or catches an exception, uses `std::bind`, `volatile`, `qDebug()` or console output, or calls Tool Help, PDH, WMI or an ANSI API other than `inet_ntop` (F-53). Every `new` creates a Qt-parented object. All tracked C++ files match clang-format 22.1.3, a full Release rebuild has no warnings, and all 783 tests pass, two of them skipped because they need an elevated run.
- **Clean code:** functions are short and do one thing, names say what they hold, and comments explain why rather than what. Pure functions carry the logic that the widgets and collectors only wire up, which is why most of it is tested without widgets or Windows. The exceptions are small: two near-identical query loops, a table of names written twice, and a few abbreviations (F-53).
- **Robustness:** nlohmann/json is used without exceptions throughout; every Windows call's result is checked and repeated failures are logged once; missing data stays `std::nullopt` or Unknown from the collector to the UI; text from outside the program is plain text everywhere.
- **Cost:** with real data on the review machine, the monitor used 0.4–1.7% of one logical processor at a 1-second interval and 0.6–2.7% at 0.5 seconds, with a working set under 50 MiB. Two of three benchmark runs were faster than the recorded baseline on every row, most by 20–30%, so Phase 6 slowed no tab.

This review found one **high-severity** issue, which every user of a release would see:

- `verdandi.exe` is linked as a console program. Started from Explorer, it opens a terminal window beside its own, and closing that window ends the monitor without its shutdown (F-45).

It found one **medium-severity** issue, in this phase's main feature:

- A shorter refresh interval takes effect only when the current one ends. Changed from 10 s to 0.5 s, the next snapshot came 10 seconds later (F-46).

The low findings are a crash at startup when the log file cannot be opened (F-47); the uptime's wording, which explains the roadmap's open "Uptime seems incorrect" (F-48); a test-runner check that has never worked (F-49); what a release build lacks: a version, its runtime, symbols and two mitigations (F-50); CI, which never builds Release and lets warnings through (F-51); and out-of-date documentation (F-52). F-53 collects consistency and presentation items.

The tests are thorough for what they model, as in earlier phases, but the high and medium findings lie outside what they reach: F-45 is a property of the built executable, and F-46 needs a running scheduler whose interval changes, which no test does.

The architecture needs no change for the release. Two parts of it will have to grow with the roadmap's later features, which is not a finding but worth planning for. `SamplingScheduler` has a setter, a member and a snapshot field for each kind of collector, so every new metric (GPU, disk activity, battery) touches the scheduler, `SystemSnapshot`, `registerSystemCollectors()` and their tests. And each view keeps its own history (the dashboard's and the Network tab's minute, the Performance tab's 1,800 samples), which the persistent history item will want in `monitoring` or behind a store, as architecture.md already allows.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | DC-21's test-runner check never worked (F-49) |
| **Phase 2** — Real Windows collectors & dashboard | ✅ Complete | |
| **Phase 3** — Sparkline charts & performance views | ✅ Complete | |
| **Phase 4** — Process explorer | ✅ Complete | |
| **Phase 5** — Network view | ✅ Complete | All Phase 5 findings are resolved |
| **Maintenance** — design decisions, tests, coverage | 🔶 Nearly complete | Open: the coverage minimum, raising the coverage of `platform/windows`, and CI's vcpkg cache |
| **Phase 6** — Settings and polish | 🔶 In progress | Five of eight items done. Open: profiling on a lower-end system (this review measured a reference, below), `asInvoker` in the manifest (F-50 adds `supportedOS`), and this review. F-45 and F-46 should be fixed before the release |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐⭐ | Every boundary holds, checked by include across `src/` and `tests/`. Phase 6's settings split cleanly across `domain`, `persistence`, `ui` and `app` |
| Naming conventions | ⭐⭐⭐⭐⭐ | All 783 tests follow `TypeName_Scenario_ExpectedResult`. A few abbreviations (F-53) |
| Formatting | ⭐⭐⭐⭐⭐ | All tracked C++ files match `.clang-format`; the design decision checks pass |
| C++20 usage | ⭐⭐⭐⭐⭐ | `std::jthread` and `std::stop_token`, designated initializers, `std::format`, ranges with projections, defaulted comparisons, `constexpr` tables |
| Error handling | ⭐⭐⭐⭐ | nlohmann/json without exceptions, every OS call checked, missing data explicit. spdlog's exception can end the program at startup (F-47) |
| Memory management | ⭐⭐⭐⭐⭐ | RAII for every handle, COM pointer, registration and buffer; Qt parenting for every widget; every history bounded |
| Threading | ⭐⭐⭐⭐½ | Lock order, listener protocol and suspend pause are correct. An interval change's notification is lost (F-46) |
| Security and input handling | ⭐⭐⭐⭐½ | Plain text for outside text, robust settings parsing, termination checks intact. CI's publishing step shares a job with third-party actions pinned by tag (F-51) |
| Testing | ⭐⭐⭐⭐ | 783 tests in 2 s of wall time, with good seams. No test changes a running scheduler's interval or checks the executable's subsystem, and the runner's list check is dead (F-46, F-45, F-49) |
| UI and presentation | ⭐⭐⭐½ | A terminal window opens beside the app (F-45). Uptime wording (F-48) and formatting edges (F-53) |
| Build system and release readiness | ⭐⭐⭐ | Console subsystem (F-45); no version, runtime, symbols or Control Flow Guard (F-50); no Release build in CI (F-51) |
| Documentation | ⭐⭐⭐⭐ | Thorough and mostly current; a few stale statements in the architecture, DC-54, the README and comments (F-52) |

### Verification performed

- `.\scripts\build.ps1 -Test` (Debug): 783 tests in 33 shards; 781 passed and 2 were skipped because they need an elevated run (`ProcessTerminatorIntegration.RealHost_ElevatedOwnChild_*` and `*_UnelevatedChildAtHighIntegrity_*`). The review ran unelevated.
- `.\scripts\build.ps1 -Release -Test` after giving all 237 C++ files a new timestamp, which forces a full recompile: 163 build steps at `/W4`, no warnings, and all tests passed.
- `.\scripts\build.ps1 -CheckFormat`: every tracked C++ file matches clang-format 22.1.3; the 133 design decision entries have their fields in order, and their links and citations in 280 files resolve.
- `.\scripts\build.ps1 -Coverage`: a full clang-cl rebuild (162 build steps) with no warnings, and all tests passed. They covered 93.5% of `src\`'s 7,001 lines and 87.4% of its 2,370 branches, against 92.5% and 87.0% on 2026-10-19. `platform/windows` had 84.1% of its lines and 76.7% of its branches, and `app` 71.4% of its lines, since no test runs `main.cpp`.
- `.\scripts\build.ps1 -Benchmark` three times in a row (see [Items Verified](#items-verified--no-issues-found)).
- **Inspected `verdandi.exe`** with `dumpbin /headers` and `/dependents` and with `mt.exe` (its manifest), in Debug and Release, and **started the Release build as Explorer starts it** (`Start-Process`), listing its child processes and the top-level windows that appeared (F-45).
- **Probes** in a throwaway test file, removed afterwards: changing a running scheduler's intervals (F-46); `configureLogging()` with a read-only log file (F-47); the CPU collector's per-core values at 0.5 s and 1 s; copying a snapshot that holds the real process list; a settings file that starts with a UTF-8 byte order mark; byte and rate formatting at unit boundaries (F-53); and the monitor's own processor time and memory on each tab, with the real collectors and the window on screen, at 1 s and 0.5 s.
- **Uptime:** compared `GetTickCount64`, `QueryUnbiasedInterruptTime` and `Win32_OperatingSystem.LastBootUpTime`, read `HiberbootEnabled`, and listed the Kernel-General, Kernel-Boot and Power-Troubleshooter events of the last boots and wakes (F-48).
- **Ran `verdandi_tests.exe --gtest_list_tests`** with `QT_QPA_PLATFORM=nonexistent` (F-49).
- **Read the libraries' sources** in vcpkg's build trees where the code depends on their behaviour: Qt 6.11.1's `QMessageBox::setTextFormat()` and `setInformativeText()`, its executable macro and entry-point link rule, and its absence of a console control handler; GoogleTest 1.18's flag parsing; spdlog 1.17's file sinks and rotation; and `lukka/run-vcpkg@v11`'s `action.yml`.
- **Searched for banned constructs** in all 237 C++ files: exceptions, `new` and `delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, `qDebug`, `std::cout` and `printf`, `using namespace std` and `using namespace` in headers, Tool Help, PDH, WMI and ANSI API variants. Checked every module's includes against its rules.

---

## Findings

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [F-45](#f-45-verdandiexe-is-a-console-program-so-every-launch-opens-a-terminal-window) | `verdandi.exe` is a console program, so every launch opens a terminal window | High | Resolved |
| [F-46](#f-46-a-new-refresh-interval-takes-effect-only-when-the-current-interval-ends) | A new refresh interval takes effect only when the current interval ends | Medium | Resolved |
| [F-47](#f-47-a-log-file-that-cannot-be-opened-ends-the-monitor-at-startup) | A log file that cannot be opened ends the monitor at startup | Low | Resolved |
| [F-48](#f-48-uptime-counts-from-the-last-full-boot-which-fast-startup-does-not-reset) | Uptime counts from the last full boot, which Fast Startup does not reset | Low | Resolved |
| [F-49](#f-49-listing-the-tests-still-creates-a-qapplication) | Listing the tests still creates a `QApplication` (Phase 1 code) | Low | Resolved |
| [F-50](#f-50-the-release-build-lacks-a-version-its-runtime-symbols-and-two-mitigations) | The release build lacks a version, its runtime, symbols and two mitigations | Low | Resolved |
| [F-51](#f-51-ci-never-builds-release-and-lets-warnings-through) | CI never builds Release and lets warnings through | Low | Open |
| [F-52](#f-52-documentation-out-of-date) | Documentation out of date | Low | Open |
| [F-53](#f-53-consistency-and-presentation-items) | Consistency and presentation items | Cosmetic | Open |

**Open findings from earlier reviews:** none.

---

### F-45: `verdandi.exe` is a console program, so every launch opens a terminal window

**Severity:** High (every user of a release sees it; the fix is one keyword)
**Files:** `src/app/CMakeLists.txt` — `qt_add_executable(verdandi ...)`; `src/app/app_logging.cpp` — `configureLogging()`

**Description:**

`qt_add_executable()` passes its arguments on to `add_executable()` and sets no subsystem of its own (`_qt_internal_create_executable()` in Qt 6.11.1's `Qt6CoreMacros.cmake`), so an executable is a GUI program only if the caller passes `WIN32`. `src/app/CMakeLists.txt` does not, and the linker builds a console program:

- `dumpbin /headers` reports `3 subsystem (Windows CUI)` for both the Debug and the Release `verdandi.exe`.
- Started as Explorer, the Start menu or a shortcut starts it (`ShellExecute`), the Release build got a `conhost.exe` child process, and a Windows Terminal window titled `C:\...\build\release\src\app\verdandi.exe` opened beside the Verdandi window. spdlog's console sink writes the log into it.

So every launch of a release opens a second window that the user did not ask for. That window is also a way to end the monitor without its shutdown. Closing it, or pressing Ctrl+C in it, sends the process a console control event. Qt installs no handler for one (qtbase 6.11.1 never calls `SetConsoleCtrlHandler`), so Windows' default handler calls `ExitProcess`. The process ends without `main()`'s shutdown: the scheduler is not stopped, the window and the collectors are not destroyed, and the exit is not logged before the logger shuts down (F-43). DC-88 has End process close the monitor's own window rather than end its process for exactly this reason.

No document mentions a console. The console sink dates from Phase 0, which asked for "console and rotating file sinks", and the tests and benchmarks run in terminals and test runners, where the subsystem makes no visible difference, so nothing showed it.

**Proposed Solution:**

1. Build a GUI program: `qt_add_executable(verdandi WIN32 main.cpp verdandi.manifest verdandi.rc)`. `Qt6::Core` links `Qt6::EntryPointPrivate` into any `WIN32_EXECUTABLE`, which supplies `WinMain` and calls `main()`, so `main.cpp` stays as it is.
2. Decide what the console sink is for. In a GUI program, spdlog's colour sink finds no standard output handle and returns at once, unless the output is redirected. Either remove it, or give developers a console on request, for example a `--console` option that calls `AllocConsole()` (in `platform/windows`) before the logger is configured, since the sink reads the handle when it is created.
3. `QCommandLineParser::showHelp()`, which `main()` calls for `--help` and for an invalid `--log-level`, then shows a message box instead of printing. Check that both read well.
4. Add a check that fails if the subsystem goes back to the console: for example an integration test that reads the PE header's `Subsystem` field of `verdandi.exe`, whose path CMake passes in with `$<TARGET_FILE:verdandi>`.
5. Record the decision (the subsystem, and what became of the console sink) in a design decision.

**Recommendation:** Fix before the release.

**Status:** Resolved (2026-10-23). Items 1 to 5 are done. `dumpbin /headers` reports `2 subsystem (Windows GUI)`, and started with `Start-Process`, the monitor got no `conhost.exe` and closed with exit code 0. The standard-output sink stays and writes only where standard output is redirected; `build.ps1 -Run` pipes it, so the script prints the log and returns when the window closes. In the message box, `--help` is Qt's help, legible though its columns no longer line up; an invalid `--log-level` showed the whole help under an information icon, and now shows an error that names the value. `ApplicationExecutableIntegration.Subsystem_IsWindowsGui` reads the subsystem from the built executable, and [DC-134](../design_decisions.md#dc-134) records the decision.

---

### F-46: A new refresh interval takes effect only when the current interval ends

**Severity:** Medium
**Files:** `src/monitoring/sampling_scheduler.cpp` — `setInterval()`, `setSlowInterval()`, `run()`, `runSlowCollectors()`; `tests/unit/sampling_scheduler_*_test.cpp`

**Description:**

`setInterval()` and `setSlowInterval()` set the interval under `m_sleepMutex` and call `m_sleepCv.notify_all()`, so that a waiting thread picks the new interval up. Neither thread does:

- The tick thread waits with `m_sleepCv.wait_until(lock, stopToken, nextTick, [&stopToken] { return stopToken.stop_requested(); })`, where `nextTick` was computed from the old interval before the wait. The notification wakes it, the predicate is false, and it waits again until the old deadline.
- The slow-collector thread waits until `nextRefresh`, also computed from the old slow interval, for `m_isEventRefreshPending || m_isSlowBaselineResetPending`, which an interval change does not set.

A probe started a scheduler with both intervals at 10 s, waited for its first snapshot and its first process refresh, then set 0.5 s and 2 s, as `applyRefreshInterval()` does for an interval of 0.5 s. The next snapshot came 10,009 ms after the change, and so did the next process refresh.

So a user who shortens the interval from 10 s in File > Settings sees nothing change for up to ten seconds, although the README says "Changes apply at once and are saved", and DC-126 shows new units at once because "the next snapshot can be ten seconds away". Meanwhile `MainWindow::setSettings()` has already given the charts the new interval, so their time labels describe a spacing that the samples do not have yet. Lengthening the interval works as expected: the current, shorter wait ends, and the next one is long.

The tests read the intervals back (`SettingsService.ApplyRefreshInterval_*`, `SamplingScheduler.SlowInterval_*`), but none changes the interval of a running scheduler. The roadmap noted before Phase 6 that `setInterval()` had no caller yet.

**Proposed Solution:**

1. Make both waits notice a change of interval. For example, give each wait a predicate that is also true once the deadline computed from the current interval has passed, and recompute that deadline whenever the thread wakes: wait while `now < tickStart + m_interval`, reading `m_interval` each time, rather than until a deadline fixed before the wait. A flag that the setters set, and the predicate reads, works too. Keep the deadline measured from the start of the tick or refresh, so that a shorter interval that has already elapsed fires at once.
2. Add unit tests: a scheduler at 10 s whose interval is set to 50 ms after its first snapshot emits the next within a second, and the same for the slow interval and a process refresh.

**Recommendation:** Fix before the release. It is the main feature of this phase, and the fix is small.

**Status:** Resolved (2026-10-23). Items 1 and 2 are done. Both threads wait through one helper that reads the interval again whenever the thread wakes, still measured from the start of the tick or refresh, so a shorter interval that has already passed ends the wait at once. A longer interval now applies to the wait in progress too. `SamplingScheduler.Running_IntervalShortened_AppliesToTheWaitInProgress` and `Running_SlowIntervalShortened_AppliesToTheWaitInProgress` set 50 ms after the first snapshot and the first process refresh of a scheduler at 10 s: each failed against the old waits, and passes in under 80 ms. The scheduler's tests passed 15 shuffled repetitions. [DC-17](../design_decisions.md#dc-17) describes the waits.

---

### F-47: A log file that cannot be opened ends the monitor at startup

**Severity:** Low (rare, but the monitor then cannot start and says nothing)
**Files:** `src/app/app_logging.cpp` — `configureLogging()`; `src/app/main.cpp` — `main()`

**Description:**

`configureLogging()` constructs a `spdlog::sinks::rotating_file_sink_mt`, whose constructor opens the log file. spdlog 1.17 tries five times, 10 ms apart, and then throws `spdlog::spdlog_ex` (`file_helper::open()`). vcpkg builds spdlog with exceptions, and nothing catches it, so the exception leaves `main()` and the process ends in `std::terminate()` before any window appears. A probe gave `configureLogging()` a read-only `verdandi.log`: it threw "Failed opening file …\verdandi.log for writing: permission denied" after 83 ms.

The same happens when `QDir().mkpath()` cannot create `%LOCALAPPDATA%\Verdandi`, whose result `main()` does not check, or when another program holds the log open without sharing write access. These are rare, but the monitor then does not start and leaves no message, not even in its log. It is the same class of failure as F-19, an exception from the logging setup that ends the process at startup, against the no-exceptions rule.

**Proposed Solution:**

1. Keep the sink's exception from escaping. Either open the file first with the standard library (`std::ofstream` in append mode, with the `std::filesystem::path` overload) and leave the file sink out when that fails, or catch `spdlog::spdlog_ex` around the sink's construction alone, at the boundary with the library, and turn it into an error value. The rules forbid throwing exceptions; containing a library's exception at its boundary is the usual way to keep a codebase free of them. If the team reads the rule more strictly, record the catch in `known_deviations.md`.
2. Without a file sink, start anyway, and say once, in a message box, that the log cannot be written and where it should be.
3. Check `QDir().mkpath()`'s result.
4. Add an integration test with a read-only log file: `configureLogging()` reports the failure and the default logger still works.

**Recommendation:** Fix before the release; it is a few lines.

**Status:** Resolved (2026-10-23). Items 1 to 4 are done, with the first option of item 1: `configureLogging()` creates the directory and opens the file for appending, as the sink does, before creating the sink, so no exception is caught and no deviation is recorded. Without the file sink it returns why; `main()` logs it, and the main window shows a window-modal warning with the path and the reason, as it does for settings that could not be saved. The directory is created there, with its error checked, in place of `QDir().mkpath()`. Started with the real `verdandi.log` read-only, the monitor opened with that warning, and the file was unchanged. `AppLoggingIntegration.ConfigureLogging_ReadOnlyLogFile_ReportsItAndLogsWithoutTheFile` and `ConfigureLogging_DirectoryCannotBeCreated_ReportsWhy` cover both failures, and [DC-135](../design_decisions.md#dc-135) records the decision, with the microseconds between the check and the sink's open in which the file could still become unwritable.

---

### F-48: Uptime counts from the last full boot, which Fast Startup does not reset

**Severity:** Low
**Files:** `src/ui/dashboard_view.cpp` — `updateUptimeCard()`; `src/ui/performance_readouts.cpp` — `cpuReadouts()`; `docs/design_decisions.md` — DC-42; `roadmap.md` — "Issues found while testing"

**Description:**

The roadmap has an open item: "Uptime seems incorrect, not sure what it is actually showing." The value is correct by Windows' own definition, but the card does not say what that definition is:

| Source | Value on the review machine |
| --- | --- |
| `GetTickCount64`, which the monitor shows | 6 d 16 h 23 min |
| `Win32_OperatingSystem.LastBootUpTime` | 16 October 06:12, 6 d 16 h 23 min ago |
| `QueryUnbiasedInterruptTime`, which leaves out sleep and hibernation | 3 d 1 h 36 min |
| Fast Startup (`HiberbootEnabled`) | On |
| Microsoft-Windows-Kernel-Boot event 27 | Boot type 0x1 (a Fast Startup boot) on 20, 21 and 22 October |

The machine was shut down and powered on every day. With Fast Startup, which Windows turns on by default, a shutdown hibernates the kernel's session and the next power-on resumes it, so the tick count and Windows' boot time carry on from the last full boot: a restart starts a new count, a shutdown does not. The card's detail line says "Since last boot", which a user who switched the PC on this morning reads as this morning. Task Manager's Up time counts the same way. DC-42 chose `GetTickCount64` because it "counts time spent in sleep and hibernation, which is what users expect from 'uptime'", without considering Fast Startup.

**Proposed Solution:**

1. Say what the value counts. Show when the count started, for example "Since Windows started, 16 Oct 06:12" (the start is `std::chrono::system_clock::now()` minus the uptime, shown in local time), with a tooltip that sleep and shutting down with Fast Startup do not restart the count, and that Restart does.
2. Optionally add the time awake, from `QueryUnbiasedInterruptTime`, as a second readout on the CPU page, so both questions have an answer.
3. Update DC-42, and close the roadmap's item.

**Recommendation:** Change the wording before the release; the second readout when convenient.

**Status:** Resolved (2026-10-23). Items 1 and 3 are done; item 2, the optional time awake, is not. The dashboard's card says "Since Windows started," and the start, the current time less the uptime, to the minute in local time (`formatUptimeStart()`), and the card and the CPU page's Up time readout have the tooltip `kUptimeToolTip`, which says what starts a new count. The snapshot's comment says the same. [DC-42](../design_decisions.md#dc-42) is rewritten, and the roadmap's item is closed.

---

### F-49: Listing the tests still creates a `QApplication`

**Severity:** Low (Phase 1 code)
**Files:** `tests/test_main.cpp`; `docs/design_decisions.md` — DC-21

**Description:**

DC-21 says that the test runner "creates no `QApplication` when `--gtest_list_tests` is passed", so that listing the tests "is instant and works anywhere". `test_main.cpp` looks for `--gtest_list_tests` in `argv`, but only after `::testing::InitGoogleTest(&argc, argv)`, which removes every flag it recognizes from `argv` (`ParseGoogleTestFlagsOnlyImpl()` in GoogleTest 1.18). The loop can never find the flag, so the `QApplication` is always created. It has been so since the file was written in Phase 1 (`3f9f63f`).

With `QT_QPA_PLATFORM=nonexistent`, which makes the platform plugin fail to load, `verdandi_tests.exe --gtest_list_tests` ended with exit code 3 and listed nothing; without it, it listed the tests. The build no longer lists tests (DC-113), so nothing in the build depends on the check, but a test explorer that lists the tests starts a `QApplication` each time, and DC-21 describes behaviour that the code does not have.

**Proposed Solution:**

1. Read GoogleTest's own flag after initialization: `if (GTEST_FLAG_GET(list_tests)) { return RUN_ALL_TESTS(); }`.
2. Add a CTest test that runs `verdandi_tests --gtest_list_tests` with `QT_QPA_PLATFORM=nonexistent` in its environment and expects it to succeed.

**Recommendation:** Fix when convenient.

**Status:** Resolved (2026-10-23). Items 1 and 2 are done. Before the fix, the Debug runner did not end with exit code 3, as described above, but waited on the C runtime's error dialog for Qt's fatal error, so the CTest test, `verdandi_tests_list_without_qt_platform`, has a 30-second timeout. With the fix it passes in 0.05 s. [DC-21](../design_decisions.md#dc-21) describes the check as it now works.

---

### F-50: The release build lacks a version, its runtime, symbols and two mitigations

**Severity:** Low
**Files:** `src/app/verdandi.rc`; `src/app/verdandi.manifest`; `src/app/main.cpp`; `CMakeLists.txt`; `CMakePresets.json`; `roadmap.md` — "Packaging and distribution"

**Description:**

The roadmap's packaging items (a portable zip, signing, winget) are post-MVP, but the release that follows this phase will be some form of `build\release`. Inspecting it found these gaps:

| Gap | Consequence | Fix |
| --- | --- | --- |
| No version anywhere in the program: `project(Verdandi VERSION 0.1.0)` and `vcpkg.json`'s `version-string` are not used. `verdandi.rc` has no `VERSIONINFO`, the startup log line has no version, there is no `--version`, and there is no About box | A bug report cannot say which build it is about. Explorer's Details tab is empty, and Task Manager's Processes tab shows "verdandi.exe" where it would show a description | A `VERSIONINFO` generated from the CMake project's version (file and product version, description "Verdandi", copyright); the version, and the commit if the build knows it, in the startup log line; `QCoreApplication::setApplicationVersion()` with `QCommandLineParser::addVersionOption()`; and a Help > About box with the version, the license and Qt's notice (`QMessageBox::aboutQt()`) |
| The MSVC runtime is not deployed: the Release `verdandi.exe` imports `MSVCP140.dll`, `MSVCP140_ATOMIC_WAIT.dll`, `VCRUNTIME140.dll` and `VCRUNTIME140_1.dll`, none of which is next to it, because `VCPKG_APPLOCAL_DEPS` copies only vcpkg's DLLs | A copy of the build directory does not start on a PC without a Visual C++ Redistributable at least as new as the build's toolset ("MSVCP140.dll was not found", or a missing entry point with an older one) | Copy the runtime next to the executable (CMake's `InstallRequiredSystemLibraries`), or require the redistributable, as part of the packaging item |
| No program database for Release: CMake's Release flags have no `/Zi`, and the link no `/DEBUG` | A crash dump that a user sends cannot be symbolized | Compile Release with `/Zi`, link with `/DEBUG /OPT:REF /OPT:ICF`, and keep the PDBs of every build that is published |
| No Control Flow Guard and no CET shadow-stack compatibility: `dumpbin` lists dynamic base, NX and high-entropy addresses, but no guard flags | Two mitigations that Microsoft recommends, and that BinSkim checks, are missing from a tool that opens other processes | `/guard:cf` when compiling and linking, and `/CETCOMPAT` when linking, checked with `dumpbin /headers` |
| The manifest declares no `supportedOS` | Microsoft documents that an application without a compatibility section gets the behaviour of Windows Vista from components that look for it | A `<compatibility>` section with the Windows 10 and 11 GUID, with the open `asInvoker` item |
| No third-party notices accompany the program | The release ships Qt's DLLs (LGPL-3.0) and spdlog and fmt (MIT), whose licenses ask for their notices to go with the binaries | A `THIRD_PARTY_NOTICES` file in the release, and Qt's notice in the About box |

**Recommendation:** Decide which belong to the first release. The version and the runtime matter to every user of a zip; the rest can go with the packaging items.

**Status:** Resolved (2026-10-23). All six gaps are closed for the first release. `project()` gives the version, description and homepage, and the build adds the commit, so `verdandi.exe` has a `VERSIONINFO` with the product version "0.1.0 (edbcb13)", `--version` prints it, the startup log line names it, and Help > About Verdandi shows it with the license, beside About Qt ([DC-136](../design_decisions.md#dc-136)). Release builds copy the C++ runtime beside the executable and have a program database; every build has Control Flow Guard and CET compatibility, and the manifest declares Windows 10 and 11 and `asInvoker`, which closes the roadmap's item ([DC-137](../design_decisions.md#dc-137)). `LICENSE.txt` and `THIRD_PARTY_NOTICES.txt`, written from vcpkg's copyright files for the packages whose DLLs ship, go beside the executable ([DC-138](../design_decisions.md#dc-138)). Integration tests read each from the built files; the Release-only one failed with `msvcp140_atomic_wait.dll` hidden.

---

### F-51: CI never builds Release and lets warnings through

**Severity:** Low
**Files:** `.github/workflows/ci.yml`; `CMakeLists.txt`

**Description:**

| Gap | Consequence | Fix |
| --- | --- | --- |
| CI builds and tests the `default` (Debug) and `coverage` (Debug, clang-cl) presets only. Release is built only by hand, with `-Release` or `-Benchmark` | The configuration that ships is the one CI never builds. In Release, assertions compile out, which is why `ProcessModel::removeNode()`'s `canMove` is `[[maybe_unused]]`, and the optimizer can report warnings that a Debug build does not | Build and test the `release` preset in CI |
| Warnings do not fail the build: MSVC runs at `/W4` without `/WX`, and clang-cl without `-Werror` | A new warning passes CI. Today both compilers are clean (this review's full Release and clang-cl rebuilds had no warnings), so making warnings errors costs nothing now and keeps it so | `/WX`, and `-Werror` for clang-cl, at least in CI |
| The Coverage job holds `contents: write` and runs two actions pinned by tag, `lukka/run-vcpkg@v11` and `actions/upload-artifact@v4`, before its last step runs `scripts/coverage_pages.ps1 -Publish` with the token | As DC-121 says, only the last step gets the token, and neither action asks for it (run-vcpkg's `action.yml` has no token input). But a step that runs earlier in the same job, such as an action whose tag was moved to malicious code, can change the script that the publishing step then runs with the token | Pin third-party actions by commit SHA, or publish from a job of its own that checks the repository out afresh and only downloads the dashboard artifact |

CI's vcpkg cache, without which every job builds the dependencies, is already an open roadmap item, and `build_system.md`'s claim of "GitHub Actions-backed binary caching" is corrected there.

**Recommendation:** Add the Release build to CI before the release; the rest when convenient.

---

### F-52: Documentation out of date

**Severity:** Low
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| DC-54 says "The Processes and Network views will follow the same pattern when they are implemented." Both exist: the Network view records every snapshot and updates its widgets only while shown, and the Processes view applies every list | `design_decisions.md` — DC-54 | Describe the four views as they are |
| The system design diagram lists "WMI/CIM one-shot queries at startup only (hardware inventory)", and the API table "Hardware inventory" (WMI) and "Per-disk I/O rates" (PDH). The code has none of them, so they read as if the monitor queried WMI. The module table gives `domain` "commands", which do not exist. Data flow step 2 says the fast collectors return "any access or availability errors"; they return `std::nullopt` and log the error. The logging table's warn row has "connectivity probe timeout"; there are no probes | `architecture.md` | Mark the WMI and PDH rows as future, or remove them, and describe `domain` and the collectors as they are |
| The README says that changed settings "apply at once" (see F-46), calls the memory charts "in use and committed" while the page says "Used" (DC-63), and says neither where the settings and the log are nor that `--log-level` exists | `README.md` | Correct the two phrases, and add a short section: `%APPDATA%\Verdandi\settings.json`, `%LOCALAPPDATA%\Verdandi\verdandi.log`, and `--log-level` |
| `Column::Memory`'s comment says "Working set."; the column shows the private working set (code review F-29). The class comment says missing values show "in the placeholder text colour"; since F-37 they use `dimmedTextColor()` | `src/ui/process_model.h` | "Private working set."; "in the dimmed text colour" |
| `ConnectivityStatus`'s comment says "the UI uses this to warn users before triggering network-heavy operations"; the monitor has none | `src/domain/connectivity_status.h` | Remove the clause |
| The comment says that domain headers can also be included without `domain/` "from unit tests that add src/domain explicitly"; no test does | `src/domain/CMakeLists.txt` | Remove the second form |
| Examples that break the guidelines' own rules: the RAII `ProcessHandle` example has `if` statements without braces; the header example opens its namespace with a brace on the same line, where "Indentation and braces" puts it on its own; the `RingBuffer::push` example opens a function the same way; the error-handling example's `CollectResult` and `CollectorError` do not exist; and the macro example is `SYSTEM_MONITOR_VERSION` | `docs/coding_guidelines.md` | Correct the examples; `VERDANDI_VERSION` |
| The guidelines say that standard library namespaces "still use targeted `using` declarations", but eight test files use `using namespace std::chrono_literals` | `docs/coding_guidelines.md`; `tests/unit/` | Allow the literal namespaces in the rule, which the C++ Core Guidelines (SF.7) permit |
| DC-123 has no `---` line before DC-124; every other entry is followed by one | `docs/design_decisions.md` | Add it |
| `.editorconfig` asks for LF line endings in every file, while `.gitattributes` checks `.ps1`, `.bat`, `.cmd`, `.manifest` and `.rc` files out with CRLF | `.editorconfig` | A CRLF section for those extensions |
| `.gitkeep` files remain in two directories that have files | `src/platform/windows/`, `tests/integration/` | Remove them |

**Recommendation:** Fix when convenient, together with the findings that change the same files.

---

### F-53: Consistency and presentation items

**Severity:** Cosmetic
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| `formatBytes()` and `formatNetworkRate()` choose the unit before rounding, so a value just below a boundary shows four digits: 1,048,575 bytes is "1024.0 KiB", 999,960 bytes "1000.0 KB", and 124,994 B/s in bits "1000.0 Kbps". `formatLinkSpeed()` rounds first for this reason (DC-57), and `chartNetworkRateAxisTemplates()` sizes the chart gutters for the four digits | `src/ui/dashboard_formatting.cpp` — `formatScaled()` | Round to one decimal, and take the next unit when the result reaches the step |
| The dashboard's CPU detail reads "1 cores" on a machine with one logical processor, such as a small virtual machine | `src/ui/dashboard_view.cpp` — `updateCpuCard()` | "1 core" |
| The dashboard card and the Settings dialog say "Uptime", the CPU page's readout "Up time" | `dashboard_view.cpp`, `settings_dialog.cpp`, `performance_readouts.cpp` | One spelling |
| `ReadoutGrid::setToolTip(std::size_t, const QString&)` hides `QWidget::setToolTip(const QString&)`, so no caller can set the grid's own tooltip | `src/ui/readout_grid.h` | Another name, such as `setReadoutToolTip()` |
| `formatIpAddress()` calls `inet_ntop`, which is `InetNtopA`, where the rules ask for wide variants. Addresses are ASCII, so nothing breaks | `src/platform/windows/ip_address_mapping.cpp` | `InetNtopW` with `wideToUtf8()`, or an exception stated in the rule |
| `queryGroupProcessorPerformance()` and `queryLegacyProcessorPerformance()` repeat the same resizing loop and conversion | `src/platform/windows/cpu_collector.cpp` | One helper that takes the call, when the coverage item injects these queries |
| The log levels' names are written out twice: `kLogLevelNames` for the settings file and `kLevels` for `--log-level`, each tested on its own, so nothing keeps them alike | `src/persistence/settings_file.cpp`, `src/app/app_logging.cpp` | One table in `domain`, such as `domain::logLevelName()` |
| `MainWindow::setSettings()` reaches into `ProcessesView`'s model for the byte units, while the other views have a `setDisplayUnits()` | `src/ui/main_window.cpp` | `ProcessesView::setDisplayUnits()` |
| `MainWindow::onSnapshotReady()` checks for null four views that the constructor always creates | `src/ui/main_window.cpp` | Remove the checks |
| Abbreviations that the naming guidance asks to avoid: `funcs`, `desc`, `uni`, `curr` and `secs` | `cpu_collector.cpp`, `network_collector.cpp`, `dashboard_formatting.cpp` | Full words |
| Struct members without the default member initializer that AGENTS.md asks for: `ProcessRateBaseline::timestamp` (while `NetworkBaseline::timestamp` has `{}`), `SeriesSummary`'s three members and `ProcessTerminationMessage`'s two | `process_collector.h`, `sparkline_widget.h`, `process_formatting.h` | Add `{}` |
| `SamplingScheduler`'s default interval is a literal 1000 ms rather than `domain::kDefaultRefreshInterval`, and `m_healthThresholds` is declared among the collectors | `src/monitoring/sampling_scheduler.h` | Use the constant; move the member |
| The Processes tab's Network column always reads "N/A" until per-process network usage is collected with ETW | `src/ui/process_model.cpp` | Optional: hide the column until it has data (DC-79 chose to show it) |

**Recommendation:** Fix when convenient, in the commits that touch these files for the other findings.

---

## Risks Not Reproduced

- **Two instances at once.** Nothing stops a second instance; the roadmap's single-instance item is post-MVP. Both write `verdandi.log`, which spdlog opens with `_SH_DENYNO`: writing is shared, but renaming is not. When either instance rotates the log at 5 MB, the rename fails twice, 100 ms apart, on whichever thread is logging, and spdlog 1.17 then truncates the file and reports the error on standard error, which a GUI program does not show (`rotating_file_sink::rotate_()`). Settings saves of the two instances race through the same `settings.json.tmp`. Not tried, because rotation needs 5 MB of log, which takes long at the default level. If the single-instance item stays post-MVP, a log name per instance would avoid the truncation.
- **No back-pressure on snapshots.** Each snapshot reaches the UI as a queued signal, whether or not the UI has handled the previous one, and Qt does not merge them. At 0.5 s, the heaviest Performance page at its 30-minute window costs about 35 ms per tick on the review machine; on a machine several times slower, the UI could fall behind, and the queue would grow without bound. Not reproduced here. If the profiling item finds it, the scheduler could skip emitting while a snapshot is still queued, at the cost of a gap in the histories.
- **Per-core gaps on processors that park cores.** The CPU collector drops every core's value for a tick in which one core's counters did not advance (DC-12). On the review machine, 80 consecutive samples at 0.5 s and 40 at 1 s all had their per-core values. A laptop that parks cores more deeply might leave a core's counters still between two ticks, which would put a gap across the whole per-core grid. Worth checking on the lower-end machine of the profiling item.
- **The settings file after a power loss.** `saveSettingsFile()` writes a temporary file and renames it over the settings file, but never flushes it to disk, so after a power loss shortly after a save, NTFS can keep the rename without the data. The next start would then read an empty or zero-filled file, use the defaults, and keep the damaged file until the next change. The standard library cannot flush a file to disk; `FlushFileBuffers()` before the rename would need a helper in `platform/windows`. A lost settings file costs little, so DC-123 may accept the risk in a sentence.

---

## Items Verified — No Issues Found

- **Module boundaries:** as listed in the overall assessment, checked by include across `src/` and `tests/`. `tests/unit/` and `tests/support/` include no Windows header; the `platform/windows` headers they include expose no Windows types. `app` alone includes from every module, as the composition root.
- **Settings file:** parsed without exceptions (`json::parse(…, nullptr, false)`, type checks before every `get()`, `dump()` with the `replace` error handler, and a SAX pass for the line and column of a syntax error). A file that starts with a UTF-8 byte order mark reads correctly. Intervals are clamped and rounded to half-second steps without overflow, from any 64-bit integer. Unknown keys, a newer version and wrong types are reported and keep their defaults. A save goes through a temporary file, and a failed save keeps the settings applied and shows a window-modal message whose path and reason are plain text; Qt 6.11.1's `setInformativeText()` gives the informative label the box's text format.
- **Settings dialog:** edits a copy and changes nothing until OK; a typed interval moves to its step when editing ends, whatever the locale; mnemonics do not collide; every log level and unit has an item.
- **Settings at startup:** read before the logger is configured, and logged after; `--log-level` overrides the file's level for one run without saving it, and keeps doing so while other settings change; the slow collectors run every 2 s or every interval, whichever is longer.
- **Sleep and resume:** Qt passes `WM_POWERBROADCAST` to native event filters from its window procedure; both scheduler threads wait while suspended and check before each collection; each resets its own rate collectors' baselines, without a lock; the tick discards its first snapshot after a resume and the slow-collector thread keeps its first refresh, as DC-129 says.
- **DPI and manifest:** the built `verdandi.exe` carries `PerMonitorV2`, the UTF-8 code page and the linker's `asInvoker`, and `ApplicationManifestIntegration` checks the first.
- **Threading:** locks are taken collector first, then the sleep mutex, everywhere; listeners are called and cleared under the collector's mutex; the scheduler clears them under `m_collectorMutex` after stopping its threads; the network-name worker is joined after its subscription is cancelled; interval and suspend state are read only under `m_sleepMutex`.
- **Process termination:** unchanged since Phase 4 and correct: never-opened processes are refused before `OpenProcess`, the identity is checked on both handles, the owner and elevation before `PROCESS_TERMINATE` is requested, and an exited process is told from a denial.
- **Text from outside the program:** every label that shows outside text is plain text, the process tooltips escape theirs, and the dashboard's script data escapes `<` so that no test name can end its script element.
- **Memory over time:** every collection is bounded by what it tracks: baselines and caches are dropped with their processes, adapters and volumes; histories are fixed-size ring buffers, and an adapter's or volume's history is dropped once it has left and holds no values; the log rotates at 5 MB × 3.
- **Snapshot cost:** copying a snapshot with the review machine's real list of 368 processes, about 224 KB with their paths and command lines, took 31.6 µs in Release. A tick copies it twice (under the lock, and into the queued signal), which is negligible.
- **The monitor's own cost** (real collectors, the window on screen, Release, 20 s per row after 10 s of warm-up; this machine is not a lower-end system, so the roadmap's profiling item stays open):

  | Tab | 1 s: % of one logical processor | 0.5 s |
  | --- | --- | --- |
  | Dashboard | 0.86 | 2.19 |
  | Performance, CPU page, 1-minute window | 1.72 | 2.66 |
  | Processes, flat | 0.39 | 0.62 |
  | Processes, tree | 0.62 | 1.25 |
  | Network | 0.94 | 0.78 |

  The working set stayed between 45 and 48 MiB and the private bytes between 16 and 18 MiB.
- **Tick cost:** three runs of `.\scripts\build.ps1 -Benchmark` with the environment lines of the 2026-10-18 baseline. The first, straight after the full Release rebuild, was slower on every row; the second and third agreed with each other and were below the baseline on every row, most by 20–30%, so the code costs no more than it did. Mean ms per tick in the second and third runs:

  | Page or tab | Baseline (2026-10-18) | This review |
  | --- | --- | --- |
  | CPU, 16 cores, 1 / 5 / 30 min | 8.8–10.3 / 14.5–15.8 / 43.4–44.9 | 6.8–6.9 / 11.5–11.8 / 35.5–35.6 |
  | CPU, 64 cores, 30 min | 38.0–41.3 | 29.7–29.8 |
  | Disk, 3 volumes, 30 min | 42.2–46.1 | 33.8 |
  | Network tab, 16 adapters | 6.8–6.9 | 4.9–5.0 |
  | Processes tab shown, 2,000, tree by CPU, new / repeated list | 9.5–12.0 / 9.0–11.4 | 7.9–8.5 / 7.6–7.8 |

  `performance.md`'s baseline stands; this review records no new one.
- **Rename:** no "System Monitor", `sysmon` or `system_monitor` remains outside DC-131, which names the old folder on purpose, and the roadmap's records of earlier work.
- **Rule files:** `AGENTS.md` and `GEMINI.md` are identical, and `.github/copilot-instructions.md` and the module `AGENTS.md` files agree with them, including the Phase 6 rules for `persistence`.
- **Known deviations:** none is open, and this review records none: F-47's catch and F-53's `inet_ntop` are each either a code change or a sentence in the rule.
- **Tests:** 783 tests, each named `TypeName_Scenario_ExpectedResult`; mappings are tested as tables of named cases; shared builders live in `tests/support/`. The slowest test takes 1.3 s, and their times add up to about 21 s, which the 33 shards finish in about 2 s.
