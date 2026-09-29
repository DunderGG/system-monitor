# Code Review — Phase 3 Completion

**Date:** 2026-09-29
**Scope:** Everything added or changed in Phase 3 (commits `e7ca94a`..`8d8c383`): the extended `SparklineWidget` (grid, multiple series, labels, annotations, dense-chart painting), the Performance tab (`PerformanceView`, `PerformancePage` and its four subclasses, `ReadoutGrid`, `performance_formatting`, `performance_readouts`), the new data behind the readouts (`CpuSample::baseSpeedMhz`, the `MemorySample` cache and pool fields, `SystemActivityCollector`, `performance_info_reader`), the Release preset, the build-script changes, their tests, and the related documentation. Phase 1 and 2 files that Phase 3 touched were checked again.
**Reviewed against:** [architecture.md](../architecture.md), [coding_guidelines.md](../coding_guidelines.md), [design_decisions.md](../design_decisions.md), [known_deviations.md](../known_deviations.md), [roadmap.md](../../roadmap.md), [AGENTS.md](../../AGENTS.md) and the module `AGENTS.md` files
**Previous reviews:** [Phase 1](phase_1.md) (F-1 to F-8), [Phase 2](phase_2.md) (F-9 to F-19). Finding IDs continue from there.

---

## Overall Assessment

The code in Phase 3 follows the rules well. Module boundaries hold, there are no banned constructs, formatting is enforced and clean, and every widget is owned through Qt parenting. The design is sound: pages keep recording while hidden, missing samples are drawn as gaps rather than zeros, and the pure builders keep the display logic testable. The dense-chart work in `72b8e3a` was the right response to a measured problem. The build has no warnings and all 350 tests pass.

The problems in this phase show up on screen more than in the code. None of the 117 new test cases looks at the pages the way a user sees them, and this review found three **medium-severity** defects that appear on an ordinary Windows machine at the default window size:

- The network page is wider than the window, so part of every adapter's readouts is hidden behind a horizontal scroll bar (F-20).
- The network page draws full-size charts for hidden WAN Miniport adapters. On the review machine, three of its five charts can never show traffic (F-21).
- The "Cached" memory readout differs from Task Manager's "Cached" by 3.8 GiB, and the domain comment that describes it is wrong (F-22).

F-21 has the same cause as F-9 in the Phase 2 review: the fake adapters in the unit tests cannot represent the interfaces Windows actually reports. The other findings are low or cosmetic, except for one maintainability issue: the performance measurement behind the roadmap's "smooth 1 Hz" item cannot be reproduced, because its benchmark was not committed (F-25).

The scores below are lower than in earlier reviews. That reflects these findings, and also that this review rendered the pages from real data and re-measured the performance instead of relying only on the code and the unit tests.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | F-4 was implemented in Phase 3, but its status was never updated (F-27) |
| **Phase 2** — Real Windows collectors & dashboard | ✅ Complete | All Phase 2 findings are resolved |
| **Phase 3** — Sparkline charts & performance views | ✅ Complete | This review was the final unchecked item. F-20 to F-22 should be fixed before Phase 4 |
| **Phase 4–6** — Processes, network view, settings | ⬜ Not started | D-1 and D-2 are planned for Phase 4. F-23 affects the Phase 5 network view |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐ | Boundaries hold. The UI keys adapters by display name because `NetworkSample` drops the LUID the collector already uses (F-23) |
| Naming conventions | ⭐⭐⭐⭐½ | Consistent; one test name has no scenario part (F-26) |
| Formatting | ⭐⭐⭐⭐⭐ | `-CheckFormat` passes on all 108 files; clang-format is enforced in CI |
| C++20 usage | ⭐⭐⭐⭐⭐ | Ranges with projections, designated initializers, `std::span` windows over the ring buffer, `std::erase_if` |
| Error handling | ⭐⭐⭐⭐½ | Gaps instead of zeros throughout, and "N/A" versus "--" is handled carefully. One readout reports a figure other than the one it names (F-22) |
| Memory management | ⭐⭐⭐⭐⭐ | Qt parenting and `deleteLater()` on rebuilds; the `QLayoutItem` returned by `replaceWidget()` is owned by a `std::unique_ptr` |
| Threading | ⭐⭐⭐⭐½ | No new threads; everything new runs on the UI thread or as a fast collector. D-1 and D-2 remain open as planned |
| Testing | ⭐⭐⭐½ | Thorough unit tests of the pure logic, but no test covers the layout at a real window size (F-20). Fake adapters cannot model hidden adapters (F-21) or duplicate names (F-23), a formatting boundary is missed (F-26), and there is no committed benchmark (F-25) |
| UI and presentation | ⭐⭐⭐ | Clipped network page (F-20), charts of hidden adapters (F-21), plot area shifting sideways as axis labels change (F-24), inconsistent annotations (F-26) |
| Build system | ⭐⭐⭐⭐½ | The Release preset and `-Release` switch work; there is no benchmark target (F-25) |
| Documentation | ⭐⭐⭐½ | The design decisions are thorough, but the architecture's chart-cost figures are out of date, F-4 is still marked deferred, and the `cachedBytes` comment is wrong (F-22, F-27) |

### Verification performed

- `.\scripts\build.ps1 -NoRun -Test`: 350/350 tests passed (unit, platform and integration). A forced full recompile (63 translation units at `/W4`) produced no warnings.
- `.\scripts\build.ps1 -CheckFormat`: all 108 files match `.clang-format`.
- **Rendered the pages from real data.** A throwaway integration test (removed afterwards) fed snapshots from the real collectors into `MainWindow` at its default 800×600 and saved each Performance page as an image. It also printed the minimum sizes that Qt's layouts compute (F-20, F-21, F-24).
- **Checked the memory figures against the OS.** Compared `GetPerformanceInfo`'s `SystemCache` with the standby, modified and cache counters, read through `Get-Counter` for comparison only (F-22).
- **Listed interfaces** with `Get-NetAdapter -IncludeHidden`, `Get-NetIPInterface` and `ipconfig /all` (F-21, F-23).
- **Re-measured the tick cost** in a Release build with a throwaway benchmark. The recorded conclusions hold (F-25).
- **Probed edge cases:** duplicate adapter names in `NetworkPerformancePage` (F-23), plot-area position against axis-label width (F-24), and `formatLinkSpeed` at unit boundaries (F-26).
- **Searched for banned constructs:** exceptions, `new`/`delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, `qDebug`/`std::cout`/`printf`, `using namespace std`. Also searched for forbidden includes across module boundaries. There were no hits.

---

## Findings

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [F-20](#f-20-performance-pages-do-not-fit-the-default-window) | Performance pages do not fit the default window | Medium | Open |
| [F-21](#f-21-network-page-charts-hidden-wan-miniport-adapters) | Network page charts hidden WAN Miniport adapters | Medium | Open |
| [F-22](#f-22-cached-readout-is-not-the-figure-its-name-and-comment-promise) | "Cached" readout is not the figure its name and comment promise | Medium | Open |
| [F-23](#f-23-adapter-histories-are-keyed-by-display-name) | Adapter histories are keyed by display name | Low | Open |
| [F-24](#f-24-plot-area-moves-when-the-y-axis-label-width-changes) | Plot area moves when the Y-axis label width changes | Low | Open |
| [F-25](#f-25-the-1-hz-performance-verification-cannot-be-reproduced) | The 1 Hz performance verification cannot be reproduced | Low | Open |
| [F-26](#f-26-presentation-and-formatting-items) | Presentation and formatting items | Low | Open |
| [F-27](#f-27-documentation-out-of-date) | Documentation out of date | Low | Open |
| [F-28](#f-28-duplicated-constants-and-helpers) | Duplicated constants and helpers | Cosmetic | Open |

**Open findings from earlier reviews:** none. [F-4](phase_1.md#f-4-mainwindowonsnapshotready-only-updates-dashboardview) was implemented in `d12d097`, but its status still says "Deferred to Phase 3" (see F-27).

---

### F-20: Performance pages do not fit the default window

**Severity:** Medium
**Files:** `src/ui/network_performance_page.cpp` — `rebuildAdapterCharts()`; `src/ui/readout_grid.cpp`; `src/ui/cpu_performance_page.cpp` — `rebuildCoreCharts()`; `src/ui/main_window.cpp`

**Description:**

`MainWindow` opens at 800×600. After the 200-pixel sidebar and the margins, a page has about 535 pixels of width. The network page puts each adapter's ten readouts in a five-column `ReadoutGrid`. A `QLabel` without word wrap cannot be narrower than its text, and the page holds values such as full IPv6 addresses (`fd44:555c:2315:4176:c6d0:77e0:5d11:3727`), adapter descriptions and DNS server lists. Rendered from real data on the review machine:

```text
network scroll area: content minimum width 875 px, viewport 535 px
                     horizontal scroll bar visible, 340 px hidden
```

At the default size, the whole Type column and the byte totals ("Received", "Sent") are cut off, and so are the chart's "now" label and its Min/Max annotation, because the chart is laid out at the content width. A user has to scroll sideways for every adapter. No test catches this: the page tests never give the view a real size, and `ShownPerformanceView` uses `WA_DontShowOnScreen` without resizing.

The CPU page has a related problem at high core counts. The per-core grid is not scrollable, and each chart has a minimum size, so the grid raises the window's minimum size:

| Logical processors | Window minimum size |
| --- | --- |
| 16 | 600×498 |
| 64 | 600×534 |
| 128 | 772×594 |
| 256 | 948×694 |

On a 256-thread workstation, the window can no longer be made smaller than 948×694 once the first snapshot arrives. The benchmark in `design_decisions.md` used a 1280×800 window, so it could not show this.

**Proposed Solution:**

1. Make the readouts fit the available width instead of forcing their own:
   - Let `ReadoutGrid` choose its column count from its width. It can re-flow the existing labels in `resizeEvent()`, with the constructor argument as the maximum, which keeps the "rebuild only when names change" rule.
   - Or give long values their own full-width rows: the adapter description, IPv6 addresses and DNS servers in a one- or two-column block, with the short values in the five-column grid.
2. Set `m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)` so the charts always fit the viewport, and let the readouts wrap vertically.
3. Put the per-core grid in a `QScrollArea`, or lower the per-core chart's minimum height above a core-count threshold, so the window's minimum size does not grow with the core count.
4. Consider a larger default window size (for example 1024×720). This does not replace items 1 to 3, because users can still shrink the window.
5. Add a widget test that shows `MainWindow` at its default size with a realistic snapshot (long IPv6 addresses and descriptions, 256 cores) and checks that the network scroll area has no horizontal range and that `minimumSizeHint()` stays within 800×600.

**Recommendation:** Fix before Phase 4. It affects every user at the default size, and the Phase 4 process table will face the same width limits.

**Status:** Open.

---

### F-21: Network page charts hidden WAN Miniport adapters

**Severity:** Medium
**Files:** `src/ui/performance_formatting.cpp` — `chartedAdapters()`; `tests/unit/performance_formatting_test.cpp`, `tests/unit/performance_view_test.cpp`

**Description:**

`chartedAdapters()` charts every adapter whose status is Up. After F-9, the collector removes loopback and NDIS filter rows but keeps virtual adapters, so that the Phase 5 network view can list them. Windows reports several hidden virtual adapters as Up permanently. On the review machine the network page charts five adapters:

| Chart title | Adapter | Hardware | Hidden | IP interface | Traffic |
| --- | --- | --- | --- | --- | --- |
| Ethernet | Realtek Gaming 2.5GbE | Yes | No | Yes | Yes |
| Local Area Connection* 7 | WAN Miniport (IP) | No | Yes | No | Never |
| Local Area Connection* 8 | WAN Miniport (IPv6) | No | Yes | No | Never |
| Local Area Connection* 9 | WAN Miniport (Network Monitor) | No | Yes | No | Never |
| Mullvad | Mullvad Tunnel (VPN) | No | No | Yes | Yes |

The three WAN Miniports are hidden from `Get-NetAdapter`, the Network Connections folder and Task Manager. They have no IP interface, do not appear in `ipconfig /all`, and never carry traffic. Each still gets a full-size chart with a "1 B/s" axis and a row of readouts, so most of the network page is empty charts, and the useful ones are further down the scroll area. WAN Miniports are present on most Windows installations, so this is not specific to the review machine.

This is the same gap as F-9: a fake `NetworkSample` in the tests is either hardware or virtual, and nothing models "a virtual interface Windows keeps Up but hides".

**Proposed Solution:**

`MIB_IF_ROW2` has no "hidden" flag, and `ConnectorPresent` is false for both the WAN Miniports and the VPN tunnel, so it cannot tell them apart. What does tell them apart is IP configuration. The adapters worth charting are bound to IP, and the collector already fills `ipAddresses` from `GetAdaptersAddresses`:

```cpp
// A virtual adapter is charted only when it is bound to IP. Hidden
// adapters such as the WAN Miniports are Up but have no addresses and
// never carry traffic.
bool isChartable(const domain::NetworkSample& adapter)
{
    return isUp(adapter) && (adapter.isHardwareInterface || !adapter.ipAddresses.empty());
}
```

If `GetAdaptersAddresses` fails, virtual adapters drop out until the next successful refresh while hardware adapters stay. That is acceptable for a chart list. If the Phase 5 network view needs the same distinction, carry an explicit `hasIpInterface` flag in `NetworkSample` instead, set when the adapter was found in the details table.

Add unit tests with a WAN-Miniport-shaped adapter (virtual, Up, no addresses) and a VPN-shaped adapter (virtual, Up, one address). Update the "Performance view" design decision, which says the page charts "one chart per adapter that is Up".

**Recommendation:** Fix before Phase 4, together with F-20, since both change what the network page lays out.

**Status:** Open.

---

### F-22: "Cached" readout is not the figure its name and comment promise

**Severity:** Medium
**Files:** `src/domain/memory_sample.h` — `cachedBytes` comment; `src/platform/windows/memory_collector.cpp` — `calculateMemorySample()`; `src/ui/performance_readouts.cpp` — `memoryReadouts()`; `docs/design_decisions.md` — "Memory detail and system activity counts…"

**Description:**

The memory page labels `PERFORMANCE_INFORMATION::SystemCache` as "Cached". The domain comment describes it as "the standby list plus the system working set… which Task Manager shows as 'Cached'". Neither statement matches the review machine. Readings taken at the same moment:

| Source | Value |
| --- | --- |
| `GetPerformanceInfo` `SystemCache` (our "Cached") | **30.59 GiB** |
| Standby list (reserve + normal + core) | 33.83 GiB |
| Modified page list | 0.54 GiB |
| Standby + modified (Task Manager's "Cached") | **34.37 GiB** |
| System cache working set (`\Memory\Cache Bytes`) | 1.26 GiB |

The readout is 3.8 GiB (11%) below Task Manager's figure, and even below the standby list on its own, so it cannot be "standby plus system working set" either. A user who compares the two tools will see numbers that disagree, with nothing to explain why.

"In use" has a smaller, related difference. It is computed as total minus available, which counts the modified list as in use. Task Manager divides memory into In use, Modified, Standby and Free, so its "In use" leaves the modified list out (0.54 GiB here).

**Proposed Solution:**

The exact standby and modified sizes come from `NtQuerySystemInformation(SystemMemoryListInformation)`. It requires `SeProfileSingleProcessPrivilege`, which unelevated users do not have, and PDH is ruled out for core metrics. So the practical fix is to report accurately what we do have:

1. Rename the readout to "System cache" (or drop it), and rewrite the `cachedBytes` comment to describe exactly what `SystemCache` is, without claiming Task Manager equivalence. Rename the field too if the new label no longer fits.
2. Rename "In use" to "Used", or add a note in the design decision that it includes modified pages. Do the same for the dashboard's and sidebar's memory text, which use the same total-minus-available calculation (see F-28).
3. Correct the design decision, and record why Task Manager's figures cannot be reproduced unelevated.
4. If an elevated mode ever exists (see the elevated termination helper in the post-MVP roadmap), `SystemMemoryListInformation` can provide the Task Manager figures then.

**Recommendation:** Fix before Phase 4. It is a small change, and the readout currently states something that is not true.

**Status:** Open.

---

### F-23: Adapter histories are keyed by display name

**Severity:** Low
**Files:** `src/domain/network_sample.h`; `src/platform/windows/network_collector.cpp` — `queryWindowsAdapters()`, `calculateNetworkSamples()`; `src/ui/network_performance_page.cpp` — `recordSamples()`, `refresh()`

**Description:**

The collector identifies an interface by its LUID, and uses it as the key for rate baselines. The LUID does not reach the domain. `NetworkSample::adapterName` is set to the friendly name, or the alias, or the description when both are empty, and `NetworkPerformancePage` keys its histories and charts by that string. This has two effects:

- **Duplicate names break the history.** A probe fed two Up adapters with the same `adapterName` into the page. It produced two charts drawing the same history, and that history held **2 samples after 1 tick**, because each adapter pushed into it. The chart then runs at twice the real time scale, and each chart shows a mixture of the two adapters. Aliases are unique on Windows, but descriptions are not ("WAN Miniport…", "Microsoft Wi-Fi Direct Virtual Adapter"), and the description is used whenever the alias is empty. No test has two adapters with the same name.
- **Renaming restarts the history.** Renaming a connection in Network Connections changes its key, so its chart disappears and a new one starts empty.

**Proposed Solution:**

Add a stable identifier to the domain type, for example `uint64_t interfaceId` holding the LUID. The collector already has this value as `key`. Key `AdapterHistory` and `m_chartedNames` by it, and keep `adapterName` for display only. The Phase 5 network view needs a stable adapter identity anyway. Add a page test with two adapters that share a name.

**Recommendation:** Fix before Phase 5, or together with F-21 if the charted-adapter logic is being changed anyway.

**Status:** Open.

---

### F-24: Plot area moves when the Y-axis label width changes

**Severity:** Low
**File:** `src/ui/charts/sparkline_widget.cpp` — `plotArea()`

**Description:**

`plotArea()` sizes the left gutter to the wider of the formatted range minimum and maximum. On an auto-ranged chart, the maximum label changes as traffic changes, and the whole plot moves sideways with it: the lines, the fill and the scrolling grid. Measured on a 600-pixel chart with `formatChartByteRate`:

| Largest sample | Max label | Plot left edge |
| --- | --- | --- |
| 900 B/s | 990 B/s | 44.8 px |
| 1 000 B/s | 1.1 KiB/s | 50.8 px |
| 50 000 B/s | 53.7 KiB/s | 57.3 px |
| 900 000 B/s | 966.8 KiB/s | 63.7 px |
| 12 000 000 B/s | 12.6 MiB/s | 61.1 px |

A burst of traffic makes the chart jump by up to 19 pixels and back, which reads as jitter on a chart that is otherwise meant to scroll smoothly. For the same reason, the charts stacked on the network page do not line up: an idle adapter's "1 B/s" gutter is narrower than a busy one's "97.9 KiB/s" gutter.

**Proposed Solution:**

Give the gutter a width that does not depend on the current values. For example, let the caller set a gutter template (`setAxisLabelTemplate("888.8 MiB/s")`) that is measured once per font change. The formatter could also provide its widest possible output. Percentage charts would then keep their gutter, and all byte-rate charts would share one. Add a test that `plotArea().left()` does not change when the auto range changes.

**Recommendation:** Fix when convenient. Doing it together with F-20 avoids adjusting the network page's layout twice.

**Status:** Open.

---

### F-25: The 1 Hz performance verification cannot be reproduced

**Severity:** Low
**Files:** `docs/design_decisions.md` — "30 minutes of performance history…", "Dense sparklines…"; `roadmap.md` — "Verify smooth 1 Hz updates…"; `scripts/build.ps1`

**Description:**

The roadmap item "Verify smooth 1 Hz updates with 5–30 minutes of history" is checked on the strength of a table in `design_decisions.md`, measured with "a throwaway benchmark (not committed)". That benchmark found the 210 ms per tick that led to the dense-chart rework, so it has already caught one serious regression. Without it:

- Nobody can reproduce the table or check it on another machine. The Phase 6 item "Profile … on a lower-end system" will have to rebuild it.
- A later change to `SparklineWidget`, to the pages, or to how often they refresh can bring back a 100 ms+ tick without any test noticing. The unit tests only check that something is painted.

This review rebuilt a comparable benchmark: a Release build, 1800 worst-case snapshots, then 20 timed ticks, each forwarded by `MainWindow` and followed by a `grab()` of the whole 1280×800 window:

| Page | 1 min | 5 min | 30 min | Recorded, 30 min |
| --- | --- | --- | --- | --- |
| CPU, 16 cores | 7.5 ms | 12.2 ms | 38.4 ms | 26.9 ms |
| CPU, 64 cores | 12.4 ms | 18.0 ms | 31.9 ms | 20.4 ms |
| Memory | 3.7 ms | 8.3 ms | 18.5 ms | 8.2 ms |
| Disk | 3.2 ms | 8.4 ms | 33.2 ms | 1.6 ms |
| Network, 2 adapters | 5.4 ms | 11.2 ms | 19.1 ms | 36.6 ms |

The recorded conclusions hold: under 13 ms at the default window, and under 40 ms in the worst case at 30 minutes. The individual numbers differ by up to 2×, and more for disk, because the synthetic data and the painting method differ. This review's data swings disk usage every 30 samples, which real disk space never does. Differences this large are a reason to commit one agreed benchmark.

**Proposed Solution:**

Commit the benchmark as an opt-in executable, for example `benchmarks/performance_tick_benchmark.cpp` building `system_monitor_benchmark`. Keep it out of `ctest` and out of CI's pass/fail. Add a `build.ps1 -Benchmark` switch that implies `-Release`, per the rule to extend the build script rather than bypass it. Update its comment-based help, `build_system.md` and the agent files. Point the design-decision table at the benchmark and record the data shape it uses.

**Recommendation:** Fix before Phase 4. The process table is the next large UI-thread cost, and it should be measured with the same tool.

**Status:** Open.

---

### F-26: Presentation and formatting items

**Severity:** Low
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| `formatLinkSpeed` prints scientific notation at unit boundaries: 999,950,000 bps → "1e+03 Mbps", 999,999 bps → "1e+03 Kbps", 1 Tbps → "1e+03 Gbps". `QString::number(…, 'g', 3)` switches to an exponent once rounding reaches 1000 | `performance_readouts.cpp` — `formatLinkSpeed()` | Pick the unit after rounding, or format with `'f'` and trim trailing zeros. Add the boundary cases to `FormatLinkSpeed_DecimalUnits` |
| The chart annotation and the readout under it show the same value at different precision: "8%" above the CPU chart, "7.5%" in the Utilization readout | `performance_formatting.cpp` — `formatChartPercent()` is used for the current value as well as the axis | Keep whole percentages for the axis and min/max, but show the current value with `formatPercent()`, or use one precision for both |
| On a multi-series chart, "Min … Max …" describes series 0 only and does not say so: Receive on the network charts, the first volume on the disk chart | `sparkline_widget.cpp` — `paintAnnotations()` | Prefix the primary series' label when there is a legend ("Receive min … max …"), or show extremes only on single-series charts |
| Disk series colours follow sort position, so adding a volume changes the colour of every volume after it | `disk_performance_page.cpp` — `refresh()` | Assign a volume its colour when it is first seen and keep it (`setSeriesColor()`) |
| Test name without a scenario part | `sparkline_widget_test.cpp` — `SparklineEnvelope.KeepsHighestPointPerPixelColumn` | e.g. `ZigzagWithinColumns_KeepsHighestPointPerColumn` |

**Recommendation:** Fix when convenient. The link-speed item is a small fix with a test, and can be done at any time.

**Status:** Open.

---

### F-27: Documentation out of date

**Severity:** Low
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| The chart cost figures contradict the measurements: "<0.05 ms per frame", "typically 60–300 points", "each repaint takes well under 1 ms". Phase 3 records 1.3–36.6 ms per tick with up to 1800 points per series | `architecture.md` — technology table (Charts) and "Sparkline charts" section | State the measured cost and point to the design decision (and to the benchmark from F-25). The same kind of correction was made to the network timing in F-10 |
| F-4 still says "Deferred to Phase 3", although `d12d097` implemented snapshot forwarding and the hidden-view rule | `code_reviews/phase_1.md` — F-4 status | Mark it Resolved (2026-09-29, `d12d097`) and link the "Snapshot forwarding" design decision |
| The `cachedBytes` comment describes a different figure | `domain/memory_sample.h` | See F-22 |
| The Performance view decision says the network page has "one chart per adapter that is Up" | `design_decisions.md` — "Performance view…" | Update together with F-21 |

**Recommendation:** Fix together with F-21 and F-22, which change the same entries.

**Status:** Open.

---

### F-28: Duplicated constants and helpers

**Severity:** Cosmetic
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| `kPercentRange{0, 100}` defined four times | `dashboard_view.cpp:21`, `cpu_performance_page.cpp:24`, `disk_performance_page.cpp:18`, `memory_performance_page.cpp:16` | One constant, e.g. `charts::kPercentRange` next to `YRange` |
| `"N/A"` defined as `kNotAvailable` in three `ui` files, plus a literal | `dashboard_view.cpp:22`, `performance_formatting.cpp:15`, `performance_readouts.cpp:18`; literal in `network_performance_page.cpp:119` | One shared `ui` constant. The copy in `charts` can stay, because the charts library does not depend on `ui` |
| Used memory (total minus available, clamped) computed three times | `dashboard_view.cpp:130`, `performance_formatting.cpp:104`, `performance_readouts.cpp:134` | One `usedMemoryBytes(const MemorySample&)` helper; this also gives F-22's "In use" change a single place |
| `countText`, `bytesText`, `rateText` exist only to pass a function to `valueOr` | `performance_readouts.cpp:22-35` | Pass lambdas at the call sites, or let `valueOr` take any callable and drop the wrappers |
| Layout numbers written as literals next to named constants in the same files: spacing 8 and 12, margins 16, heading size 16, chart minimum height 120 | `performance_page.cpp`, `performance_view.cpp`, `network_performance_page.cpp` | Name them, and share page margins and spacing with `DashboardView` |
| `chartedAdapters()` sorts with `QString::localeAwareCompare`, so this pure function's result depends on the machine's locale. The readouts decision avoided exactly that for counts | `performance_formatting.cpp` — `chartedAdapters()` | `QString::compare(…, Qt::CaseInsensitive)` |

**Recommendation:** Fix when convenient, ideally in the same commits that touch these files for F-20 to F-22.

**Status:** Open.

---

## Items Verified — No Issues Found

- **Module boundaries:** `ui` and `ui/charts` include only Qt, the standard library, `domain` and `ui` headers. `charts` does not depend on `ui`, since value formatting is injected through `ValueFormatter`. The new platform code keeps SDK types inside its translation units: `PerformanceInfoData` and `baseSpeedFromMaxMhz` expose none.
- **API choices:** `GetPerformanceInfo` for pool sizes and process, thread and handle counts, and `CallNtPowerInformation(ProcessorInformation)` for the rated clock speed, read once. Both are documented Win32 calls; there is no PDH, WMI or Tool Help. The buffer for `CallNtPowerInformation` is sized from `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)`. The locally declared `PROCESSOR_POWER_INFORMATION` matches the documented layout. The reasons for leaving out a current-speed readout are sound and recorded.
- **No zero placeholders:** a missing CPU, memory, disk or network value is recorded as a gap and drawn as one. A failed network read keeps the charts and shows "N/A" readouts. The cache and pool fields are `std::nullopt` when only `GetPerformanceInfo` fails, and a zero page size is rejected. `calculateSystemActivity()` rejects a process count of 0.
- **Recording is separate from display:** every page records every snapshot whether it is shown or not, which fixes the history gaps F-4 would otherwise have caused. `refresh()` runs only for the visible page, and again on `showEvent` and page changes. A hidden page does no widget work.
- **History bookkeeping:** the per-core history restarts when the core count changes. An empty per-core list records gaps rather than rebuilding the grid. Disk volumes and network adapters that leave the snapshots record gaps and are dropped once their histories are empty. `showHistory()` passes only the visible window to the chart, so auto range and min/max follow what is on screen.
- **Sparkline algorithms:** `verticalGridLines()` uses modular arithmetic that cannot underflow while the history is short. The min/max decimation keeps spikes and splits segments at gaps. `upperEnvelope()` keeps each column's highest point. `summarizeSeries()` ignores gaps and marks the newest point on ties. All are pure and unit tested.
- **Qt ownership:** every widget has a parent. Rebuilt containers (per-core grid, adapter list, readout cells) are detached and released with `deleteLater()` or by `QScrollArea::setWidget()`. The `QLayoutItem` returned by `replaceWidget()` is owned by a `std::unique_ptr`, correctly, since it is not a `QObject`.
- **Theme handling:** series colours switch to their dark steps from the widget's own background at paint time. Grid, frame and secondary text use the palette's text colour at reduced opacity. Status hues are kept out of the series palette. The screenshots taken for this review, in the dark theme, render correctly.
- **Scheduler integration:** `SystemActivityCollector` is registered like the other fast collectors, runs outside `m_collectorMutex`, and has a scheduler test. Its `GetPerformanceInfo` call (about 19 µs) fits the fast-collector budget.
- **Build scripts:** `-Release` selects the configure, build and test presets and the launch path consistently. `-Format` and `-CheckFormat` now cover untracked files. The `vswhere` early-exit fix is in both scripts.
- **Known deviations:** there are no new deviations. D-1 and D-2 remain open as planned for Phase 4.
- **Design decisions:** every non-obvious Phase 3 choice has a Decision / Rationale entry, including measurements for the performance-driven ones. F-22, F-25 and F-27 cover the entries that are inaccurate or cannot be reproduced.
