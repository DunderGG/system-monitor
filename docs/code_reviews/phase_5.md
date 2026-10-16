# Code Review — Phase 5 Completion

**Date:** 2026-10-16
**Scope:** Everything added or changed in Phase 5 (commits `cdff80c`..`b559f4b`): the Network tab (`NetworkView`, `AdapterCard`, `network_formatting`), the shared `AdapterThroughputHistory` and `sample_history`, the adapter type (`domain::AdapterType`, `toAdapterType()`), roaming (`ConnectivityStatus::isRoaming`, `toIsRoaming()`), the network names (`domain::NetworkProfile`, `NetworkProfileCollector`, `toNetworkCategory()`), the event-driven wake-up (`IEventDrivenCollector`, the scheduler's event refresh and the collectors' change listeners), the Network tab benchmark, their tests, and the related documentation. Phase 1–4 files that Phase 5 touched were checked again.
**Reviewed against:** [architecture.md](../architecture.md), [coding_guidelines.md](../coding_guidelines.md), [design_decisions.md](../design_decisions.md), [known_deviations.md](../known_deviations.md), [roadmap.md](../../roadmap.md), [AGENTS.md](../../AGENTS.md) and the module `AGENTS.md` files
**Previous reviews:** [Phase 1](phase_1.md) (F-1 to F-8), [Phase 2](phase_2.md) (F-9 to F-19), [Phase 3](phase_3.md) (F-20 to F-28), [Phase 4](phase_4.md) (F-29 to F-34). Finding IDs continue from there.

---

## Overall Assessment

Phase 5 is smaller than Phase 4: 6 commits, 70 files, about 3,400 added lines and 83 new test cases. The code is careful, and most of the design holds up:

- **Boundaries:** the phase's one new Windows dependency, the Network List Manager, stays in `platform/windows`. COM types and adapter GUIDs never leave it: names reach the UI as `domain::NetworkProfile`s keyed by the interface LUID that every other adapter structure already uses.
- **Event-driven collectors:** the change-listener protocol is small and correct. Callbacks store, then call a listener that only sets a flag. Listeners are called and cleared under the collector's own mutex, and the lock order is the same everywhere. A probe destroyed 150 schedulers at random points while notifications were being delivered, without a deadlock.
- **COM:** every interface pointer, the `BSTR` and the apartment are owned by RAII types, the apartment is declared first so it is released last, and `S_FALSE` and `RPC_E_CHANGED_MODE` are balanced correctly.
- **Missing data:** "Unknown", "None" and "N/A" stay distinct for the level, the cost, roaming and the network names.
- **Build:** a full Release recompile has no warnings, formatting is clean, and all 715 tests pass. The two tests that need an elevated run are skipped when unelevated.

This review found three **medium-severity** issues. All three are on the new tab, and all three were confirmed by rendering it, two of them from the review machine's real adapters:

- A network's name is rendered as HTML. An access point that names its network `<img height=3000>` makes the window's minimum size 3142×3301 (F-35).
- Disconnected adapters show tentative and deprecated addresses, and placeholder DNS servers, as if they were configured. On the review machine four of the six cards listed by default do (F-36).
- The adapter's status and description are drawn in a bevel colour, at a contrast of 1.13:1 in the dark theme. The status of every adapter that is not Up is effectively invisible (F-37).

The low findings are a design risk and edge cases:

- The Network List Manager query runs on the shared slow-collector thread with no rate limit and no time limit (F-38).
- Cards of adapters that cannot carry traffic take most of the list, and the list reorders itself (F-39).
- Test gaps (F-40) and documentation figures (F-41).

Two low findings predate Phase 5 but were found while measuring it: the app ships without Qt's Windows style plugin (F-42), and the logger is shut down before the collectors are destroyed (F-43).

As in Phases 3 and 4, the tests are thorough for what they model. But their fake adapters cannot represent what Windows reports for a disconnected adapter, and no test feeds the UI text that the program does not control.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | |
| **Phase 2** — Real Windows collectors & dashboard | ✅ Complete | |
| **Phase 3** — Sparkline charts & performance views | ✅ Complete | |
| **Phase 4** — Process explorer | ✅ Complete | All Phase 4 findings are resolved |
| **Phase 5** — Network view | ✅ Complete | This review was the final unchecked item. F-35 to F-37 should be fixed before Phase 6 |
| **Phase 6** — Settings and polish | ⬜ Not started | F-42 fits with its manifest and DPI items |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐½ | Boundaries hold: COM and GUIDs stay in `platform/windows`, and the UI matches names to adapters by the LUID. The Network List Manager query is the slow thread's first cross-process call, with no rate or time limit (F-38) |
| Naming conventions | ⭐⭐⭐⭐⭐ | All 83 new test cases follow `TypeName_Scenario_ExpectedResult`. Types, members and files follow the rules |
| Formatting | ⭐⭐⭐⭐⭐ | All 189 tracked C++ files match `.clang-format` |
| C++20 usage | ⭐⭐⭐⭐⭐ | Designated initializers throughout, ranges with projections, `std::erase_if`, defaulted `operator==`, `std::exchange` for COM out-parameters, `std::jthread` and `std::stop_token` |
| Error handling | ⭐⭐⭐⭐½ | Unknown, None and N/A kept apart for every new value. A failed name read is cached and retried at the next change. Some COM failures are dropped without a log line, and roaming can be known while the cost is not (F-44) |
| Memory management | ⭐⭐⭐⭐⭐ | COM pointers, the `BSTR` and the apartment are released in the right order. Cards are Qt-parented and released with `deleteLater()` |
| Threading | ⭐⭐⭐⭐½ | The listener protocol is correct, documented, and survived a teardown stress probe. Event refreshes have no rate limit (F-38), and the scheduler logs while holding the mutex the listener takes (F-44) |
| Security and input handling | ⭐⭐½ | Network names, which a Wi-Fi access point chooses, are rendered as rich text (F-35). Phase 4 escaped or set plain text for every comparable string |
| Testing | ⭐⭐⭐½ | 83 new test cases with good seams: injected subscribers, readers and clocks. The fakes cannot represent a disconnected adapter's leftover addresses or untrusted text, and the new tab has no window-size test (F-40) |
| UI and presentation | ⭐⭐½ | Rich-text names (F-35), leftover addresses (F-36), an unreadable status and description (F-37), and a list dominated by cards that cannot carry traffic (F-39) |
| Build system | ⭐⭐⭐⭐ | The benchmark covers the new tab. The Windows style plugin is not deployed (F-42, pre-existing) |
| Documentation | ⭐⭐⭐⭐ | Six thorough decisions (DC-96 to DC-101), and the rule files were updated together. A recorded count, an environment record and a diagram are out of date (F-41) |

### Verification performed

- `.\scripts\build.ps1 -Test` (Debug): 715 tests; 713 passed and 2 were skipped because they need an elevated run. This review ran unelevated.
- `.\scripts\build.ps1 -Release -Test` after touching every C++ source, which forces a full recompile (118 translation units at `/W4`, including the review's probe): no warnings, and all tests passed.
- clang-format 22.1.3 in check mode on all 189 tracked C++ files: all match.
- `.\scripts\build.ps1 -Benchmark` once. Then three runs each of this phase's build and of the end-of-Phase-4 build (`6d9b66e`, built in a temporary worktree), interleaved, to tell a regression from the machine's state (see [Items Verified](#items-verified--no-issues-found)).
- **Rendered the Network tab from real data.** A throwaway integration test, removed afterwards, fed snapshots from the real network, connectivity and network-name collectors into `MainWindow` at 800×600 and at the default 1024×720, with and without "Show all adapters". It dumped every card, checked the window's minimum size, the scroll area's ranges and every label's width, and saved images. It also measured the contrast of each card's text in the dark and light colour schemes (F-36, F-37, F-39).
- **Compared with the OS:** `Get-NetAdapter -IncludeHidden`, `Get-NetIPAddress` (address states), `Get-DnsClientServerAddress`, `Get-NetConnectionProfile`, `Get-NetIPInterface` and `ipconfig /all` (F-36, F-41).
- **Probed untrusted text:** network names with markup that fits in a 32-byte SSID (F-35).
- **Probed the threading:**
  - 150 rounds of creating, starting, stopping and destroying a scheduler at random points, while the subscriptions of a real `ConnectivityCollector` and `NetworkProfileCollector` delivered notifications continuously from their own threads. A watchdog would have aborted on a deadlock.
  - A 2-second notification storm against a name reader that takes 5 ms, counting the reads and the full refreshes (F-38).
- **Timed the Network List Manager query:** five reads on a thread with a single-threaded apartment, and five on new threads, which initialize COM for the read (F-38).
- **Listed the available Qt styles** (`QStyleFactory::keys()`) and the deployed plugins (F-42), and read spdlog's shutdown path (F-43).
- **Searched for banned constructs** in the 56 changed C++ files: exceptions, `new`/`delete` outside Qt parenting, C-style casts, `std::bind`, `volatile`, `qDebug`/`std::cout`/`printf`, `using namespace std`, Tool Help, PDH, WMI and ANSI API variants. There were no hits: every `new` creates a Qt-parented widget or layout. Windows headers appear only in `platform/windows` and in the platform and integration tests.

---

## Findings

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [F-35](#f-35-network-names-are-rendered-as-rich-text) | Network names are rendered as rich text | Medium | Resolved |
| [F-36](#f-36-disconnected-adapters-show-leftover-addresses-and-placeholder-dns-servers) | Disconnected adapters show leftover addresses and placeholder DNS servers | Medium | Resolved |
| [F-37](#f-37-adapter-status-and-description-are-unreadable) | Adapter status and description are unreadable | Medium | Open |
| [F-38](#f-38-the-network-list-manager-query-has-no-rate-limit-and-no-time-limit) | The Network List Manager query has no rate limit and no time limit | Low | Open |
| [F-39](#f-39-cards-that-cannot-carry-traffic-dominate-the-adapter-list-and-the-order-shifts) | Cards that cannot carry traffic dominate the adapter list, and the order shifts | Low | Open |
| [F-40](#f-40-test-coverage-gaps) | Test coverage gaps | Low | Open |
| [F-41](#f-41-documentation-out-of-date) | Documentation out of date | Low | Open |
| [F-42](#f-42-qts-windows-style-plugin-is-not-deployed) | Qt's Windows style plugin is not deployed (pre-existing) | Low | Resolved |
| [F-43](#f-43-the-logger-is-shut-down-before-the-collectors-are-destroyed) | The logger is shut down before the collectors are destroyed (pre-existing) | Low | Resolved |
| [F-44](#f-44-consistency-and-presentation-items) | Consistency and presentation items | Cosmetic | Open |

**Open findings from earlier reviews:** none.

---

### F-35: Network names are rendered as rich text

**Severity:** Medium
**Files:** `src/ui/readout_grid.cpp` — `rebuild()`; `src/ui/adapter_card.cpp` — constructor; `src/ui/network_performance_page.cpp` — `rebuildAdapterCharts()`; `src/ui/AGENTS.md`

**Description:**

Every `QLabel` on the new tab uses Qt's default text format, `Qt::AutoText`. In that format, a label renders its text as HTML when `Qt::mightBeRichText()` finds something that looks like a tag before the first line break. The tab's labels show strings that come from outside the program:

- **The "Network" readout on each card, and the connectivity section's "Networks" readout,** show `NetworkProfile::name`. The Network List Manager names a Wi-Fi network after its SSID: up to 32 bytes that the access point chooses. Anyone who runs a hotspot the user joins controls this text.
- **The card's title and description line** show the adapter's alias and device description. They are local, but VPN clients and drivers choose them.

A probe gave the tab two network names, each short enough to be an SSID:

| Network name | Result |
| --- | --- |
| `<h1>FREE</h1>` | Rendered as a heading, both on the card and in the connectivity section |
| `<img height=3000>` | The readout label became 3106×3055 pixels and its card 3499 pixels tall. `MainWindow::minimumSizeHint()` became 3142×3301 |

The connectivity section is outside the scroll area, so the second name forces the main window to a minimum size larger than any common screen, for as long as that network stays connected and the tab has been shown. Rich text also ignores the `\n` that `adapterNetworkText()` and `connectivityReadouts()` use to put one network per line, and it allows spoofing: fonts, colours, or text that looks like another readout.

Phase 4 handled the same risk for process names and command lines: its tooltips pass them through `toHtmlEscaped()` (`process_formatting.cpp`), and both of its dialogs set `Qt::PlainText` (`processes_view.cpp`). `ReadoutGrid` (Phase 3) never set a format. That did no harm while it showed numbers, addresses and driver descriptions. Phase 5 is the first to put text from the network into it. Qt's documentation recommends `setTextFormat(Qt::PlainText)` for exactly this case.

**Proposed Solution:**

1. Set `Qt::PlainText` on every label that shows data:
   - the name and value labels in `ReadoutGrid::rebuild()`;
   - the card's title, status and description labels;
   - the chart titles on the Performance tab's network page.
2. State the rule in `src/ui/AGENTS.md`: a label or tooltip that shows text from the system or the network uses `Qt::PlainText`, or escapes it with `toHtmlEscaped()` inside rich text.
3. Add view tests with markup in a network name, an alias and a description. Check that each label's `textFormat()` is `Qt::PlainText`, that it shows the literal string, and that the window's `minimumSizeHint()` stays within 800×600.

**Recommendation:** Fix before Phase 6. It is a few lines of code, and the input comes from outside the machine.

**Status:** Resolved (2026-10-16). Items 1 to 3 are done.

- `ReadoutGrid` creates its name and value labels as `Qt::PlainText`, which covers every readout on both tabs, including the "Network" and "Networks" readouts. The card's title, status and description, and the chart titles on the Performance tab's network page, are plain text too. So are `ResourceCard`'s value and detail labels, which show a volume's name; Windows paths cannot contain a tag, but the rule now applies to every label that shows outside text. Each label's format is set before its text.
- `src/ui/AGENTS.md` and a new "Text from outside the program" section in the coding guidelines state the rule: `Qt::PlainText` for a label, and `toHtmlEscaped()` for each untrusted part of rich text.
- New tests give an alias, a description and two network names (`<h1>FREE</h1>` and `<img height=3000>`) markup. They check that every label on the card and in the connectivity section is plain text and shows the literal string, and that `MainWindow` shown at 800×600 on the Network tab keeps a minimum size within 800×600. Before the fix, that test reproduced the 3142×3301 above. Further tests cover `ReadoutGrid` (markup in a name, and in a value updated in place), the Performance tab's chart title and `ResourceCard`.

See [DC-102](../design_decisions.md#dc-102-labels-that-show-text-from-the-system-or-the-network-are-plain-text-code-review-f-35).

---

### F-36: Disconnected adapters show leftover addresses and placeholder DNS servers

**Severity:** Medium
**Files:** `src/platform/windows/network_collector.cpp` — `queryAdapterDetails()`; `src/ui/network_formatting.cpp` — `adapterDetailReadouts()`; `src/domain/network_sample.h`; `docs/design_decisions.md` — DC-97

**Description:**

`queryAdapterDetails()` copies every unicast address and DNS server that `GetAdaptersAddresses` lists, whatever its state, and `adapterDetailReadouts()` shows them whether or not the adapter is Up. But Windows keeps addresses on an adapter whose media is disconnected, in states that cannot be used.

On the review machine four of the six cards listed by default are Down. Every one of them shows addresses and DNS servers that `ipconfig /all` hides, because it reports these adapters as "Media disconnected":

| Card (status) | Addresses shown | State in `Get-NetIPAddress` | DNS servers shown |
| --- | --- | --- | --- |
| WiFi (Down) | 192.168.1.16, fe80::cd5b:b3cd:b185:d66b | Deprecated, Deprecated | 192.168.1.1 |
| Bluetooth Network Connection (Down) | 169.254.113.81, fe80::bc71:942f:7fb6:cbaa | Tentative, Deprecated | fec0:0:0:ffff::1, ::2, ::3 |
| Local Area Connection* 1 (Down) | 169.254.47.250, fe80::c60e:16e0:309b:edf3 | Tentative, Deprecated | fec0:0:0:ffff::1, ::2, ::3 |
| Local Area Connection* 10 (Down) | 169.254.172.62, fe80::edd1:2fa1:534:8ff7 | Tentative, Deprecated | fec0:0:0:ffff::1, ::2, ::3 |

- **The Wi-Fi card shows the lease from its last connection.** 192.168.1.16 is in the same subnet as the Ethernet adapter's 192.168.1.119, so the machine appears to be connected to the LAN twice.
- **The 169.254 addresses are Tentative automatic private addresses.** No other host can reach them.
- **`fec0:0:0:ffff::1` to `::3` are not configured servers.** They are the deprecated site-local addresses that Windows lists when it has no IPv6 DNS server.

DC-97 already hides the link speed of an adapter that is not Up, because "showing those values would describe a link that does not exist". The addresses and DNS servers describe the same link. Most Windows machines with Wi-Fi have Wi-Fi Direct virtual adapters, and many also have a Bluetooth PAN adapter, so most users see such cards.

The tests did not catch this: the fake samples have addresses but no address states, and no test gives a Down adapter any addresses. It is the same gap that F-9 and F-21 found: the fakes cannot represent what Windows reports.

**Proposed Solution:**

1. **At the platform boundary,** keep only addresses that can be used. Skip unicast addresses whose `DadState` is `IpDadStateTentative`, `IpDadStateDuplicate` or `IpDadStateInvalid`. Keep `IpDadStatePreferred`. Keep `IpDadStateDeprecated` only while the adapter is Up, because an expired temporary IPv6 address can still carry existing connections. Alternatively, carry the state into the domain, so the UI can mark deprecated addresses as `ipconfig` does.
2. **In `adapterDetailReadouts()`,** show the addresses and DNS servers only while the adapter is Up, like the link speed, and "Not connected" otherwise. The Performance tab charts only adapters that are Up, so its readouts do not change.
3. **Tests:** add collector and formatting tests with a Down adapter that has a Tentative 169.254 address, a Deprecated lease and the `fec0:0:0:ffff::` servers. Update DC-97.

**Recommendation:** Fix before Phase 6. Item 2 alone is a small UI change. Item 1 also corrects the data for any later consumer, such as export.

**Status:** Resolved (2026-10-16). Items 1 to 3 are done.

- `GetAdaptersAddresses`' `DadState` is kept with each unicast address (`AddressState`, mapped by `toAddressState()`) through the cache into `RawNetworkAdapter`. `calculateNetworkSamples()` keeps Preferred addresses, and Deprecated ones only while the adapter is Up, using the status from the current tick. Tentative, Duplicate, Invalid and unknown states are dropped. The state stays in the platform module rather than the domain.
- The placeholder DNS servers `fec0:0:0:ffff::1` to `::3` are also dropped at the boundary (`isPlaceholderDnsServer()`), whatever the adapter's status. On the development machine Windows lists them for every adapter without a configured DNS server, loopback included, which is Up, so hiding them only on adapters that are not Up would have missed an Up adapter with no DNS server.
- `adapterDetailReadouts()` shows "Not connected" for the addresses and DNS servers of an adapter that is not Up, like the link speed.
- Tests: collector tests give a Down, a Dormant and an Up adapter every address state, and model the WiFi card's Deprecated lease and the Bluetooth card's Tentative 169.254 address; a Deprecated address is kept once its adapter comes Up. Formatting tests show "Not connected" for every status other than Up, with the `fec0:0:0:ffff::` servers in the sample. A view test takes a card from Down to Up. Platform tests cover the mapping and the placeholder check, and an integration test checks that no real adapter reports a placeholder. With F-35's tests for untrusted names, this covers the second row of F-40.
- On the review machine, the collector now reports no addresses for the four Down cards in the table above, no DNS servers for the Bluetooth and two Wi-Fi Direct cards, and every Preferred address of the Ethernet and Mullvad adapters.

See [DC-103](../design_decisions.md#dc-103-unusable-addresses-and-placeholder-dns-servers-dropped-addresses-and-dns-servers-shown-only-while-up-code-review-f-36); DC-35 and DC-97 point to it.

---

### F-37: Adapter status and description are unreadable

**Severity:** Medium
**Files:** `src/ui/adapter_card.cpp` — `kNotUpStyleSheet`, constructor; `src/ui/resource_card.cpp` — `statusStyleSheet()` (pre-existing)

**Description:**

The card draws two texts with `color: palette(mid)`: its device description, and the status of every adapter that is not Up ("Down", "Not present", "Dormant" and so on). `QPalette::Mid` is the bevel colour of 3D frames, between `Button` and `Dark`. It is not meant for text. Measured on the rendered cards:

| Colour scheme | Text (`Mid`) | Background (`Window`) | Contrast |
| --- | --- | --- | --- |
| Dark (the review machine) | #282828 | #1e1e1e | 1.13:1 |
| Light | #a0a0a0 | #f0f0f0 | 2.29:1 |

WCAG's minimum for normal text is 4.5:1. In the dark scheme both texts are practically invisible: in the rendered WiFi card, "Down" and "RZ616 Wi-Fi 6E 160MHz" can barely be told from the background. The operational status is one of the roadmap's required per-adapter fields, and in the dark scheme it is now readable only for adapters that are Up. Even the Up status, bold `#2e8b57`, measures only 3.93:1 there, and its colour works out to about 3.7:1 on the light background.

The module already has a dimmed text role that works: the readout names in the same card use `QPalette::PlaceholderText` and measure 5.15:1 in the dark scheme.

The dashboard's `ResourceCard` draws its Unknown health status with the same `palette(mid)` style, so that label has had the same contrast since Phase 2. Phase 5 copied the pattern ("The Up colour is the dashboard's Healthy green").

**Proposed Solution:**

1. Draw dimmed text with the role that `ReadoutGrid` uses (`setForegroundRole(QPalette::PlaceholderText)`), or with the text colour at reduced alpha, as the charts do, instead of `palette(mid)`. Apply it to the card's description and status and to `ResourceCard`'s Unknown status.
2. Share the status styles between `AdapterCard` and `ResourceCard` (see F-44). Choose greens that reach 4.5:1 in both colour schemes, one for each scheme if necessary.
3. Add a widget test that renders a card in both colour schemes (`QStyleHints::setColorScheme()`) and checks the contrast of its status and description against the background, as the probe did.

**Recommendation:** Fix before Phase 6, together with F-36, since both change the card.

---

### F-38: The Network List Manager query has no rate limit and no time limit

**Severity:** Low
**Files:** `src/monitoring/sampling_scheduler.cpp` — `requestEventRefresh()`, `runSlowCollectors()`; `src/platform/windows/network_profile_collector.cpp` — `collect()`, `markStale()`; `docs/design_decisions.md` — DC-100, DC-101; `docs/architecture.md` — threading model

**Description:**

Before this phase, the slow-collector thread made only in-process calls into the kernel and IP Helper (`NtQuerySystemInformation`, `GetNetworkConnectivityHint`). `NetworkProfileCollector` adds its first cross-process call: COM into the Network List Service. On the review machine one read takes 4.2–5.6 ms, and the first in the process about 9 ms. That is as much as the process enumeration (3.5–6 ms) that DC-101 keeps off the event path. Nothing bounds how often the read runs, or how long it may take:

- **No rate limit.** A notification that finds the names fresh marks them stale and wakes the thread, and every event refresh then reads them again.
  - A probe gave the collector a subscription that kept notifying, and a reader that took 5 ms. It read 130 times in 2 seconds, keeping the slow thread busy a third of the time. The full refreshes kept their 200 ms cadence.
  - Real notifications come in bursts while an adapter reconnects or a link flaps. DC-101 gives exactly that as its reason not to run a full refresh for each change.
  - Each tick copies only the latest result, so every read after the first within one tick is wasted.
- **No time limit.** A COM call into a service has no timeout by default. If the Network List Service is slow to answer or stops responding, the slow thread waits inside the call:
  - the process list stops refreshing;
  - connectivity changes are not collected;
  - `SamplingScheduler::stop()` cannot join the thread, so the process does not exit after its window closes.

  DC-04 restricts WMI partly because of hung calls. DC-100 and DC-101 do not discuss this failure mode for the Network List Manager.

The cost in normal operation is fine. What the probe shows is that the cost is not bounded.

**Proposed Solution:**

1. **Coalesce event refreshes.** Let `runSlowCollectors()` run at most one event refresh per tick interval, or let `NetworkProfileCollector::collect()` keep a minimum gap between two reads. Either bounds a burst to about one read per second, and no change reaches the UI later than the tick that would show it anyway.
2. **Decide whether the names get their own worker,** so that a stuck service cannot hold up the process list. The worker could be a `std::jthread` owned by the collector, or a third scheduler thread for slow external queries. It could keep its multithreaded apartment for its lifetime, which also removes the per-read `CoInitializeEx`. Shutdown would still wait for a call in progress. If the risk is accepted instead, record it in DC-100.
3. **Record the read's cost** in DC-100 and in the architecture's slow-collector box, which gives a figure for every other slow collector.

**Recommendation:** Items 1 and 3 when convenient. Item 2 is a design choice: a dedicated thread, or a recorded risk.

---

### F-39: Cards that cannot carry traffic dominate the adapter list, and the order shifts

**Severity:** Low
**Files:** `src/ui/network_formatting.cpp` — `isListedByDefault()`, `listedAdapters()`; `src/ui/adapter_card.cpp`; `src/ui/network_view.cpp` — `refresh()`, `arrangeCards()`; `src/ui/performance_formatting.cpp` — `chartedAdapters()`; `docs/design_decisions.md` — DC-96, DC-98

**Description:**

Rendered from the review machine's 19 adapters:

- **Disconnected adapters take most of the list.** By default the tab lists six cards. Four are Down, and two of those are the Wi-Fi Direct virtual adapters, which Windows marks as hidden (`Get-NetAdapter -IncludeHidden` reports `Hidden`), so Network Connections does not show them. Each Down card has a full chart that can only draw a flat 0 B/s line on a "1 B/s" axis. At the default 1024×720 the list is 1,588 pixels tall in a 496-pixel viewport, and four of its six 110-pixel charts can only show zero. At 800×600 the viewport is 376 pixels tall and two cards are in view. This is F-21's pattern again: the Performance tab stopped charting adapters that cannot carry traffic, and this tab now charts every disconnected adapter it lists.
- **"Show all adapters" mixes its cards into the list.** It adds 13 cards, and because the order is status, then hardware, then name, they land between the default ones. "6to4 Adapter" comes before "Bluetooth Network Connection", and "Ethernet (Kernel Debugger)" before "Local Area Connection* 1". A user who turns it on cannot tell which cards it added.
- **Numbered names sort as text:** "Local Area Connection* 1", "… 10", "… 11", "… 2", …, "… 9".
- **Cards move when an adapter changes state.** Adapters that are Up come first, so a flapping link, or a Bluetooth adapter that connects, moves its card to the top and shifts every other card under the scroll position that DC-96 keeps.
- **The two tabs order the same adapters differently.** The Performance tab's `chartedAdapters()` orders hardware first, then by name with `std::ranges::stable_sort`, and has no interface-ID tie-break. DC-96 added that tie-break to the Network tab so that cards with the same name do not swap places when `GetIfTable2`'s row order changes.

**Proposed Solution:**

1. Collapse the chart of an adapter that is not Up, and show "Not connected" in its place. Expand it when the adapter comes up. Its history is recorded either way, so nothing is lost.
2. Sort the adapters that `isListedByDefault()` accepts first, so "Show all adapters" appends its cards after them, perhaps under a "Hidden adapters" heading.
3. Compare names in natural order (digits as numbers), in a way that does not depend on the locale, in both tabs.
4. Consider keeping a card in place when only its status changes, for example by ordering on hardware and name only. At least give the Performance tab the same comparator, with the ID tie-break.
5. Decide whether the hidden Wi-Fi Direct adapters belong in the default list. `MIB_IF_ROW2` has no hidden flag, but "virtual, not Up, and no Preferred address" (see F-36) would leave them out until Mobile Hotspot or a wireless display uses them.

**Recommendation:** Fix when convenient, together with F-36 and F-37, which change the same card.

---

### F-40: Test coverage gaps

**Severity:** Low
**Files:** `tests/unit/network_view_test.cpp`, `tests/unit/network_profile_collector_test.cpp`, `tests/unit/sampling_scheduler_test.cpp`, `benchmarks/performance_tick_benchmark.cpp`

**Description:**

| Gap | Consequence | Fix |
| --- | --- | --- |
| The Network tab has no window-size test. Phase 3 added 800×600 tests for the Performance pages after the network page overflowed its window (F-20), but `ShownNetworkView` is never resized | The tab fits today (minimum size 600×490, no clipped label), but nothing keeps it so. F-35's 3142×3301 minimum size fails no test | Show `MainWindow` at 800×600 with the Network tab current and a demanding snapshot: three IPv6 addresses, long descriptions, two networks. Check `minimumSizeHint()`, the scroll area's horizontal range and the label widths, as `NetworkPerformancePage.SmallWindow_ReadoutsFitWithoutHorizontalScrolling` does |
| The fake samples carry addresses but no address states, and no test passes text with markup | F-35 and F-36 pass every test | Model a disconnected adapter's leftover addresses, and untrusted names, in the fakes (see F-35 and F-36) |
| `NetworkProfileCollector::collect()` clears the stale mark before it reads, "so a change reported during the read triggers another read". No test delivers a notification from inside the reader | A change that cleared the mark after the read would lose notifications and still pass | A fake reader that calls `state.handler()` while it reads. The next `collect()` must read again |
| The scheduler's event-driven tests use only a connectivity collector. The network-name collector's listener path, a pending event when a full refresh is due (`isEventPending && now < nextRefresh`), and destruction while notifications arrive are not tested | A regression in the wake-up logic or in the lock order would show up only as flaky behaviour | A scheduler test with an event-driven fake `INetworkProfileCollector`. A test that requests an event refresh when a full refresh is due and checks that one full refresh runs. A short version of the review's teardown probe, with a watchdog |
| The Network tab benchmark times only adapters that are all listed and Up. It never times "Show all adapters" (19 cards on the review machine) or a status change, which re-inserts every card | The cost of the largest list and of a reorder is unmeasured | Add a "Show all" row with 20 adapters, half of them Down, and time one tick in which an adapter changes state |

**Recommendation:** Add the first three with the fixes for F-35 to F-37. The scheduler tests and the benchmark rows when convenient.

---

### F-41: Documentation out of date

**Severity:** Low
**Files:** as listed

**Description:**

| Item | Location | Fix |
| --- | --- | --- |
| DC-96 says that the hidden adapters that are not bound to IP "made up four of the ten adapters" on the development machine. On that machine the network collector reports 19 adapters, and 13 of them are not bound to IP: eight WAN Miniports, the RAS Async Adapter, 6to4, Teredo, IP-HTTPS and the kernel debugger adapter. The figure matches `Get-NetAdapter -IncludeHidden`, which does not list the WAN Miniports | DC-96 | Count from the collector's output, or say which tool the figure comes from |
| DC-98 records its benchmark environment as "the High performance plan, MSVC 19.51 and Qt 6.11.1". It leaves out the power mode and the Windows version, which DC-95 records with every table so that a change in the machine can be told from a regression | DC-98 | Paste the lines that `build.ps1 -Benchmark` prints, as DC-95 does |
| The architecture's system design diagram lists the Windows APIs of the integration layer, but not `INetworkListManager`. The API table below it does list it | `architecture.md` — "System design" | Add `INetworkListManager (network names, lazy)` to the diagram |
| No document gives the cost of a Network List Manager read, although the architecture gives one for every other collector | `architecture.md` — threading model; DC-100 | Record about 4–5 ms per read, and about 9 ms for the first (see F-38) |
| The comment on the deployment step says that "only the platform plugin" is loaded dynamically and needs copying | `src/app/CMakeLists.txt` | See F-42 |

**Recommendation:** Fix when convenient, together with the findings that change the same entries.

---

### F-42: Qt's Windows style plugin is not deployed

**Severity:** Low (pre-existing)
**Files:** `src/app/CMakeLists.txt`; `tests/CMakeLists.txt`; `benchmarks/CMakeLists.txt`; `tests/unit/qt_configuration_test.cpp`

**Description:**

vcpkg builds `qmodernwindowsstyle.dll`, the plugin that provides Qt's `windows11` and `windowsvista` styles (`Qt6::QModernWindowsStylePlugin`). The build copies only the platform plugin next to `system_monitor.exe`, and its comment says that "only the platform plugin, which is loaded dynamically rather than linked, needs to be placed manually next to the executable". The style plugin is loaded dynamically too.

Without a `styles` directory, `QStyleFactory::keys()` returns only "Windows" and "Fusion", and the app runs with Qt's built-in `windows` style. Its check boxes, scroll bars, tab bar and frames are the classic bevelled controls rather than the Windows 10/11 look, and the dark palette comes from that style. The tests and benchmarks deploy the same way, so they match the app; every colour this review measured for F-37 is from this style.

This predates Phase 5 and was found while measuring it.

**Proposed Solution:**

1. Copy `Qt6::QModernWindowsStylePlugin` into a `styles` directory next to the app, the tests and the benchmarks, in the same way as the platform plugin. Correct the comment.
2. Extend `qt_configuration_test.cpp`, which already guards the PNG feature (DC-86), to check that `QStyleFactory::keys()` contains "windows11".
3. Re-measure the benchmarks and F-37's contrast with the new style, and record the decision.

**Recommendation:** Fix in Phase 6, with the manifest and DPI items. It changes how every tab looks, so check the pages at 800×600 again afterwards.

**Status:** Resolved (2026-10-16). Items 1 to 3 are done, ahead of Phase 6 because F-37's colours depend on the style.

- A top-level CMake function, `sysmon_deploy_qt_plugins()`, copies the platform plugin and `Qt6::QModernWindowsStylePlugin` next to the app, the tests and both benchmarks, in place of three copies of the platform plugin's step. Its comment says which plugins are loaded at run time, which corrects the comment in `src/app/CMakeLists.txt` (F-41's last row).
- `qt_configuration_test.cpp` checks that `QStyleFactory::keys()` lists `windows11` and `windowsvista`, and that the application runs with one of them.
- Under `windows11` every test passes, including the 800×600 tests. Renders of every tab at 800×600 in both colour schemes fit, with a minimum size of 602×492. The style draws tab pages and cards in the base colour (#ffffff, or #2d2d2d in the dark scheme), so F-37's contrast is measured against that.
- The benchmark ran alternately with and without the plugin on the same binary. The style adds about 1 ms per tick on the Network tab and on the network page, and nothing measurable on the other pages.

See [DC-104](../design_decisions.md#dc-104-qts-windows-style-plugin-deployed-next-to-every-executable-code-review-f-42).

---

### F-43: The logger is shut down before the collectors are destroyed

**Severity:** Low (pre-existing; Phase 5 adds one more path)
**Files:** `src/app/main.cpp`; `src/platform/windows/connectivity_collector.cpp` — `~WindowsConnectivitySubscription()`; `src/platform/windows/network_collector.cpp` — `MibNotificationDeleter`

**Description:**

`main()` stops the scheduler, logs its exit and calls `spdlog::shutdown()`, then returns. Only then are `mainWindow` and `scheduler` destroyed, and the scheduler destroys its collectors.

`spdlog::shutdown()` drops every logger, including the default one: `registry::drop_all()` resets `default_logger_`. `spdlog::warn()` then calls `default_logger_raw()->log(...)` without checking for null. The collectors' teardown logs on failure: `~WindowsConnectivitySubscription()` and `MibNotificationDeleter` both log "CancelMibChangeNotify2 failed …". Such a line at exit dereferences a null pointer, so the process crashes on its way out, and the message that would explain the failure is lost.

Phase 5 adds a third registration that logs this way (`NetworkProfileCollector`'s subscription). The failure itself is rare. But the architecture's logging table suggests logging "collector lifecycle" at debug, and a collector that did so in its destructor would crash the app on every exit.

**Proposed Solution:**

Destroy the window and the scheduler before the logger is shut down. For example, put them in a block that ends before `spdlog::shutdown()`, or hold the scheduler in a `std::optional` and `reset()` it after `stop()`.

**Recommendation:** Fix when convenient. It is a small change to `main.cpp`.

**Status:** Resolved (2026-10-16). `main()` now calls `runMonitor()`, which owns the scheduler and the window. They and the collectors are destroyed when it returns, before `main()` logs its exit and calls `spdlog::shutdown()`. A comment there gives the reason, and the architecture's logging section states the order. The app still exits with code 0, and its log now records the exit after the collectors are gone.

---

### F-44: Consistency and presentation items

**Severity:** Cosmetic
**Files:** as listed

| Item | Location | Fix |
| --- | --- | --- |
| Duplicated card constants. The Up/Healthy green `#2e8b57`, the dimmed `palette(mid)` style and the card margins `12, 10, 12, 10` are written out in both `AdapterCard` and `ResourceCard`. `kHistoryCapacity = 60` ("one minute … as on the dashboard") is defined in both `NetworkView` and `DashboardView`. F-28 removed this kind of duplication | `adapter_card.cpp`, `resource_card.cpp`, `network_view.h`, `dashboard_view.h` | Shared card styles and margins in `view_metrics.h` or a small card-style header, and one constant for the one-minute history |
| `ConnectivityStatus::isRoaming` "is std::nullopt whenever isMetered is". `toIsMetered()` maps every unrecognised cost value to `std::nullopt` (its `default:` case), but `toIsRoaming()` returns a value for everything except `NetworkConnectivityCostHintUnknown`. A cost value added in a later SDK would give an unknown cost with a known roaming flag | `connectivity_hint_mapping.cpp` — `toIsRoaming()`; `connectivity_status.h` | `if (!toIsMetered(cost)) { return std::nullopt; }`, and a platform test with an out-of-range cost |
| Both scheduler threads write their start line to the log (`spdlog::info`, a file write) while holding `m_sleepMutex`. Since DC-101, `requestEventRefresh()` takes that mutex on OS callback threads, where the event-driven rules allow no blocking work | `sampling_scheduler.cpp` — `run()`, `runSlowCollectors()` | Copy the interval under the lock and log after releasing it |
| `connectedInterfaceIds()` drops a connection without a log line when `GetNetworkConnections`, `GetAdapterId` or `ConvertInterfaceGuidToLuid` fails. A network that shows "None" on every card then leaves no trace. The guidelines ask for every Windows call's result to be checked and reported | `network_profile_collector.cpp` — `connectedInterfaceIds()` | Log the failure at debug, as the unreadable-name case does |
| IPv6 link-local addresses are shown without the zone index (`%16`) that `ipconfig` shows and that is needed to use them. Temporary and link-local addresses are not marked, so the Ethernet card shows three similar-looking IPv6 lines | `network_collector.cpp` — `sockaddrToString()`; `network_formatting.cpp` | Append `%` and `sin6_scope_id` for link-local addresses. Consider marking "(temporary)" and "(link-local)", which needs the address origin from F-36's item 1 |
| The placeholder says "No network adapters" when adapters exist but every one of them is behind "Show all adapters" | `network_view.cpp` — `refresh()` | For example: "No adapters to list. Select Show all adapters to list hidden ones." |
| A VPN tunnel reports a nominal link speed: the Mullvad card says "100 Gbps". DC-97's argument for hiding a disconnected adapter's speed ("a link that does not exist") applies to a virtual link too | `network_formatting.cpp` — `adapterDetailReadouts()` | Optional: show a virtual adapter's speed as reported but say so in a tooltip, or leave it out |
| `build.ps1 -CheckFormat` stops at the first batch of 50 files that fails, so unformatted files in later batches are not listed (pre-existing) | `scripts/build.ps1` | Check every batch, then fail once at the end |

**Recommendation:** Fix when convenient, ideally in the commits that touch these files for F-35 to F-38.

---

## Risks Not Reproduced

- **Names read before Windows identifies a network.** An event refresh reads the names within milliseconds of a connectivity notification. If the Network List Manager updates its list only after it sends the notification, for example while a new network is still "Identifying…", a card keeps the old or temporary name until the next notification, or for up to a minute. Microsoft documents no order between the two, and testing it needs a network change on the review machine, which this review did not make. If it shows up, the Network List Manager's own events (which DC-100 rejected because they need a COM event sink) or a second, delayed read after each event refresh would cover it.

---

## Items Verified — No Issues Found

- **Module boundaries:**
  - `ui` includes no Windows headers. Names and connectivity reach it only as `domain::NetworkProfile` and `domain::ConnectivityStatus`.
  - The new domain types (`AdapterType`, `NetworkProfile`, `NetworkCategory`, `isRoaming`) use only the standard library.
  - The headers that expose SDK types (`adapter_type_mapping.h`, `network_category_mapping.h`, `connectivity_hint_mapping.h`) say that they are internal, and only platform sources and `tests/platform/` include them. `network_profile_collector.h` and `connectivity_collector.h` expose no Windows types, so the unit tests can include them.
- **API choices:**
  - The names come from `INetworkListManager`, read only on the slow-collector thread.
  - Adapter GUIDs are converted to LUIDs with `ConvertInterfaceGuidToLuid` at the boundary.
  - The adapter type comes from `MIB_IF_ROW2`, which `GetIfTable2` already returns.
  - There is no WMI, PDH or Tool Help, and only wide API variants are used.
- **COM handling:**
  - The apartment is declared before every COM object, so they are released before `CoUninitialize`. `S_FALSE` is balanced and `RPC_E_CHANGED_MODE` is not, and an integration test runs the collector on a thread with a single-threaded apartment.
  - Every interface pointer and the `BSTR` are owned by `std::unique_ptr` deleters. `std::exchange` hands each enumerated pointer over, so none is released twice.
  - `Next(1, …, nullptr)` is valid for a count of one. `SysStringLen` sizes the name, so embedded nulls are kept.
- **Event-driven rules:**
  - Callbacks store, then call the listener, which only sets a flag and notifies.
  - Subscriptions are each collector's last member. The scheduler clears the listeners in its destructor, under each collector's mutex, after stopping its threads.
  - Locks are always taken in the order collector mutex, then sleep mutex, and the scheduler never calls a collector while it holds the sleep mutex. The teardown probe (150 rounds under a continuous notification storm) found no deadlock.
  - The flag is set under the mutex and waited for with a predicate, so no wake-up is lost. A pending event that finds a full refresh due is answered by the full refresh, and full refreshes kept their cadence under the storm.
- **Network names:**
  - On the review machine, both connected networks ("Normandy" and "Mullvad", both Private) matched the LUIDs of the Ethernet and Mullvad adapters.
  - A failed read is cached and retried at the next notification or after a minute, not on every slow refresh, and its failures are logged once through `RepeatedFailureLog`.
  - The stale mark is cleared before the read, so a change reported during a read causes another read (by reading the code; see F-40).
- **Real-data checks:**
  - **Adapter types:** for the ten adapters that `Get-NetAdapter -IncludeHidden` lists, each type matched the media it reports. The Bluetooth PAN adapter (Ethernet interface type, Bluetooth medium) reads "Bluetooth (virtual)", the Wi-Fi Direct adapters "Wi-Fi (virtual)", Mullvad (`IF_TYPE_PROP_VIRTUAL`) "Virtual", and 6to4, Teredo and IP-HTTPS "Tunnel". Of the adapters it does not list, four WAN Miniports read "Tunnel", and the PPPoE miniport and the RAS Async Adapter "PPP".
  - **Link speed:** reads "Not connected" for the Down Wi-Fi and Bluetooth adapters, which Windows reports at 144.4 Mbps and 3 Mbps.
  - **Connectivity:** the section showed what the collector read ("Internet access", "Unmetered", "No"), which agrees with the Internet connectivity and the profiles that `Get-NetConnectionProfile` reports.
  - **Layout:** with ordinary names, the window's minimum size is 600×490 at both window sizes, there is no horizontal scrolling, and no label is narrower than its text.
- **History:** `AdapterThroughputHistory` is the recording logic the Performance tab's network page had, moved unchanged, and the page behaves as before. Every adapter is recorded while the tab is hidden, and `showEvent()` catches up.
- **Card reuse:** `arrangeCards()` keeps each card by interface ID, so a renamed or moved adapter keeps its card and chart. Removed cards are hidden and released with `deleteLater()`.
- **Tick cost:** the Network tab measured 4.8–6.6 ms per tick for 2 adapters, 5.3–7.3 ms for 8 and 5.7–7.3 ms for 16 over four runs, against DC-98's 4.2, 5.7 and 5.7 ms, and 16 adapters cost about what 8 do, as DC-98 says. Every figure was well above DC-95's on both builds, although `build.ps1 -Benchmark` printed the same power plan and power mode as DC-95, probably because a game, a live wallpaper and a hardware monitor were running. The interleaved runs show that Phase 5 did not slow the Performance tab. Mean ms per tick:

  | Page and window | DC-95 (2026-10-15) | End of Phase 4 today, 3 runs | Phase 5 today, 3 runs |
  | --- | --- | --- | --- |
  | CPU, 16 cores, 1 min | 7.0–7.2 | 9.1–10.2 | 8.0–9.8 |
  | CPU, 64 cores, 1 min | 12.4–13.0 | 18.1–18.5 | 18.3–21.1 |
  | Memory, 1 min | 3.9–4.1 | 5.4–6.4 | 5.4–6.0 |
  | Disk, 30 min | 37.6–38.4 | 47.0–50.1 | 48.3–54.1 |
  | Network, 1 min | 4.6–4.7 | 6.3–7.2 | 7.0–7.7 |
  | Network, 30 min | 13.1–13.3 | 16.4–17.5 | 15.8–18.5 |

  The two builds' ranges overlap on every row. The network page's 1-minute window averaged 0.5 ms more on this phase's build, within the spread of either build's runs. Its code path changed only in ways that cost microseconds: the same history logic in a shared class, the type text, and the shared address helpers. DC-95's and DC-98's figures stand.
- **Logging:** connectivity changes are logged once at info, now with roaming. Failures of the name query are logged once at warn. Nothing is logged from a notification callback.
- **Known deviations:** Phase 5 adds no deviation, and none is open.
- **Rule files:** `AGENTS.md`, `GEMINI.md`, `copilot-instructions.md`, `monitoring/AGENTS.md`, `platform/windows/AGENTS.md` and `architecture.md` describe the network-name collector and the "store, then wake" rule in the same way.
