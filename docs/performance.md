# Performance

What one 1 Hz tick costs the UI thread on each tab, as `.\scripts\build.ps1 -Benchmark` measures it in a Release build ([DC-66](design_decisions.md#dc-66)). The figures depend on the machine and its state as much as on the code, so a run is compared only with runs that printed the same environment lines ([DC-95](design_decisions.md#dc-95)), and best with runs made the same day.

## Recording a run

1. Close other programs that use the processor, and run `.\scripts\build.ps1 -Benchmark` three times in a row.
2. Record the range of the means over the three runs, the longest single tick, and the three environment lines the run prints.
3. When a change alters a tab's cost, or the environment changes, replace that tab's baseline below. Add a line to the history: the date, what changed, and the figures that show it.

Design decisions quote at most the figure that justifies a choice, and link here ([DC-117](design_decisions.md#dc-117)).

## What is measured

Each benchmark shows its tab in a 1280×800 window off screen. The data is synthetic, fixed and a worst case, so two runs measure the same work.

- **Performance tab** (`benchmarks/performance_tick_benchmark.cpp`): for each page, `MainWindow` records 1800 snapshots while the tab is hidden, then the page is selected and, for each chart window, 20 ticks are timed: `MainWindow::onSnapshotReady()` followed by `grab()` of the whole window. The pages are CPU with 16 and with 64 cores, memory, disk with three volumes, and network with two adapters. Every series is a sine wave between 5% and 95% of its range, with a period of 20 to 30 samples that differs per series. Throughput peaks at 100 MiB/s.
- **Network tab** (the same file): the same tick with the Network tab shown, one minute of history per adapter chart. The adapters are Up and listed by default: 2, 8 and 16 of them, then 20 with "Show all adapters", 10 of them Down and hidden, then 16 with one going down and up on alternate ticks.
- **Processes tab** (`benchmarks/process_table_benchmark.cpp`): `ProcessesView` applying a snapshot's process list, and, with the tab shown, a repaint of the view. It runs 400, 1,000 and 2,000 simulated processes in five views: flat by name, flat by CPU, tree by name, tree by CPU, and tree by CPU filtered to "svchost". After 10 ticks of warm-up it times 60 ticks that alternate between a new list and a repeat of it, as the scheduler sends them: the list refreshes every 2 seconds and a snapshot comes every second. Each new list changes the values of 40% of the processes, and 2 processes exit and 2 start.

## Baseline

Measured on 2026-10-18, on the development machine, with three runs one after another:

```text
Processor: AMD Ryzen 7 9800X3D 8-Core Processor
Power plan: High performance; power mode: Best performance, on mains power
Built with MSVC 19.51.36260 (Release), running Qt 6.11.1 on Windows 11 Version 25H2 (10.0.26200), 16 logical processors.
```

Most figures rose a little from each run to the next. Every run since 2026-10-16 has been slower than those of 2026-10-15 with the same environment lines, with or without the windows11 style, which accounts for about 1 ms on the network views only (see the history). Compare a new run with this baseline rather than with the history.

### Performance tab

Range of the mean ms per tick over the three runs:

| Page | 1 min | 5 min | 30 min |
| --- | --- | --- | --- |
| CPU, 16 cores | 8.8–10.3 | 14.5–15.8 | 43.4–44.9 |
| CPU, 64 cores | 16.9–18.8 | 21.1–22.4 | 38.0–41.3 |
| Memory | 4.9–6.1 | 11.1–12.0 | 26.0–27.4 |
| Disk, 3 volumes | 5.7–6.9 | 27.3–30.9 | 42.2–46.1 |
| Network, 2 adapters | 6.6–7.3 | 14.4–15.3 | 16.9–18.5 |

The longest single tick was 63.0 ms (disk, 30 minutes). At 30 minutes the 64-core CPU page costs less than the 16-core one, because its per-core charts are smaller and a dense chart's cost is bounded by its width ([DC-48](design_decisions.md#dc-48)). The disk page is slow at 5 and 30 minutes because all three volumes swing on every sample, which real disk usage never does.

### Network tab

| Scenario | Mean ms per tick | Longest tick |
| --- | --- | --- |
| 2 adapters | 6.0–6.7 | 9.7 |
| 8 adapters | 6.6–7.0 | 8.1 |
| 16 adapters | 6.8–6.9 | 8.4 |
| 20 adapters with "Show all adapters", 10 of them Down and hidden | 6.7–7.4 | 8.9 |
| 16 adapters, one going down and up on alternate ticks | 6.7–8.0 | 9.8 |

Twenty adapters cost about what 8 and 16 do: only the cards in view are painted, and a collapsed chart is not updated ([DC-98](design_decisions.md#dc-98)). An adapter that changes state collapses or expands its chart, which moves the cards below it, and costs a little more.

### Processes tab

2,000 processes, ms per tick for a new list / a repeated list:

| View | Tab hidden | Tab shown |
| --- | --- | --- |
| Flat, by name | 0.25–0.30 / 0.21–0.26 | 6.4–7.1 / 6.5–7.1 |
| Flat, by CPU | 1.20–1.38 / 0.22–0.27 | 7.6–9.8 / 6.6–8.2 |
| Tree, by name | 0.55–0.61 / 1.79–2.15 | 9.1–11.3 / 9.7–12.2 |
| Tree, by CPU | 1.02–1.17 / 1.77–2.02 | 9.5–12.0 / 9.0–11.4 |
| Tree, by CPU, filtered | 1.20–1.45 / 0.99–1.21 | 7.9–10.1 / 7.6–9.5 |

At 400 processes every view costs at most 0.37 ms per tick with the tab hidden, and 5.5–8.0 ms with it shown. The longest single tick was 26.0 ms (tree by name, 2,000 processes, shown). An exited row is removed three seconds after its process exits ([DC-82](design_decisions.md#dc-82)), usually on a tick that repeats the list, so in tree views a repeat costs more than a new list. Compare the two together with earlier runs.

## History

- **2026-09-29, Phase 3, Performance tab:** a benchmark that was not committed measured the Performance tab at 1.3–12.1 ms per tick at the 1-minute window and up to 36.6 ms at 30 minutes, on the development machine. The dense-chart painting of [DC-48](design_decisions.md#dc-48) brought the 30-minute CPU page down from about 210 ms. A Release build of the app used about 41 MiB of working set.
- **2026-10-13, code review F-25:** the committed benchmark ([DC-66](design_decisions.md#dc-66)) measured 4.0–14.3 ms at 1 minute and 13.3–39.1 ms at 30 minutes, with single ticks up to 48.6 ms. Its data swings every series, all three disk volumes included, so its disk page was far slower than the first benchmark's 1.6 ms.
- **2026-10-14, Phase 4, process explorer view:** the Processes tab benchmark. Signalling only the cells that changed, and sorting once per refresh ([DC-79](design_decisions.md#dc-79), [DC-81](design_decisions.md#dc-81)), cut a CPU-sorted tree of 2,000 processes from 27.18 / 1.79 ms per new / repeated list to 2.72 / 0.17 ms with the tab hidden, and from 32.7 to 9.7 ms per new list with it shown. At 400 processes every view then cost under 0.5 ms per new list. Keeping exited rows for three seconds left that tree at 2.8 ms over a new and a repeated list, against 2.9 ms before.
- **2026-10-15, code review F-33:** the review had measured the Performance tab 25–45% above the 2026-10-13 figures, and the Processes tab 15–20% above. With the environment printed ([DC-95](design_decisions.md#dc-95)), three runs were back within 2 ms of the 2026-10-13 table: 3.9–13.0 ms at 1 minute, up to 39.7 ms at 30 minutes, and a longest tick of 53.3 ms. The CPU-sorted tree of 2,000 processes cost 0.92–0.98 / 1.63–1.64 ms with the tab hidden. At 400 processes every view cost at most 0.28 ms hidden and 4.1–4.7 ms shown.
- **2026-10-16, Phase 5, network view:** the Network tab benchmark. With 2, 8 and 16 adapters a tick cost a mean of 4.2, 5.7 and 5.7 ms.
- **2026-10-16, code review F-42:** the windows11 style ([DC-104](design_decisions.md#dc-104)), timed on one Release binary with its `styles` directory present for three runs and renamed for two, alternately. With the style, the Network tab cost 5.9–8.6 ms against 5.6–7.1 ms for 2 to 16 adapters, and the Performance tab's network page at 1 minute 7.2–8.9 ms against 6.1–7.1 ms. On the other pages the ranges overlapped. Every run that day, with or without the style, was slower than on 2026-10-15, with the same environment lines.
- **2026-10-16, code review F-40 and F-41:** the Network tab benchmark added "Show all adapters" and an adapter that changes state. Three runs measured means of 5.8–7.0 ms, with a longest tick of 8.8 ms.
- **2026-10-18, maintenance:** the baseline above, the first with all three tabs measured on the same code and day.
