# Code Review — Phase 4 Completion

**Date:** 2026-10-15
**Scope:** Everything added or changed in Phase 4 (commits `9d73941`..`1d0a689`): the process list reader and parser (`SystemProcessInformationReader`, `parseSystemProcessInformation()`), `ProcessCollector` with its identity cache, details reader, access policy and rate calculation, the slow-collector thread in `SamplingScheduler`, the Processes tab (`ProcessModel`, `ProcessSortFilterProxyModel`, `ProcessesView`, `process_formatting`, `process_icons`), process termination (`terminateOwnedProcess()`, `runsElevated()`, the End process action and its messages), the process table benchmark, the `build.ps1` configure retry, the qtbase PNG feature, their tests, and the related documentation. Phase 1–3 files that Phase 4 touched were checked again.
**Reviewed against:** [architecture.md](../architecture.md), [coding_guidelines.md](../coding_guidelines.md), [design_decisions.md](../design_decisions.md), [known_deviations.md](../known_deviations.md), [roadmap.md](../../roadmap.md), [AGENTS.md](../../AGENTS.md) and the module `AGENTS.md` files
**Previous reviews:** [Phase 1](phase_1.md) (F-1 to F-8), [Phase 2](phase_2.md) (F-9 to F-19), [Phase 3](phase_3.md) (F-20 to F-28). Finding IDs continue from there.

---

## Overall Assessment

Phase 4 is the largest phase so far, with 21 commits, 91 files, about 9,100 added lines and 228 new test cases. It is also the most security-sensitive phase, and the code holds up well:

- **Boundaries:** module boundaries hold. The UI ends processes through an injected `ProcessTerminator` and never names a platform function.
- **Identity:** every per-process structure is keyed by `(PID, creation time)`.
- **Protected processes:** `lsass.exe` and `csrss.exe` are filtered in three places before any `OpenProcess` call.
- **Termination:** `PROCESS_TERMINATE` is requested only after the identity, owner and elevation checks pass, while a first handle keeps the PID from being reused.
- **Threading:** D-1 and D-2 are resolved by a slow-collector thread that is small, correctly locked and tested.
- **Build:** a full recompile has no warnings, formatting is clean, and all 608 tests pass. The two tests that need an elevated run are skipped when unelevated.

This review found no high-severity defect. It found one **medium-severity** issue, which every user sees:

- The Memory column shows each process's full working set, which counts shared pages again for every process that maps them. On the review machine the column adds up to 33.2 GiB while 30.5 GiB of memory is in use. Browser processes show up to 1.8 times their private working set, the figure Task Manager's Memory column shows (F-29).

A probe confirmed each of the low findings, which are edge cases of the new features:

- The tree can show a new process under an unrelated process that has already exited (F-30).
- End process is offered for exited rows and for the monitor itself, which then stops without a clean shutdown (F-31).
- The `lsass.exe`/`csrss.exe` guard trusts the image name its caller passes, so it fails open when the name is missing (F-32).

Most of the documentation's detail is accurate, but a few recorded figures and one stated mechanism no longer hold (F-33).

As in Phase 3, the findings came from running the code against real data and edge cases, not from reading it. No new test is wrong. But none of them models a PID reused by a process the list never saw, the monitor's own row, or memory shared between processes.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | |
| **Phase 2** — Real Windows collectors & dashboard | ✅ Complete | |
| **Phase 3** — Sparkline charts & performance views | ✅ Complete | All Phase 3 findings are resolved |
| **Phase 4** — Process explorer | ✅ Complete | This review was the final unchecked item. D-1 and D-2 are resolved. F-29 should be fixed before Phase 5 |
| **Phase 5–6** — Network view, settings | ⬜ Not started | The adapter `interfaceId` from F-23 is ready for the network view |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐½ | Boundaries hold, and the UI ends processes through an injected function typed with domain values. The `lsass.exe`/`csrss.exe` rule depends on a name the caller supplies (F-32) |
| Naming conventions | ⭐⭐⭐⭐½ | Consistent. One C-style cast in a test, and one older test name without a scenario part (F-34) |
| Formatting | ⭐⭐⭐⭐⭐ | `-CheckFormat` passes on all 164 files |
| C++20 usage | ⭐⭐⭐⭐⭐ | `std::span` buffers, designated initializers throughout, `<=>` with `std::lexicographical_compare_three_way` for names, `std::jthread` and `std::stop_token`, `std::erase_if`, ranges with projections |
| Error handling | ⭐⭐⭐⭐½ | `ProcessDetailsStatus`, optional rates and a seven-value `ProcessTerminationResult` replace flags and zeros. Every Win32 failure is mapped and logged at the right level. The never-opened check fails open on an empty name (F-32) |
| Memory management | ⭐⭐⭐⭐⭐ | `ScopedHandle` for every process and token handle, model nodes owned by `std::unique_ptr` in one map, dialogs parented and `WA_DeleteOnClose` |
| Threading | ⭐⭐⭐⭐⭐ | D-1 and D-2 resolved with one slow-collector `std::jthread`. Collectors run outside every lock, the lock order is consistent, `stop()` wakes both waits, and all of it is tested |
| Security and access rights | ⭐⭐⭐⭐½ | Details are read with limited access only. Termination checks the identity, owner and elevation before it asks for `PROCESS_TERMINATE`, and holds the first handle so the PID cannot be reused. F-32 is the gap |
| Testing | ⭐⭐⭐⭐ | 228 new test cases, a seeded churn test, an injected list query for the resize loop, and integration tests that hold elevated and unelevated. No test models a PID reused by an unlisted process (F-30), an exited row or the monitor's own row (F-31), or shared memory (F-29). The churn test does not filter or toggle the tree (F-34) |
| UI and presentation | ⭐⭐⭐½ | Memory column (F-29), short-lived wrong parents in the tree (F-30), End process on exited rows and on the monitor itself (F-31), the Network column's sort (F-34) |
| Build system | ⭐⭐⭐⭐⭐ | The configure retry, the PNG feature with a test that fails if it is dropped, and the second benchmark under `-Benchmark` |
| Documentation | ⭐⭐⭐⭐ | Twenty-two thorough design decisions (DC-69 to DC-90), most with measurements, and the rule files were updated together. Some recorded figures and one stated mechanism no longer hold (F-33) |

### Verification performed

- `.\scripts\build.ps1 -Test` (Debug): 608 tests; 606 passed and 2 were skipped because they need an elevated run. This review ran unelevated and did not re-run them elevated.
- `.\scripts\build.ps1 -Release -Test` after touching every C++ source, which forces a full recompile of 99 translation units at `/W4`: no warnings and the same test results.
- `.\scripts\build.ps1 -CheckFormat`: all 164 files match `.clang-format`.
- `.\scripts\build.ps1 -Benchmark`, run three times. It was also run once for the end-of-Phase-3 code (`dde63e6`), built in a temporary worktree against the same vcpkg manifest, to tell a regression from a change in the environment (F-33).
- **Probed the model and proxy** with throwaway unit tests, removed afterwards:
  - 400 refreshes of random churn in tree mode, with a filter, a CPU sort and periodic tree-mode toggles. After each update the proxy was compared with an independently computed filtered tree.
  - A filter match that moves to the top level, and one that exits, under recursive filtering.
  - A PID reused by a process the list never saw (F-30).
  - End process on an exited row (F-31).
  - The Network column's first sort (F-34).
- **Probed real data** with a throwaway integration test, also removed:
  - Per-process CPU over a 2-second interval.
  - Process counts against `GetPerformanceInfo`, and the details statuses.
  - The size of the strings in the list, and the cost of copying it.
  - The model's cost with real paths and command lines.
  - Working set against private working set (F-29).
  - Whether an exited process that another handle keeps open stays listed (F-33).
- **Measured PID reuse** by starting 150 short-lived `cmd.exe` processes one after another (F-30).
- **Searched for banned constructs:** exceptions, `new`/`delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, `qDebug`/`std::cout`/`printf`, `using namespace std`, Tool Help, PDH, `PROCESS_ALL_ACCESS` and `PROCESS_VM_READ`. The search found one C-style cast in a test (F-34). Windows headers appear only in `platform/windows` and in the platform and integration tests, and `PROCESS_TERMINATE` appears only in `process_terminator.cpp`.

---

## Findings

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [F-29](#f-29-memory-column-counts-shared-memory-once-per-process) | Memory column counts shared memory once per process | Medium | Open |
| [F-30](#f-30-tree-can-show-a-new-process-under-an-exited-unrelated-one) | Tree can show a new process under an exited, unrelated one | Low | Open |
| [F-31](#f-31-end-process-is-offered-for-exited-rows-and-for-the-monitor-itself) | End process is offered for exited rows and for the monitor itself | Low | Open |
| [F-32](#f-32-the-never-opened-guard-trusts-the-callers-image-name) | The never-opened guard trusts the caller's image name | Low | Open |
| [F-33](#f-33-documentation-and-recorded-figures-out-of-date) | Documentation and recorded figures out of date | Low | Open |
| [F-34](#f-34-presentation-and-consistency-items) | Presentation and consistency items | Cosmetic | Open |

**Open findings from earlier reviews:** none.

---

### F-29: Memory column counts shared memory once per process

**Severity:** Medium
**Files:** `src/ui/process_model.cpp` — `displayText()`, `headerToolTip()`; `src/ui/process_sort_filter_proxy_model.cpp` — `compareColumn()`; `src/domain/process_info.h`; `src/platform/windows/system_process_information.cpp` — `toProcessInfo()`; `docs/design_decisions.md` — DC-80

**Description:**

The Memory column shows `workingSetBytes`, the full working set (`WorkingSetSize`). A working set includes shared pages, such as DLL code and memory that a browser's processes share. Each shared page is counted again for every process that maps it. The header tooltip calls the value "the physical memory the process is using", which reads as memory that belongs to the process.

Task Manager's Memory column shows the private working set instead, so the two tools disagree for most processes. These values come from the same process list on the review machine:

| Process | Working set (our Memory column) | Private working set |
| --- | --- | --- |
| brave.exe | 594.5 MiB | 495.0 MiB |
| MsMpEng.exe | 484.2 MiB | 301.8 MiB |
| Code.exe | 384.0 MiB | 271.4 MiB |
| brave.exe | 378.7 MiB | 207.0 MiB |
| firefox.exe | 373.2 MiB | 215.2 MiB |
| All 399 processes | 33.2 GiB | 17.2 GiB |

The column adds up to 33.2 GiB on a machine with 30.5 GiB in use (total minus available), so it cannot be read as each process's share of memory. Some users add up a browser's processes, and others compare a process with Task Manager. Both get a figure up to 1.8 times too high, and nothing explains why. This is the same kind of disagreement as F-22, in the column users sort by to find the process that uses the most memory.

Every list entry already has the private working set in `FullSystemProcessInformation::WorkingSetPrivateSize`, which the parser reads into its copy of the entry and then drops. The roadmap asked for "Memory (working set)", and DC-80 records that choice without comparing it with Task Manager's figure.

**Proposed Solution:**

1. Add `privateWorkingSetBytes` to `ProcessInfo`, set it from `WorkingSetPrivateSize` (clamped like the other `LARGE_INTEGER` fields), and show it as Memory. Sort by it too.
2. Give the Memory cell a tooltip with each figure, for example "Private working set: 207.0 MiB", "Working set: 378.7 MiB (includes memory shared with other processes)" and "Commit size: …". `privateBytes` is collected but not shown anywhere yet.
3. Reword the header tooltip, for example: "Private working set: the physical memory this process uses on its own, as Task Manager shows it".
4. Update DC-80 and the column list in the roadmap. Extend `Parse_SingleEntry_ExtractsEveryField` and the model's value and sort tests.

**Recommendation:** Fix before Phase 5. The value is already in the parsed entry, so the change is small.

**Status:** Open.

---

### F-30: Tree can show a new process under an exited, unrelated one

**Severity:** Low
**Files:** `src/ui/process_model.cpp` — `findParent()`, `insertNodes()`, `setTreeMode()`; `tests/unit/process_model_test.cpp` — `TreeMode_RandomChurn_…`; `docs/design_decisions.md` — DC-82, DC-84

**Description:**

`findParent()` chooses "the latest listed process with its parent PID that was created no later than itself". The creation-time check rejects a parent PID that a younger process has reused. Since DC-84, an exited process keeps its row for three seconds and stays a candidate parent, so that a child it started just before it exited appears under it.

The rule does not check that the candidate was still running when the child was created. Here is the failure:

1. Process P is listed with PID 100, and then exits.
2. A short-lived launcher L gets PID 100, starts process C, and exits before the next refresh, so L is never listed.
3. At the next refresh C appears with parent PID 100.

P is the only process with that PID, it is older than C, and its row is still shown, so `findParent()` puts C under P. But P had exited before C was created. A probe reproduced this: P was seen exiting at the second refresh, and C was created long afterwards and appeared at the third refresh.

```text
System | old.exe | ..child.exe |
```

C moves to the top level when P's row is removed, so the wrong tree lasts at most three seconds. `setTreeMode()` relinks through the same function and can make the same link.

The case is not rare. Launchers that start a program and exit at once are common, and DC-84 itself mentions them. On the review machine, Windows gave a freed PID to a new process after as few as 6 launches, about 0.4 seconds. The churn test reuses PIDs, but it checks only that a parent has the child's parent PID and is no younger, and P passes both checks.

**Proposed Solution:**

A process that first appears in a refresh was created after the previous list was read, or it would have been in that list. Its parent was running when it was created, so the parent was either in the previous list or was never listed. An exited row is therefore a possible parent only for a process that was listed no later than the refresh that marked the row as exited.

1. Give `Node` the time it was first listed (the `now` of the update that added it), kept for the life of the row, unlike `startedAt`.
2. In `findParent()`, skip a candidate whose `exitedAt` is earlier than the child's first-listed time. This keeps DC-84's case: a child that appears in the same refresh in which its parent is marked exited is still placed under it. It covers `setTreeMode()` too, because both times are kept.
3. Add the probe as a unit test. In the churn test, also check that a parent was not marked exited before its child was first listed; the test already records both times.

**Recommendation:** Fix when convenient. It is a short change to one function, plus a test.

**Status:** Open.

---

### F-31: End process is offered for exited rows and for the monitor itself

**Severity:** Low
**Files:** `src/ui/processes_view.cpp` — `updateEndProcessAction()`, `confirmEndProcess()`; `src/ui/process_model.h`; `src/app/app_logging.cpp`

**Description:**

`updateEndProcessAction()` enables End process whenever the current row holds a process. Two kinds of row should not get the normal flow:

- **Exited rows.** The model knows that a red, italic row has exited, and its tooltip says so. A probe confirmed that the action stays enabled on such a row. The dialog asks "Do you want to end notepad.exe (PID 900)?", and confirming produces "notepad.exe (PID 900) has already exited.", which the table already showed. The action is not re-checked when the current row becomes an exited row, because that does not change the current index.
- **The monitor's own row.** The table lists `system_monitor.exe`, and nothing treats it specially. When the monitor runs unelevated, every check passes (same user, not elevated), so confirming calls `TerminateProcess` on the monitor itself. It stops at once, so `scheduler.stop()` and `spdlog::shutdown()` never run, and the "Terminated" log line is never written. The log flushes on its own only at `warn` (`flush_on` in `app_logging.cpp`), so info-level lines written since the last flush are lost too. When the monitor runs elevated, the request is refused as "Elevated", which reads oddly when the monitor is asked to end itself.

**Proposed Solution:**

1. Let `ProcessModel` say whether a row has exited, for example with `bool hasExited(const QModelIndex&) const`, and disable End process for such a row. Re-check the action after every `updateSnapshot()`, which is cheap.
2. Compare the selected PID with `QCoreApplication::applicationPid()`. Qt provides that value, so the UI needs no Windows call. After a confirmation worded for the monitor ("Close System Monitor?"), close the main window so the normal shutdown path runs. Task Manager also lets users end it from its own list, so offering this matches what they expect. Alternatively, disable the action for that row.
3. Add view tests for both rows.

**Recommendation:** Fix when convenient, together with any other change to the End process action.

**Status:** Open.

---

### F-32: The never-opened guard trusts the caller's image name

**Severity:** Low
**Files:** `src/platform/windows/process_access_policy.cpp` — `isNeverOpenedProcess()`; `src/platform/windows/process_details_reader.cpp` — `readProcessDetails()`; `src/platform/windows/process_terminator.cpp` — `terminateOwnedProcess()`; `src/platform/windows/system_process_information.cpp` — `imageNameOf()`; `docs/design_decisions.md` — DC-74

**Description:**

Both functions that call `OpenProcess` refuse `lsass.exe` and `csrss.exe` by comparing `ProcessInfo::imageName`, a value their caller supplies. An empty name passes the check, and two paths can produce one:

- **A name outside the buffer:** the parser leaves a name empty when its characters are not inside the returned buffer (`imageNameOf()`). The collector's filter and the details reader then open the process, whatever it is. This did not happen on the review machine, where only the Idle process had no name. But the parser allows for it, and nothing after the parser treats it as a reason not to open the process.
- **A caller that sets only the identity:** `terminateOwnedProcess()` is a public function that takes any `ProcessInfo`. A caller that fills in only the identity, as the integration tests do for PID 0 (`ProcessInfo{.pid = 0}`), passes the check with any PID, including lsass's.

DC-74 says that the reader's second check means "code that calls it directly cannot open them either". That holds only when the caller passes the right name. The architecture's rule has no exceptions ("Never call `OpenProcess` on `lsass.exe` or `csrss.exe`"), so the guard should fail closed.

**Proposed Solution:**

1. Treat an empty image name as never opened for every PID except 0. Such a process gets `NotQueried`, and a request to end it is refused as `NeverOpened`. The Idle process keeps its current handling.
2. State on `terminateOwnedProcess()` and `ProcessDetailsReader` that `imageName` must come from the process list, and correct DC-74.
3. Add unit tests for an empty name in `process_access_policy_test.cpp`, and check that `RealHost_IdleProcess_NotOwnedByCurrentUser` still holds.

**Recommendation:** Fix when convenient. It is a one-line rule with tests, and it closes the only path by which the rule could be broken.

**Status:** Open.

---

### F-33: Documentation and recorded figures out of date

**Severity:** Low
**Files:** as listed

**Description:**

The Performance tab benchmark no longer reproduces the figures in DC-66 and `architecture.md`. Mean milliseconds per tick:

| Page and chart window | Recorded (DC-66) | Current code today, 3 runs | End-of-Phase-3 code (`dde63e6`) today |
| --- | --- | --- | --- |
| CPU, 16 cores, 1 min | 7.6 | 9.1–10.5 | 9.3 |
| CPU, 64 cores, 1 min | 14.3 | 18.6–19.1 | 18.0 |
| Memory, 1 min | 4.0 | 5.3–6.1 | 5.9 |
| Disk, 5 min | 23.8 | 29.7–32.7 | 29.9 |
| CPU, 16 cores, 30 min | 39.1 | 45.8–50.1 | 45.3 |
| Disk, 30 min | 38.7 | 45.8–48.3 | 47.5 |

DC-66 says its means stayed within 3 ms over three runs. Today they are 25–45% higher, with single ticks up to 78 ms. The Phase 3 code measures the same today, so Phase 4 did not cause the increase. The toolchain or the machine's state changed, and the tables do not record which. The Processes tab's figures are about 15–20% above DC-83 and DC-84 in the same way.

| Item | Location | Fix |
| --- | --- | --- |
| The tick-cost figures above: "about 4–15 ms per page at the 1-minute window and up to about 40 ms at 30 minutes" | `architecture.md` — technology table (Charts) and "Sparkline chart design"; DC-66; DC-83 | Re-measure and update the figures. Record the compiler, Qt version and Windows power plan with each table, or have the benchmark print them, so the next drift can be told apart from a regression |
| DC-78 and the test say that an exited process can stay listed because "another process, such as an antivirus scanner" holds a handle to it. A probe kept its own handle to a terminated child open, and 0.5 seconds later the child was no longer listed. A handle does not keep a process in the list. The test's polling is harmless, but the reason it states is wrong | DC-78; `tests/integration/process_tracking_test.cpp` — `RealHost_ChildProcessExits_DisappearsFromList` | Say only that the list can take a moment to drop an exited process, or find out what keeps it there |
| The reader's buffer "doubles when no size is reported", but the extra eighth of headroom is then added as well, so the buffer grows to 2.25 times its size | DC-70; `system_process_information_reader.h` — class comment | Describe the growth as it is, or skip the headroom when doubling |
| DC-76 says that dividing by every logical processor "makes the column add up to the total CPU usage". Over a 2-second interval, the column added up to 98.7%, including the Idle process. Interrupt and DPC time is charged to no process (Task Manager shows it as "System interrupts"), and processes that exited during the interval drop out | DC-76 | Say that the column adds up to slightly less than the total, and why |
| DC-74 says the reader's second check stops direct callers from opening `lsass.exe` and `csrss.exe` | DC-74 | See F-32 |

**Recommendation:** Fix when convenient. Update DC-80 together with F-29.

**Status:** Open.

---

### F-34: Presentation and consistency items

**Severity:** Cosmetic
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| The first click on the Network header sorts the names Z to A. Every Network value compares equal, so the comparison falls back to the name, and the header's first sort order is descending, as for every measured value. A probe got "zeta.exe, alpha.exe" | `process_model.cpp` — `headerData()` (`Qt::InitialSortOrderRole`); `process_sort_filter_proxy_model.cpp` — `compareColumn()` | Give Network an ascending first order, or do not sort by it until it has data |
| One C-style cast, `(void)collector.collect();`. The rest of the code uses `static_cast<void>` (7 places) | `tests/unit/process_collector_test.cpp:52` | `static_cast<void>(collector.collect());` |
| The random-churn test runs only in tree mode without a filter. It never toggles the mode, and it does not check that the proxy is sorted, although the proxy defers sorting with logic of its own (DC-83). A probe that added all three passed 400 refreshes, so this is a gap in coverage rather than a defect | `tests/unit/process_model_test.cpp` — `TreeMode_RandomChurn_…` | Add a filter, periodic `setTreeMode()` toggles, a sort-order check at each level, and a comparison with an independently filtered tree |
| `m_detailsIcons` is a `std::array<QIcon, 4>` indexed by the enum's value. A new `ProcessDetailsStatus` value would read past its end without a compile error | `process_model.h` | Size the array from a count constant next to the enum, or `static_assert` the enum's last value |
| `setProcessCollector()` is declared after `setHealthThresholds()`, outside the block whose comment states the registration invariant | `sampling_scheduler.h` | Move it next to the other `set…Collector()` declarations |
| A test name from Phase 2 still has no scenario part; F-26 renamed a similar one | `tests/unit/sparkline_widget_test.cpp` — `SparklineAutoRange.UsesLargestSampleWithHeadroomAndIgnoresGaps` | For example `MixedSamplesWithGaps_UsesLargestWithHeadroom` |

**Recommendation:** Fix when convenient, ideally in the commits that touch these files for F-29 to F-31.

**Status:** Open.

---

## Items Verified — No Issues Found

- **Module boundaries:**
  - `ui` includes no Windows headers. Process data reaches it only as `domain::ProcessInfo`, and termination only as the `ui::ProcessTerminator` that `main.cpp` sets.
  - The new `domain` types (`ProcessIdentity`, `ProcessDetailsStatus`, `ProcessTerminationResult`) use only the standard library.
  - The headers that expose SDK types (`system_process_information.h`, `process_handle.h`, `process_elevation.h`) say that they are internal, and only platform sources and the platform and integration tests include them. The new platform headers that unit tests include pull in no Windows headers.
- **API choices:**
  - The process list comes from `NtQuerySystemInformation(SystemProcessInformation)`, resolved from `ntdll.dll`.
  - Paths and command lines come from `QueryFullProcessImageNameW` and `NtQueryInformationProcess(ProcessCommandLineInformation)`, which need neither `PROCESS_VM_READ` nor reading the PEB.
  - The owner and elevation come from `GetTokenInformation`.
  - The phase uses no Tool Help, PDH or WMI.
- **Buffer safety:**
  - The resize loop stops after five calls and at 64 MiB.
  - The parser checks every `NextEntryOffset` and image name against the buffer, copies entries with `memcpy`, and has a test with an unaligned buffer. `static_assert`s tie its full layout to `winternl.h`.
  - The command-line reader checks that the returned `UNICODE_STRING` lies inside its own buffer.
- **Identity everywhere:**
  - The details cache, the rate baselines, the model's rows and the terminator are all keyed by `(PID, creation time)`.
  - The details reader and the terminator compare `GetProcessTimes` with the identity after opening the process.
  - The terminator checks again on its second handle, which it opens while the first one still keeps the PID from being reused.
- **Termination checks:** every branch was read against DC-87 and DC-90.
  - PID 0 and the never-opened names are refused before any `OpenProcess` call.
  - When `TerminateProcess` fails with `ERROR_ACCESS_DENIED` because the process has exited, the zero-timeout wait reports `AlreadyExited`.
  - The owner check comes before the elevation check, so another user's process keeps its more specific message.
  - The dialog acts on a copy of the process taken when it opened. Cancel is both the default and the escape button, and the dialogs show names as plain text.
- **Real-data checks:**
  - **Counts:** the list had 399 processes, the same count as `GetPerformanceInfo` at the same moment.
  - **Details:** 249 processes were read, 146 denied access, 3 were never opened (`lsass.exe` and two `csrss.exe`) and 1 was unavailable (the Idle process). No process had an empty name.
  - **CPU:** per-process CPU over a 2-second interval added up to 98.7% including the Idle process (73.8%), which confirms the units. F-33 explains the remainder.
  - **Exited processes:** a probe kept a handle to an exited child open, and the child was no longer listed, so the table cannot show "zombie" processes as running.
- **Copy costs:**
  - The real list carries 160 KB of strings, and its longest command line has 2,797 characters.
  - In a Release build, copying the list, or a snapshot that holds it, takes 0.11 ms. The pipeline copies it twice per tick, so DC-77's choice to copy rather than share the list is sound.
  - Applying a repeat of the real list to the model takes 0.034 ms, the same as with the benchmark's data, which has no strings.
- **Model and proxy consistency:**
  - Besides the committed churn test, a probe ran 400 refreshes in tree mode with a filter, a CPU sort and periodic toggles. After every update it compared the proxy with an independently filtered tree, and the row counts, parents and order all matched.
  - Recursive filtering hides an ancestor when its only match moves to the top level or exits.
  - The deferred sort turns dynamic sorting back on afterwards.
- **Threading:**
  - Slow collectors run outside `m_collectorMutex` and `m_slowResultsMutex`. Both threads take the two mutexes in the same order, and the slow thread holds the results mutex only to move a finished result in.
  - `stop()` requests both stops and notifies the shared condition variable. `Stop_LongSlowInterval_RespondsImmediately` covers a 60-second slow interval.
- **Process table cost:**
  - At 400 processes, a refresh costs 0.08–0.32 ms in every view with the tab hidden. Repainting the shown tab adds about 6–7 ms.
  - The CPU-sorted tree of 2,000 processes costs 1.2 ms per refresh and 2.2 ms per repeat tick, which now includes DC-84's removals. Together that is 3.4 ms, against DC-84's 2.8 ms for the pair, the same drift as in F-33.
- **Qt ownership and lifetime:** each model node has a single owner. Every dialog and message box has a parent and deletes itself when it closes. The icons are model members rather than statics, so no pixmap outlives the `QGuiApplication`.
- **Logging:**
  - Routine details failures are logged at `debug`.
  - Termination outcomes are logged at `info`, `warn` and `error`, as the architecture's logging table says.
  - Command lines are never logged.
- **Build scripts and dependencies:** the configure retry runs only when a cache existed and only once, and the configure stamp is written only after a success. `qt_configuration_test.cpp` fails if the `qtbase[png]` feature is ever dropped.
- **Known deviations:** D-1 and D-2 are resolved, and their comments are gone from the code. Phase 4 adds no new deviation.
- **Rule files:** `AGENTS.md`, `GEMINI.md`, `copilot-instructions.md`, `platform/windows/AGENTS.md` and `architecture.md` state the termination exception to the access-rights rule the same way, and `ui/AGENTS.md` now has the rule for injected actions.
