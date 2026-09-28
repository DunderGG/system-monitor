# Known Deviations

Places where the code knowingly departs from [architecture.md](architecture.md), [coding_guidelines.md](coding_guidelines.md), or [AGENTS.md](../AGENTS.md). Each entry is a debt to pay down, not a precedent to copy.

When you introduce a deviation, add an entry here and reference its ID in a code comment at the deviating site. When you resolve one, set its status to **Resolved** with the date and what changed, rather than deleting it. The phase code review in [roadmap.md](../roadmap.md) should check this list.

| ID | Summary | Severity | Status |
| --- | --- | --- | --- |
| [D-1](#d-1-sampling-scheduler-has-no-slow-collector-thread) | Sampling scheduler has no slow-collector thread | Medium | Open — planned for Phase 4 |
| [D-2](#d-2-connectivity-fallback-polls-on-the-scheduler-thread) | Connectivity fallback polls on the scheduler thread | Low | Open — blocked on D-1 |
| [D-3](#d-3-connectivity-updates-arrive-on-an-os-thread-pool-thread) | Connectivity updates arrive on an OS thread-pool thread | Low | Open — needs architecture decision |
| [D-4](#d-4-a-unit-test-includes-windows-sdk-headers) | A unit test includes Windows SDK headers | Low | Open — needs guideline decision |
| [D-5](#d-5-notification-callback-wiring-has-no-unit-test) | Notification callback wiring has no unit test | Low | Open |
| [D-6](#d-6-network-throughput-reports-zero-when-no-baseline-exists) | Network throughput reports zero when no baseline exists | Low | Open |
| [D-7](#d-7-memory-and-cpu-data-report-zeros-when-missing) | Memory and CPU data report zeros when missing | Medium | Open |

---

### D-1: Sampling scheduler has no slow-collector thread

**Rule:** Slow collectors (process enumeration, connectivity) run on separate `std::jthread`s at their own cadence; the scheduler merges their latest result. ([architecture.md — Threading model](architecture.md#threading-model), [AGENTS.md — Threading rules](../AGENTS.md#threading-rules))

**Current state:** `SamplingScheduler::sampleOnce()` calls every registered collector inline on the scheduler tick, including `IConnectivityCollector` and `IProcessCollector`. There is no slow-collector thread or mutex-protected merge. This predates the connectivity collector.

**Files:** `src/monitoring/sampling_scheduler.cpp` — `sampleOnce()`

**Plan:** Build the slow-collector thread and merge in Phase 4 ([roadmap: "Run the process collector on a slow-collector thread"](../roadmap.md#process-collector-srcplatformwindows)), and move connectivity onto it at the same time.

---

### D-2: Connectivity fallback polls on the scheduler thread

**Rule:** Same as D-1.

**Current state:** `ConnectivityCollector` normally caches the status delivered by `NotifyNetworkConnectivityHintChange`, so `collect()` on the scheduler thread only copies a value under a mutex. If registration fails at runtime, it falls back to calling `GetNetworkConnectivityHint` synchronously inside every `collect()`, which puts an OS call on the scheduler tick.

**Files:** `src/platform/windows/connectivity_collector.cpp` — constructor (fallback branch)

**Plan:** Once D-1 is resolved, run the fallback poll on the slow-collector thread at its own cadence. Blocked on D-1.

---

### D-3: Connectivity updates arrive on an OS thread-pool thread

**Rule:** Use `std::jthread` with `std::stop_token` for all background threads. ([AGENTS.md — Threading rules](../AGENTS.md#threading-rules), [coding_guidelines.md — Threading](coding_guidelines.md#threading))

**Current state:** `NotifyNetworkConnectivityHintChange` invokes its callback on a Windows thread-pool thread that the application does not own. The callback only writes a mutex-protected value, and `CancelMibChangeNotify2` (via RAII) waits for in-flight callbacks before the collector is destroyed. Neither document describes event-driven collectors fed by OS callbacks, so this pattern is outside the documented threading model.

**Files:** `src/platform/windows/connectivity_collector.cpp` — `NotificationBridge`, `NotificationHandleDeleter`

**Plan:** Decide whether OS-owned callback threads are acceptable for event-driven collectors. If they are, document the pattern and its rules (callbacks only touch mutex-protected state, and are cancelled before teardown) in the architecture threading model. If not, forward notifications to the slow-collector thread from D-1.

---

### D-4: A unit test includes Windows SDK headers

**Rule:** All Windows API calls must be isolated in `src/platform/windows/`; no other module includes Windows headers. Unit tests must have no OS dependencies. ([coding_guidelines.md — Windows API wrapping](coding_guidelines.md#windows-api-wrapping), [AGENTS.md — Testing rules](../AGENTS.md#testing-rules), [platform/windows/AGENTS.md](../src/platform/windows/AGENTS.md): "Never expose Windows types outside this module")

**Current state:** `tests/unit/connectivity_hint_mapping_test.cpp` includes `platform/windows/connectivity_hint_mapping.h`, which exposes Windows SDK types (`NL_NETWORK_CONNECTIVITY_HINT`) so the Windows-to-domain mapping can be tested directly. The test makes no OS calls, so this is a compile-time dependency only.

**Files:** `src/platform/windows/connectivity_hint_mapping.h`, `tests/unit/connectivity_hint_mapping_test.cpp`

**Plan:** Either clarify the guidelines to allow unit tests of `platform/windows` mapping code to include SDK headers (no API calls), or move these tests to a platform-specific test target.

---

### D-5: Notification callback wiring has no unit test

**Rule:** Include regression tests for network state transitions. ([architecture.md — Testing strategy](architecture.md#testing-strategy))

**Current state:** State transitions are unit tested through the injected `ConnectivityReader`, and the mapping is unit tested directly. The path from the OS callback (`NotificationBridge::onConnectivityChange`) into the cache is covered only by integration tests, which cannot force a connectivity change.

**Files:** `src/platform/windows/connectivity_collector.cpp`, `tests/integration/connectivity_collector_test.cpp`

**Plan:** Add a seam that lets a test deliver a notification (for example an injectable subscription function) if this path gains more logic than "map, then store".

---

### D-6: Network throughput reports zero when no baseline exists

**Rule:** Never substitute zero or a default for missing data; represent it with `std::optional` or an error variant. ([coding_guidelines.md — Error handling](coding_guidelines.md#error-handling), [architecture.md — Domain design guidelines](architecture.md#domain-design-guidelines))

**Current state:** On the first sample for an adapter there is no baseline to compute a rate from, and `NetworkCollector` reports `inBytesPerSec` / `outBytesPerSec` as `0`, which the UI cannot tell apart from an idle link. This predates the connectivity work and was found while reviewing it; the other collectors have not yet been audited for the same pattern.

**Files:** `src/platform/windows/network_collector.cpp` — `calculateNetworkSamples()`, `src/domain/network_sample.h`

**Plan:** Make the rate fields `std::optional<uint64_t>` (or add a validity flag), and check the CPU collector's first-sample behaviour for the same issue during the Phase 2 code review.

---

### D-7: Memory and CPU data report zeros when missing

**Rule:** Represent missing or inaccessible data with `std::optional` or error variants, never as zero or a default value. ([domain/AGENTS.md](../src/domain/AGENTS.md), [coding_guidelines.md — Error handling](coding_guidelines.md#error-handling))

**Current state:** `SystemSnapshot::cpu` and `SystemSnapshot::memory` are not optional, so when no CPU or memory collector is registered the snapshot carries all-zero samples (asserted by the `SampleOnce_MissingCollectorsProduceDefaults` scheduler test). `MemoryCollector::collect()` also returns a default, all-zero `MemorySample` when `GlobalMemoryStatusEx` fails. The UI cannot distinguish either case from real readings. Whether `CpuCollector` does the same on failure has not been audited.

**Files:** `src/domain/system_snapshot.h`, `src/platform/windows/memory_collector.cpp` — `collect()`, `tests/unit/sampling_scheduler_test.cpp`

**Plan:** Make the `cpu` and `memory` snapshot fields `std::optional` (as `uptime` already is) and return `std::nullopt` on collector failure. Audit `CpuCollector` at the same time. Best done before the dashboard cards consume these fields.
