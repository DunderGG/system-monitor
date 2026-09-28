# Code Review — Phase 2 Completion

**Date:** 2026-09-25
**Scope:** Everything added or changed in Phase 2 (commits `a1a529d`..`e402c0d`): the Windows collectors in `src/platform/windows/`, health evaluation, the dashboard, `ResourceCard`, the minimal `SparklineWidget`, the move of `RingBuffer` to `domain`, their tests, and the related CMake and documentation changes. Phase 1 files that Phase 2 touched were checked again.
**Reviewed against:** [architecture.md](../architecture.md), [coding_guidelines.md](../coding_guidelines.md), [design_decisions.md](../design_decisions.md), [known_deviations.md](../known_deviations.md), [roadmap.md](../../roadmap.md), [AGENTS.md](../../AGENTS.md) and the module `AGENTS.md` files
**Previous review:** [Phase 1](phase_1.md) (findings F-1 to F-8). Finding IDs continue from there.

---

## Overall Assessment

Phase 2 is functionally complete and structurally sound. Module boundaries hold. No Windows header reaches `ui`, `domain` or `monitoring`, and every Windows resource has an RAII owner. The no-zero-placeholder rule was applied carefully to CPU, memory, disk, throughput and connectivity, and each collector has reader injection with deterministic unit tests. The event-driven connectivity collector matches the documented pattern closely. All 204 tests pass.

The review found one **high-severity correctness bug**: the dashboard's network throughput counts the same traffic several times. On the review machine it showed about five times the real rate (F-9). This slipped through because the unit tests' fake adapters cannot represent the Windows interface kinds that cause it. The rest of the findings are medium or lower: collection cost, log volume, empty collections that hide failures, and consistency items.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | F-4 is deferred to Phase 3; all other Phase 1 findings are resolved |
| **Phase 2** — Real Windows collectors & dashboard | ✅ Complete | This review was the final unchecked item; see the findings below |
| **Phase 3** — Sparkline charts & performance views | 🟡 Started | 5 of 8 sparkline items built early for the dashboard; Performance tab not started |
| **Phase 4–6** — Processes, network view, settings | ⬜ Not started | D-1 and D-2 are planned for Phase 4 |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐ | Boundaries hold; `GetAdaptersAddresses` runs every tick (F-10); undeclared platform → monitoring dependency (F-13) |
| Naming conventions | ⭐⭐⭐⭐½ | Consistent apart from the `pfn…` type aliases (F-18) |
| Formatting | ⭐⭐⭐⭐ | Three lines over the 120-column limit; include grouping drift; `.clang-format` is not enforced (F-17, F-18) |
| C++20 usage | ⭐⭐⭐⭐⭐ | Designated initializers, `std::optional`, `std::span`, ranges, `std::erase_if` used throughout |
| Error handling | ⭐⭐⭐⭐ | Optional values used end to end; empty disk and network vectors still hide failures (F-12) |
| Memory management | ⭐⭐⭐⭐⭐ | `FreeMibTable` and `CancelMibChangeNotify2` wrapped in RAII; Qt parenting everywhere in the UI |
| Threading | ⭐⭐⭐⭐½ | The connectivity callback does logging I/O on the OS thread (F-14); D-1 and D-2 remain open as planned |
| Testing | ⭐⭐⭐⭐½ | Fakes, not mocks; correct `ASSERT`/`EXPECT` use; fake adapters cannot model filter or tunnel interfaces (F-9) |
| Build system | ⭐⭐⭐⭐ | Platform tests separated cleanly; hidden include dependency (F-13) |
| Documentation | ⭐⭐⭐⭐⭐ | Every non-obvious choice is in `design_decisions.md`; deviations tracked and resolved promptly |

### Verification performed

- `.\scripts\build.ps1 -NoRun -Test`: build succeeded and 204/204 tests passed (unit, platform and integration).
- Read `GetIfTable2` directly on the review machine to check how the network collector sees real interfaces (F-9).
- Timed `GetAdaptersAddresses` and `GetIfTable2` on the review machine (F-10).
- Searched the code for banned constructs (exceptions, `new`/`delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, `std::thread`, `qDebug`/`std::cout`/`printf`, `using namespace std`, ANSI `…A` APIs, PDH, Tool Help, WMI, legacy `GetIfTable`) and for forbidden includes across module boundaries. The only hits are listed in F-18.

---

## Findings

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [F-9](#f-9-network-throughput-total-counts-the-same-traffic-several-times) | Network throughput total counts the same traffic several times | High | Resolved |
| [F-10](#f-10-getadaptersaddresses-runs-on-every-scheduler-tick) | `GetAdaptersAddresses` runs on every scheduler tick | Medium | Resolved |
| [F-11](#f-11-persistent-failures-are-logged-on-every-tick) | Persistent failures are logged on every tick | Medium | Resolved |
| [F-12](#f-12-empty-disk-and-network-vectors-hide-collector-failures) | Empty disk and network vectors hide collector failures | Low | Resolved |
| [F-13](#f-13-platformwindows-depends-on-monitoring-headers-without-declaring-it) | `platform/windows` depends on `monitoring` headers without declaring it | Low | Resolved |
| [F-14](#f-14-connectivity-callback-writes-to-the-log-on-the-os-thread) | Connectivity callback writes to the log on the OS thread | Low | Resolved |
| [F-15](#f-15-cpu-multi-group-aggregation-keys-on-core-count-not-group-count) | CPU multi-group aggregation keys on core count, not group count | Low | Open |
| [F-16](#f-16-resourcecardsetstatus-restyles-every-tick) | `ResourceCard::setStatus` restyles every tick | Low | Open |
| [F-17](#f-17-clang-format-is-not-enforced) | `.clang-format` is not enforced | Low | Open |
| [F-18](#f-18-style-and-consistency-items) | Style and consistency items | Cosmetic | Open |
| [F-19](#f-19-log-file-path-is-converted-to-the-ansi-code-page) | Log file path is converted to the ANSI code page (Phase 0 code) | Medium | Open |

**Open findings from earlier reviews:** [F-4](phase_1.md#f-4-mainwindowonsnapshotready-only-updates-dashboardview) (snapshot forwarding to other tabs), deferred to the Phase 3 Performance tab.

---

### F-9: Network throughput total counts the same traffic several times

**Severity:** High
**Files:** `src/platform/windows/network_collector.cpp` — `queryWindowsAdapters()`, `calculateNetworkSamples()`; `src/ui/dashboard_formatting.cpp` — `sumActiveThroughput()`

**Description:**

`GetIfTable2` returns one row for every interface in the NDIS stack, not one per network card. The collector drops only `IF_TYPE_SOFTWARE_LOOPBACK`, and the dashboard adds up every adapter that is `Up`. Two kinds of rows repeat the same traffic:

1. **NDIS filter interfaces.** Every lightweight filter bound to an adapter (WFP, QoS Packet Scheduler, and third-party filters such as Npcap or VPN and antivirus filters) appears as its own `Up` row whose counters match the adapter it sits on.
2. **Tunnel and virtual adapters.** VPN tunnels, Hyper-V vEthernet adapters and similar carry traffic that also crosses the physical adapter.

`GetIfTable2` output on the review machine, showing only `Up` rows that are not loopback:

```text
 idx type hw filter      InOctets  Alias
  20    6  0      1    2022185212  Ethernet-WFP Native MAC Layer LightWeight Filter-0000
  21    6  0      1    2022185212  Ethernet-QoS Packet Scheduler-0000
  22    6  0      1    2022185212  Ethernet-WFP 802.3 MAC Layer LightWeight Filter-0000
  16    6  1      0    2022185212  Ethernet
  32   53  0      0    1914742740  Mullvad            (VPN tunnel over Ethernet)
  ...  plus 3 WAN Miniports and 6 filter rows on them, all with 0 bytes
upNonLoopback=14  sumInOctets=10003483588
```

The dashboard sums 14 rows, so the physical traffic is counted four times and the VPN traffic once more. The network card and its sparkline show about **5×** the real throughput. A machine without a VPN still sees about 4×, because the WFP and QoS filters are bound by default.

The unit tests did not catch this because `RawNetworkAdapter` has no field for "filter interface" or "hardware interface", so a fake adapter cannot represent the problem.

**Proposed Solution:**

1. Carry the two relevant `MIB_IF_ROW2::InterfaceAndOperStatusFlags` bits across the boundary:

   ```cpp
   // RawNetworkAdapter
   bool isFilterInterface{false};
   bool isHardwareInterface{false};

   // queryWindowsAdapters()
   raw.isFilterInterface = row.InterfaceAndOperStatusFlags.FilterInterface != FALSE;
   raw.isHardwareInterface = row.InterfaceAndOperStatusFlags.HardwareInterface != FALSE;
   ```

2. In `calculateNetworkSamples()`, skip filter interfaces next to loopback. They never mean anything to a user, and Task Manager does not show them.
3. Add `bool isHardwareInterface` to `domain::NetworkSample`. Make `sumActiveThroughput()` sum only adapters that are `Up` **and** hardware interfaces. Tunnel and virtual adapters stay in the snapshot so the Phase 5 network view can list them, but they do not inflate the total.
4. Add unit tests that give `calculateNetworkSamples()` a filter row and give `sumActiveThroughput()` a tunnel adapter.
5. Update the "Adapter identity & addresses" and "Dashboard cards" entries in `design_decisions.md`, which currently say "filter out loopback" and "total throughput over adapters that are Up".

**Recommendation:** Fix before starting Phase 3. The dashboard number is wrong on every Windows machine, and the Phase 3 Performance view will reuse the same data.

**Status:** Resolved (2026-09-25). `RawNetworkAdapter` carries `isFilterInterface` and `isHardwareInterface` from `InterfaceAndOperStatusFlags`. `calculateNetworkSamples()` drops filter interfaces next to loopback, and `NetworkSample::isHardwareInterface` is new. `sumActiveThroughput()` and `hasActiveAdapter()` consider only hardware adapters that are Up. New unit tests cover a filter row, the hardware flag, a VPN tunnel not being counted twice, a virtual adapter without a rate, and "only a virtual adapter is Up". Both design-decision entries are updated. On the review machine, the total now comes from the Ethernet row alone instead of 14 rows.

---

### F-10: `GetAdaptersAddresses` runs on every scheduler tick

**Severity:** Medium
**File:** `src/platform/windows/network_collector.cpp` — `queryWindowsAdapters()`

**Description:**

Every `collect()` calls both `GetIfTable2` (counters) and `GetAdaptersAddresses` (names, IP addresses, DNS servers) on the scheduler thread. Timings on the review machine (average of 50 calls after warm-up):

| Call | Time |
| --- | --- |
| `GetAdaptersAddresses` | ~2.1 ms |
| `GetIfTable2` (42 rows) | ~0.7 ms |

The architecture budgets fast collectors at "under 1 ms combined", with network counters at about 0.1 ms. The network collector alone takes about 2.8 ms per tick, and three quarters of that is spent re-reading addresses and DNS servers, which change only when the network configuration changes. The `GetAdaptersAddresses` call also retries only once on `ERROR_BUFFER_OVERFLOW`. The documented pattern is a small loop, because the required size can grow between calls.

**Proposed Solution:**

Cache `AdapterDetails` by LUID and refresh them only when needed:

- Keep `GetIfTable2` on every tick (it is the counter source).
- Re-query `GetAdaptersAddresses` when (a) `GetIfTable2` reports a LUID that is not in the cache, (b) an address-change notification (`NotifyUnicastIpAddressChange` / `NotifyIpInterfaceChange`) has set a dirty flag, or (c) a slow fallback interval such as 30 s has passed, because DNS server changes raise no IP Helper notification.
- The notification callback only sets the dirty flag, following the [event-driven collector rules](../architecture.md#event-driven-collectors-os-callback-threads). The re-query itself runs in `collect()`.
- Retry `GetAdaptersAddresses` in a loop of up to 3 attempts.
- Once the slow-collector thread exists (D-1), the address refresh could move there instead.

Also correct the architecture's timing estimate for network counters (~0.1 ms) to match the measured `GetIfTable2` cost, or remove it.

**Recommendation:** Fix together with F-9, because both change `queryWindowsAdapters()`. Record the caching approach in `design_decisions.md`.

**Status:** Resolved (2026-09-25). A new `AdapterDetailsCache` (`platform/windows/adapter_details_cache.h`, no Windows types) decides when details are due: stale mark, new interface, or 30 s maximum age. A failed query clears the details and retries on the next tick. `WindowsAdapterReader` in `network_collector.cpp` reads `GetIfTable2` every tick, re-reads `GetAdaptersAddresses` (now a loop of up to 3 attempts) only when due, and registers `NotifyUnicastIpAddressChange` / `NotifyIpInterfaceChange` as RAII members whose callbacks only mark the cache stale. Added 11 unit tests for the policy and 2 integration tests (details kept across the cached path; repeated construction and destruction). Updated the architecture timing and added a design decision.

---

### F-11: Persistent failures are logged on every tick

**Severity:** Medium
**Files:** `src/platform/windows/disk_collector.cpp` — `readDiskSpace()`; `memory_collector.cpp`; `cpu_collector.cpp`; `network_collector.cpp` — `queryWindowsAdapters()`; `connectivity_collector.cpp` — `queryConnectivityHint()` (polling fallback)

**Description:**

Every collector logs a failure each time it happens, so a condition that persists produces a log line every second. The design decision for disk collection expects this case: a BitLocker-locked or unformatted fixed volume makes `GetDiskFreeSpaceExW` fail, and `readDiskSpace()` then logs a `warn` for that volume **every tick**. That is about 86,000 lines a day. Two things make it worse:

- `configureLogging()` sets `flush_on(spdlog::level::warn)`, so each of those lines also forces a disk flush. That works against the goal of keeping the monitor's own disk impact low.
- With 5 MB × 3 rotating files (about 15 MB), one failing volume fills the whole log in a day or two, and the useful history (startup, connectivity changes, real errors) is rotated away. Each additional failure source shortens that time.

The same happens with a failing `GetSystemTimes`, `GlobalMemoryStatusEx` or `GetIfTable2` (`error`), with `NtQuerySystemInformationEx` failing (two `warn` lines per tick, including the fallback notice), and with the connectivity polling fallback.

**Proposed Solution:**

Log when an operation starts failing and when it recovers, not on every tick. The connectivity collector's change-only logging already follows this approach. A small helper in `platform/windows` keeps it consistent:

```cpp
// Tracks one repeating operation so its failure is logged once and its
// recovery once, rather than on every scheduler tick.
class FailureLogState
{
public:
    /** Returns true on the first failure after a success (or at start). */
    [[nodiscard]] bool enterFailure() { return !std::exchange(m_isFailing, true); }

    /** Returns true on the first success after a failure. */
    [[nodiscard]] bool enterSuccess() { return std::exchange(m_isFailing, false); }

private:
    bool m_isFailing{false};
};
```

Disk needs one state per volume, e.g. a `std::unordered_map<std::string, FailureLogState>` in `DiskCollector`. The default readers are lambdas and free functions today, so the state has to live in the collector or in captured state. The repeated failures can still go to `debug` for troubleshooting.

**Recommendation:** Fix before Phase 4. The process collector will add per-process access failures, which make the problem far larger unless this pattern already exists.

**Status:** Resolved (2026-09-25). Added `RepeatedFailureLog` (`platform/windows/repeated_failure_log.h`) instead of the proposed `FailureLogState`. It takes the log level and message itself, so each call site is one line, and it logs the recovery with the number of consecutive failures. It is applied to `GetSystemTimes`, both per-core `NtQuerySystemInformation(Ex)` paths (the separate "falling back" line is folded into the group-query message), `GlobalMemoryStatusEx`, `GetLogicalDrives`, `GetDiskFreeSpaceExW` (one log per volume), `GetIfTable2`, `GetAdaptersAddresses`, and `GetNetworkConnectivityHint`. One-time registration failures still log directly. Five unit tests check the emitted log lines through an in-memory sink.

---

### F-12: Empty disk and network vectors hide collector failures

**Severity:** Low
**Files:** `src/monitoring/collector.h`, `src/domain/system_snapshot.h`, `src/platform/windows/disk_collector.cpp`, `network_collector.cpp`, `src/ui/dashboard_view.cpp` — `updateNetworkCard()`

**Description:**

`IDiskCollector` and `INetworkCollector` return `std::vector`. On failure (`GetLogicalDrives` returns 0, or `GetIfTable2` fails) they return an empty vector, which looks the same as "this machine has no fixed volumes" or "no adapters". This is the same missing-data-as-default pattern that D-6, D-7 and D-8 removed elsewhere. It has one visible effect: when `GetIfTable2` fails, the network card says **"No active adapter"**, which states something that was never observed. The disk card shows "N/A" either way, and disk health is Unknown either way, so disk is affected only in principle.

**Proposed Solution:**

Apply the D-7 approach to the two remaining collections:

```cpp
using IDiskCollector = ICollector<std::optional<std::vector<domain::DiskSample>>>;
using INetworkCollector = ICollector<std::optional<std::vector<domain::NetworkSample>>>;
```

Make `SystemSnapshot::disks` and `::networks` optional: `std::nullopt` means no collector or a failed query, and an empty vector means the query succeeded and found nothing. The dashboard then shows "N/A" for `std::nullopt` and "No active adapter" only for a successful, empty result. `IProcessCollector` should follow the same shape when it is implemented in Phase 4.

**Recommendation:** Fix before Phase 4 so the process collector starts with the right shape. It breaks the domain rule, so if it is not fixed straight away, record it in `known_deviations.md`.

**Status:** Resolved (2026-09-25). The disk, network, and process collector interfaces and snapshot fields are now `std::optional<std::vector<...>>`. `DiskCollector` returns `std::nullopt` when enumeration fails, or when fixed drives exist but every read failed, and `FixedDriveEnumerator` now returns an optional. `NetworkCollector` returns `std::nullopt` when the interface table cannot be read. The dashboard shows "N/A" instead of "No active adapter" for missing network data. New tests cover enumeration failure, all reads failing, missing disk data in health evaluation, and the dashboard's network card without data. Added a design decision.

---

### F-13: `platform/windows` depends on `monitoring` headers without declaring it

**Severity:** Low
**Files:** `src/platform/windows/*_collector.h`, `src/platform/windows/CMakeLists.txt`, `docs/architecture.md` (module table)

**Description:**

Every Windows collector header includes `monitoring/collector.h` to implement `ICpuCollector` and the other interfaces. `sysmon_platform_windows` links only `sysmon_domain`, so the include resolves only because `src/` happens to be on its include path. CMake does not know about the dependency. The build stays correct only while `collector.h` is header-only and has no Qt dependency, and nothing enforces either condition. The architecture's module table does not mention this direction (platform → monitoring interfaces) either. The design intent is clear from `src/monitoring/AGENTS.md` ("Concrete Windows collectors belong exclusively in `platform/windows/`"), but it is not written down as a dependency rule.

Linking `sysmon_monitoring` would not be right either, because it would pull `Qt6::Core` into `platform/windows`, whose rules forbid Qt.

**Proposed Solution:**

Split the interfaces into a Qt-free, header-only target and link it explicitly:

```cmake
# src/monitoring/CMakeLists.txt
add_library(sysmon_collector_interfaces INTERFACE)  # collector.h: std + domain only
target_include_directories(sysmon_collector_interfaces INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(sysmon_collector_interfaces INTERFACE sysmon_domain)

target_link_libraries(sysmon_monitoring PUBLIC sysmon_collector_interfaces ...)

# src/platform/windows/CMakeLists.txt
target_link_libraries(sysmon_platform_windows PUBLIC sysmon_collector_interfaces ...)
```

Add a line to the architecture's module table and to `src/platform/windows/AGENTS.md`: `platform/windows` implements the collector interfaces from `monitoring/collector.h` and must not include anything else from `monitoring`. Record the target split in `design_decisions.md`.

**Recommendation:** Fix when convenient. It is a small change and makes the dependency explicit before more collectors arrive in Phase 4.

**Status:** Resolved (2026-09-25). `collector.h` moved into the new `INTERFACE` target `sysmon_collector_interfaces`, which `sysmon_monitoring` and `sysmon_platform_windows` both link publicly. Documented in the architecture's module table, in `src/platform/windows/AGENTS.md`, and in a design decision. `system_monitor_platform_tests` links `sysmon_platform_windows` without Qt, so the build now checks that the interfaces stay Qt-free.

---

### F-14: Connectivity callback writes to the log on the OS thread

**Severity:** Low
**File:** `src/platform/windows/connectivity_collector.cpp` — `applyStatus()`

**Description:**

[Event-driven collector rule 1](../architecture.md#event-driven-collectors-os-callback-threads) says the callback "converts the notification to a domain value and stores it in mutex-protected state… It does no blocking or long-running work." The notification path calls `applyStatus()`, which stores the value and then calls `spdlog::info` when the status changed. The rotating file sink writes to the file synchronously under its own mutex and can also rename files during rotation, so the OS thread-pool thread ends up doing file I/O. Connectivity changes are rare, so the practical cost is small, but the code does not follow the rule as written, and `ConnectivityCollector` is described as the reference implementation that later collectors will copy.

**Proposed Solution:**

Keep the callback store-only and log the change from `collect()` on the scheduler thread:

```cpp
domain::ConnectivityStatus ConnectivityCollector::collect()
{
    if (m_pollingReader) {
        storeStatus(m_pollingReader().value_or(domain::ConnectivityStatus{}));
    }

    domain::ConnectivityStatus status;
    {
        const std::lock_guard lock(m_mutex);
        status = m_cachedStatus;
    }

    // Only the scheduler thread reads or writes m_lastLoggedStatus.
    if (status != m_lastLoggedStatus) {
        spdlog::info("Connectivity changed: level={}, metered={}", levelName(status.level),
                     meteredName(status.isMetered));
        m_lastLoggedStatus = status;
    }
    return status;
}
```

The trade-off is that a change which reverts within one tick is not logged. That is acceptable for a status log. If the team would rather keep logging in the callback, amend rule 1 to allow non-blocking or rare logging instead.

**Recommendation:** Fix when convenient, before another event-driven collector copies the pattern (F-10's address notifications would be the next one).

**Status:** Resolved (2026-09-25). `applyStatus()` became `storeStatus()`, which only writes the cache under the mutex. `collect()` compares the cached status with `m_lastLoggedStatus` (used on the collecting thread only) and logs the change there. F-10's address notifications already followed the rule. A new unit test captures the log and checks that a notification logs nothing and that the following `collect()` calls log the change exactly once. The log-capture helper moved to `tests/unit/log_capture.h` so the F-11 tests share it. Updated the connectivity design decision.

---

### F-15: CPU multi-group aggregation keys on core count, not group count

**Severity:** Low
**File:** `src/platform/windows/cpu_collector.cpp` — constructors and `collect()` (`currentCoreTimes.size() > 64`)

**Description:**

The [multi-group design decision](../design_decisions.md#multi-processor-group-support-64-logical-cores-ntquerysysteminformationex-iteration-and-total-cpu-aggregation) says `GetSystemTimes` reports only the calling thread's processor group, so the collector must add up per-core times across all groups instead. The code decides to aggregate when there are **more than 64 cores**, but the condition that actually matters is **more than one processor group**. Windows can create several groups with 64 or fewer logical processors in total: on some NUMA servers, for example, or when the group size is set with `bcdedit /set groupsize`. On such a machine the collector would use `GetSystemTimes` and, by the design decision's own reasoning, miss the secondary groups.

**Proposed Solution:**

Decide by group count. For example, the default constructor records `GetActiveProcessorGroupCount() > 1` as `m_isMultiGroup`, and the injected constructor takes it as a parameter so the existing 128-core tests keep working. Add a unit test with two groups of 8 cores.

**Recommendation:** Fix when convenient. It is hard to hit on desktop hardware, but it is a small change.

**Status:** Open

---

### F-16: `ResourceCard::setStatus` restyles every tick

**Severity:** Low
**File:** `src/ui/resource_card.cpp` — `setStatus()`

**Description:**

`DashboardView::updateSnapshot()` calls `setStatus()` on four cards every second. Each call runs `QWidget::setStyleSheet()`, which makes Qt re-polish the label's style even when the level has not changed. The cost is small today, but it is wasted work on the UI thread, and the Performance view will add more widgets with the same pattern.

**Proposed Solution:**

```cpp
void ResourceCard::setStatus(std::optional<domain::HealthLevel> level)
{
    if (level == m_status) {
        return;
    }
    // ... existing body
}
```

`m_status` starts as `std::nullopt` and the constructor calls `setStatus(HealthLevel::Unknown)`, so the first call always applies.

**Recommendation:** Fix when convenient.

**Status:** Open

---

### F-17: `.clang-format` is not enforced

**Severity:** Low
**Files:** `.clang-format`, `scripts/bootstrap.ps1`, `scripts/build.ps1`, `.github/workflows/ci.yml`

**Description:**

F-7 added `.clang-format`, but nothing runs it. `clang-format` is not installed by `bootstrap.ps1`, is not on the review machine, and CI has no format check. The drift that F-7 warned about has started: lines over the 120-column hard limit, Qt headers inside the standard-library include group, and SDK includes in different orders (all in F-18). The file also leaves `PointerAlignment` unset, so it falls back to LLVM's `Right` (`Type &name`). The code follows that style consistently, but the examples in `coding_guidelines.md` use `Type& name`.

**Proposed Solution:**

1. Have `bootstrap.ps1` detect or install `clang-format` (LLVM).
2. Add a `-CheckFormat` switch to `build.ps1` that runs `clang-format --dry-run --Werror` on `src/` and `tests/`, with comment-based help and the AGENTS.md build commands updated to match.
3. Add the same check to CI.
4. Set `PointerAlignment` explicitly and make the guideline examples match whichever style is chosen.
5. Validate the configuration on the current code first, as F-7 recommended, and apply the result in one formatting-only commit.

**Recommendation:** Fix when convenient, ideally before Phase 3, which adds a lot of UI code.

**Status:** Open

---

### F-18: Style and consistency items

**Severity:** Cosmetic
**Files:** as listed

**Description and proposed fixes:**

| Item | Location | Fix |
| --- | --- | --- |
| Lines over the 120-column hard limit | `cpu_collector.cpp:70` (131), `network_collector.cpp:173` (121), `network_collector.cpp:217` (148) | Wrap |
| Qt headers inside the standard-library include group | `sampling_scheduler.h:8` (`<QObject>`), `sampling_scheduler.cpp:6` (`<QMetaType>`), `app/main.cpp:4-7` | Move to the third-party group. Phase 1 code that the Phase 1 review missed |
| Windows SDK headers ordered and spelled inconsistently | SDK before spdlog in `cpu`, `connectivity`, `network`; after it in `memory`, `disk`. Both `<Windows.h>` and `<windows.h>` are used | Add a rule to `coding_guidelines.md` (e.g. SDK headers as their own block inside the third-party group) and use one spelling |
| Raw C arrays | `disk_collector.cpp:30,33`, `network_collector.cpp:117,135`, `app_logging.cpp:26` | `std::array` (works with `inet_ntop` through `.data()`) |
| Type aliases not in `PascalCase` | `cpu_collector.cpp:36,43` (`pfnNtQuerySystemInformation…`) | e.g. `NtQuerySystemInformationFn` |
| `using namespace std::chrono;` directive | `dashboard_formatting.cpp:39` | The guidelines ask for targeted `using` declarations for `std` names |
| Windows API return values not checked | second `WideCharToMultiByte` call (`network_collector.cpp:38`), `CancelMibChangeNotify2` (`connectivity_collector.cpp:77`) | Check the result and log on failure |
| Unicast and DNS address loops duplicated | `network_collector.cpp:113-147` | Extract a `sockaddrToString()` helper |
| Two `SteadyClockReader` aliases | `CpuCollector::SteadyClockReader` (nested) and `platform::SteadyClockReader` (namespace scope) | Define it once, e.g. in a small `platform/windows` header |
| Pure calculation helpers shaped differently | `NetworkCollector::calculateNetworkSamples` is a static member; `calculateCpuUsage`, `calculateMemorySample` and `calculateDiskSample` are free functions | Make it a free function |
| `#pragma comment(lib, …)` | `network_collector.cpp:19-20` | Remove; CMake already links `iphlpapi` and `ws2_32` |
| `explicit` on default constructors | `CpuCollector`, `MemoryCollector`, `DiskCollector` | Remove (no effect; `UptimeCollector` and `NetworkCollector` do not use it) |
| Empty-body style | `{}` vs `{\n}` across collector constructors | Settle through `clang-format` (F-17) |
| Domain types documented with `//` | all `src/domain/*.h` | The guidelines ask for `/** */` on public API. Phase 1 code |
| Empty lines in a document | `design_decisions.md:280-284` | Remove |
| Test files use several project `using namespace` directives | `tests/**` | Allowed in practice, but the rule only mentions "a file's own project namespace". Say explicitly in the guidelines that tests may use the namespaces under test |

**Recommendation:** Fix alongside F-17, since most of these are what a format check would catch.

**Status:** Open

---

### F-19: Log file path is converted to the ANSI code page

**Severity:** Medium (Phase 0 code, outside this phase's scope, found during this review)
**File:** `src/app/app_logging.cpp` — `configureLogging()`

**Description:**

```cpp
std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath.string(), ...);
```

`main.cpp` builds the log path correctly as a wide `std::filesystem::path` from `%LOCALAPPDATA%`. `path::string()` then converts it to the process's ANSI code page. If the Windows user name contains characters that the code page cannot represent (for example a Chinese or Cyrillic user name on a machine whose system locale is Western European), MSVC's `path::string()` throws `std::system_error` ("No mapping for the Unicode character exists in the target multi-byte code page"). Nothing catches it, so the application terminates at startup before any window appears. This also breaks the project's no-exceptions rule.

**Proposed Solution (choose one):**

- **UTF-8 active code page.** Set `<activeCodePage>UTF-8</activeCodePage>` in the application manifest. This needs Windows 10 1903 or later, and the project already requires 2004 for `GetNetworkConnectivityHint`. `path::string()` then produces UTF-8, and the CRT opens narrow file names as UTF-8. This also fits the UTF-8 domain-string decision. The manifest is already planned for Phase 6 (DPI awareness, `asInvoker`), so this would bring that item forward.
- **Wide file names in spdlog.** Build spdlog with wide file-name support (`SPDLOG_WCHAR_FILENAMES`; check what the vcpkg port offers) and pass `logFilePath.wstring()`.

Either way, add a test that configures logging with a non-ASCII path in a temporary directory.

**Recommendation:** Fix soon. The issue is outside Phase 2, but it crashes the application at startup for affected users.

**Status:** Open

---

## Items Verified — No Issues Found

- **Module boundaries:** no Windows headers in `ui`, `ui/charts`, `domain` or `monitoring`; no `monitoring` or `platform` includes in `ui`; no Qt in `domain` or `platform/windows`. `connectivity_hint_mapping.h` exposes SDK types and is included only by `platform/windows` and `tests/platform/`, as documented.
- **API choices match the architecture:** `GetSystemTimes`, `NtQuerySystemInformation(Ex)` for per-core values (resolved dynamically from `ntdll.dll`, with a resizing-buffer loop), `GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW` restricted to `DRIVE_FIXED`, `GetIfTable2` (64-bit counters), `GetAdaptersAddresses`, `GetNetworkConnectivityHint` / `NotifyNetworkConnectivityHintChange`, and `GetTickCount64`. No PDH, Tool Help, WMI, legacy `GetIfTable` or `…A` APIs.
- **RAII for Windows resources:** `ScopedMibIfTable2` releases with `FreeMibTable` on every path. `WindowsConnectivitySubscription` cancels with `CancelMibChangeNotify2` and is declared last in `ConnectivityCollector`, so it is destroyed before the state its callback writes.
- **Event-driven collector rules 2–5:** cancel before teardown, seed synchronously before subscribing, `collect()` reads only the cache, polling fallback (tracked as D-2). Rule 1 is covered by F-14.
- **Rates use `steady_clock`:** CPU and network rates divide by measured monotonic elapsed time. Counter resets, zero elapsed time and first samples produce `std::nullopt`, not zero.
- **No zero placeholders** for CPU, memory, disk capacity, throughput and connectivity. The dashboard shows "N/A" and records sparkline gaps. The remaining gap is F-12.
- **Health evaluation** lives in `monitoring`, is pure, is carried in the snapshot, and never reports missing data as Healthy.
- **Snapshot pipeline:** `SystemSnapshot` stays a copyable value type. The UI receives it only through `Qt::QueuedConnection`, and no widget is touched off the UI thread.
- **Qt ownership:** every widget in `DashboardView`, `ResourceCard` and `SparklineWidget` is parented, with no smart pointers mixed in. Charts follow the architecture: custom `QPainter` drawing, `QPainterPath` and a `QLinearGradient` fill, with no charting library.
- **`RingBuffer` in `domain`:** header-only, standard library only, and allocates nothing on push, as `src/domain/AGENTS.md` requires.
- **Tests:** names follow `TypeName_Scenario_ExpectedResult`. `ASSERT_*` is used only to guard dereferences and indexing. `tests/unit/` includes no Windows headers and constructs no OS-backed collectors. Integration tests check invariants rather than fixed values.
- **Known deviations:** D-3 to D-8 were resolved during the phase with matching code, tests and documentation. D-1 and D-2 remain open as planned for Phase 4 and are referenced by comments at the deviating code (`sampling_scheduler.cpp`, `connectivity_collector.cpp`).
- **Design decisions:** every non-obvious Phase 2 choice has a Decision / Rationale entry in `design_decisions.md`.
- **No banned constructs:** no exceptions, `new`/`delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, raw `std::thread`, `qDebug`/`std::cout`/`printf`, `using namespace std;`, or commented-out code.
