# Code Review — Phase 1 Completion

**Date:** 2026-09-22
**Scope:** All 46 files across `src/`, `tests/`, `scripts/`, CMake configuration, vcpkg manifests, CI, and documentation.
**Reviewed against:** [architecture.md](architecture.md), [coding_guidelines.md](coding_guidelines.md), [design_decisions.md](design_decisions.md), [roadmap.md](../roadmap.md), [AGENTS.md](../AGENTS.md)

---

## Overall Assessment

The codebase is in excellent shape. Phase 0 and Phase 1 are substantially complete, and the code follows the documented guidelines with very high fidelity. The architecture is clean, module boundaries are respected, naming conventions are consistent, and the testing foundation is solid. The findings below are refinements, not fundamental problems.

### Roadmap Progress

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 0** — Project scaffolding | ✅ Complete | All items checked off |
| **Phase 1** — Domain types & synthetic pipeline | ✅ Complete | This review was the final unchecked item |
| **Phase 2** — Real Windows collectors & dashboard | ⬜ Not started | No `platform/windows/` code exists yet |
| **Phase 3–6** — Charts, processes, network, settings | ⬜ Not started | Placeholder views in place |

### Compliance Scorecard

| Category | Score | Notes |
|----------|-------|-------|
| Architecture compliance | ⭐⭐⭐⭐⭐ | Module boundaries, threading model, snapshot data flow all exactly as designed |
| Naming conventions | ⭐⭐⭐⭐⭐ | 100% consistent across all files |
| Formatting | ⭐⭐⭐⭐⭐ | Allman/K&R brace mix, 4-space indentation, all correct |
| C++20 usage | ⭐⭐⭐⭐⭐ | jthread, stop_token, concepts, designated init, span, optional, nodiscard |
| Error handling | ⭐⭐⭐⭐⭐ | No exceptions, optional for missing data, assertions for bugs |
| Memory management | ⭐⭐⭐⭐⭐ | unique_ptr, Qt parent-child, no raw new/delete outside Qt |
| Threading | ⭐⭐⭐⭐⭐ | jthread+stop_token, mutex+lock_guard, queued connections |
| Testing | ⭐⭐⭐⭐½ | Comprehensive for Phase 1; minor naming inconsistency |
| Build system | ⭐⭐⭐⭐⭐ | CMake presets, vcpkg manifest, CI, bootstrap/build scripts |
| Documentation | ⭐⭐⭐⭐⭐ | Architecture, guidelines, design decisions all thorough |

---

## Findings

### F-1: `SamplingScheduler::sampleOnce()` holds collector mutex during all collections

**Severity:** Low
**File:** `src/monitoring/sampling_scheduler.cpp` — `sampleOnce()`

**Description:**

The `m_collectorMutex` is acquired once at the top of `sampleOnce()` and held for the duration of every `collect()` call:

```cpp
domain::SystemSnapshot SamplingScheduler::sampleOnce()
{
    domain::SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();

    std::lock_guard lock(m_collectorMutex);  // held for ALL collector calls
    if (m_cpuCollector) {
        snapshot.cpu = m_cpuCollector->collect();
    }
    if (m_memoryCollector) {
        snapshot.memory = m_memoryCollector->collect();
    }
    // ... all remaining collectors under the same lock
}
```

Currently this is correct because synthetic collectors return instantly. However, when real Windows API collectors are wired in (Phase 2), `setCpuCollector()` and similar methods will block for the full duration of all collections combined, since they also acquire `m_collectorMutex`.

**Proposed Solution:**

Snapshot the collector pointers under the lock, then call them outside:

```cpp
domain::SystemSnapshot SamplingScheduler::sampleOnce()
{
    domain::SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();

    // Snapshot the collector pointers under the lock
    ICpuCollector* cpu = nullptr;
    IMemoryCollector* memory = nullptr;
    // ... etc
    {
        std::lock_guard lock(m_collectorMutex);
        cpu = m_cpuCollector.get();
        memory = m_memoryCollector.get();
        // ...
    }

    // Call collectors outside the lock
    if (cpu) {
        snapshot.cpu = cpu->collect();
    }
    if (memory) {
        snapshot.memory = memory->collect();
    }
    // ...

    return snapshot;
}
```

This relies on the invariant that collectors are only swapped while the scheduler is stopped. If hot-swapping collectors during operation is needed, a `std::shared_ptr` approach or a separate synchronization scheme would be required.

**Recommendation:** Defer this change until Phase 2 when real collectors are introduced. Document the invariant that `set*Collector()` should only be called before `start()` or after `stop()`.

**Status:** Resolved (2026-09-23) — Implemented pointer snapshotting under `m_collectorMutex` in `sampleOnce()`, documented stopped-state registration invariant in `sampling_scheduler.h`, and added `assert(!m_isRunning.load())` in setters. Tested via `SampleOnce_ReleasesCollectorMutexBeforeCollection`.

---

### F-2: `RingBuffer` move-push copies before moving

**Severity:** Low
**File:** `src/monitoring/ring_buffer.h` — `push(T&& sample)`

**Description:**

The move overload writes to two storage slots (primary and mirror for the O(1) contiguous span trick). The first write is a copy, the second is a move:

```cpp
void push(T&& sample)
{
    const std::size_t pos = m_head;
    m_storage[pos] = sample;                        // copy (sample is an lvalue)
    m_storage[pos + m_capacity] = std::move(sample); // move
    // ...
}
```

Since `sample` is a named variable (an lvalue reference to an rvalue reference), the first assignment copies instead of moving. After the `std::move` on the second line, `sample` is in a moved-from state, so we can't reverse the order.

**Proposed Solution:**

This is inherent to the mirrored-storage design — you need the data in two slots, so you can only move into one of them. The current implementation (copy + move) is functionally correct and already better than the const-ref overload (copy + copy) for types where moving is cheaper than copying (e.g., `std::string`, `std::vector`).

No code change needed. Add a brief comment explaining why:

```cpp
void push(T&& sample)
{
    const std::size_t pos = m_head;
    // Copy into primary slot, then move into mirror. Both slots must hold
    // the value for the contiguous-span trick, so we can't avoid one copy.
    m_storage[pos] = sample;
    m_storage[pos + m_capacity] = std::move(sample);
    // ...
}
```

**Recommendation:** Add the clarifying comment. No functional change required.

**Status:** Resolved (2026-09-23) — Added clarifying comment to `push(T&&)` in `src/monitoring/ring_buffer.h` explaining the unavoidable 1-copy + 1-move trade-off inherent to mirrored storage for zero-copy contiguous span access.

---

### F-3: Test naming uses non-standard suite/test name pattern

**Severity:** Cosmetic
**File:** `tests/unit/domain_types_test.cpp`

**Description:**

The coding guidelines specify the pattern `TypeName_Scenario_ExpectedResult`. The GTest `TEST` macro takes two parameters: `TEST(TestSuiteName, TestName)`. Several domain tests encode the full pattern in the suite name and repeat part of it as the test name:

```cpp
TEST(CpuSample_DefaultConstruction_ZeroValues, DefaultValues)   // redundant
TEST(MemorySample_DesignatedInit_FieldsRoundTrip, DesignatedInit) // redundant
```

The conventional application of the guideline would be:

```cpp
TEST(CpuSample, DefaultConstruction_ZeroValues)
TEST(MemorySample, DesignatedInit_FieldsRoundTrip)
```

This matters because GTest formats output as `TestSuite.TestName`, and the current pattern produces verbose output like `CpuSample_DefaultConstruction_ZeroValues.DefaultValues`.

Note: tests in other files (`ring_buffer_test.cpp`, `sampling_scheduler_test.cpp`, `main_window_test.cpp`) already follow the correct convention with the type as the suite name.

**Proposed Solution:**

Refactor tests in `domain_types_test.cpp` to use the type name as the test suite:

```cpp
TEST(CpuSample, DefaultConstruction_ZeroValues) { ... }
TEST(CpuSample, DesignatedInit_FieldsRoundTrip) { ... }
TEST(MemorySample, DefaultConstruction_ZeroValues) { ... }
TEST(MemorySample, DesignatedInit_FieldsRoundTrip) { ... }
// ... and so on for all domain type tests
```

**Recommendation:** Fix when convenient. Low priority.

---

### F-4: `MainWindow::onSnapshotReady` only updates `DashboardView`

**Severity:** Not a bug (expected for Phase 1)
**File:** `src/ui/main_window.cpp` — `onSnapshotReady()`

**Description:**

```cpp
void MainWindow::onSnapshotReady(const sysmon::domain::SystemSnapshot& snapshot)
{
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
}
```

Only the dashboard receives snapshot updates. The other three views (Performance, Processes, Network) are placeholders with no `updateSnapshot` method yet.

**Proposed Solution (for Phase 2+):**

When the other views become functional, extend this method to forward the snapshot:

```cpp
void MainWindow::onSnapshotReady(const sysmon::domain::SystemSnapshot& snapshot)
{
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
    if (m_performanceView != nullptr) {
        m_performanceView->updateSnapshot(snapshot);
    }
    if (m_processesView != nullptr) {
        m_processesView->updateSnapshot(snapshot);
    }
    if (m_networkView != nullptr) {
        m_networkView->updateSnapshot(snapshot);
    }
}
```

Alternatively, consider an optimization where only the currently visible tab's view is updated, since updating invisible views wastes CPU:

```cpp
void MainWindow::onSnapshotReady(const sysmon::domain::SystemSnapshot& snapshot)
{
    auto* activeView = m_tabWidget->currentWidget();
    // Always update dashboard for tray icon / system health status
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
    // Only update the active non-dashboard tab
    if (activeView == m_performanceView) {
        m_performanceView->updateSnapshot(snapshot);
    } else if (activeView == m_processesView) {
        m_processesView->updateSnapshot(snapshot);
    }
    // ... etc.
}
```

**Recommendation:** Decide the approach when implementing Phase 2. No action needed now.

---

### F-5: `m_interval` read without lock in log line

**Severity:** Very low
**File:** `src/monitoring/sampling_scheduler.cpp` — `run()`

**Description:**

```cpp
void SamplingScheduler::run(std::stop_token stopToken)
{
    spdlog::info("SamplingScheduler background thread started (interval: {}ms)",
                 m_interval.count());  // ← no lock on m_sleepMutex
    // ...
}
```

`m_interval` is protected by `m_sleepMutex` in `setInterval()` and `interval()`, but this log line reads it without holding the mutex. This is technically a data race under the C++ memory model if `setInterval()` is called concurrently.

In practice, `start()` is called from the main thread and `setInterval()` is unlikely to be called before the background thread reaches this line, so this is a theoretical TSAN issue, not a real bug.

**Proposed Solution:**

Read the interval under the lock:

```cpp
void SamplingScheduler::run(std::stop_token stopToken)
{
    {
        std::lock_guard lock(m_sleepMutex);
        spdlog::info("SamplingScheduler background thread started (interval: {}ms)",
                     m_interval.count());
    }
    // ...
}
```

Or cache the value:

```cpp
void SamplingScheduler::run(std::stop_token stopToken)
{
    const auto startInterval = [&] {
        std::lock_guard lock(m_sleepMutex);
        return m_interval;
    }();
    spdlog::info("SamplingScheduler background thread started (interval: {}ms)",
                 startInterval.count());
    // ...
}
```

**Recommendation:** Fix with the simpler lock approach. Minimal effort, eliminates a potential TSAN report.

---

### F-6: Domain string types use `std::string` — encoding decision undocumented

**Severity:** Design decision
**Files:** `src/domain/disk_sample.h`, `src/domain/network_sample.h`, `src/domain/process_info.h`

**Description:**

The architecture mandates "Use `W` (wide/Unicode) API variants" and the domain layer must remain platform-independent. The domain types use `std::string`:

- `DiskSample::volumeName` — `std::string`
- `NetworkSample::adapterName` — `std::string`
- `ProcessInfo::imageName` — `std::string`
- `ProcessInfo::imagePath` — `std::optional<std::string>`
- `ProcessInfo::commandLine` — `std::optional<std::string>`

Windows APIs return wide strings (`std::wstring` / `WCHAR*`). Using `std::string` in the domain implies a UTF-16 → UTF-8 conversion at the `platform/windows` boundary.

This is a defensible choice: UTF-8 is the standard encoding for cross-platform C++ code, Qt's `QString` handles UTF-8 natively, and the domain layer stays platform-agnostic. However, Windows paths can contain characters in supplementary planes (e.g., emoji in filenames) that require careful handling during conversion.

**Proposed Solution:**

No code change needed. Document the decision in `design_decisions.md`:

```markdown
### Domain string encoding: UTF-8 (`std::string`) over wide strings (`std::wstring`)

**Decision:** All string fields in domain types use `std::string` with UTF-8 encoding.
Wide-to-UTF-8 conversion happens at the `platform/windows` boundary.

**Rationale:** The domain layer must be platform-independent. `std::wstring` is a
Windows-specific convention (16-bit code units); using it would leak a platform
assumption into `src/domain/`. UTF-8 is the de facto standard encoding for
cross-platform C++ and is natively supported by Qt (`QString::fromUtf8`), spdlog,
and nlohmann/json. The Windows API wrapper functions in `platform/windows/` convert
from `WCHAR*` / `std::wstring` to UTF-8 `std::string` using
`WideCharToMultiByte(CP_UTF8, ...)` or equivalent. Supplementary-plane characters
(e.g., emoji in filenames) round-trip correctly through UTF-8.
```

**Recommendation:** Add the design decision entry. No code change.

---

### F-7: No `.clang-format` configuration file

**Severity:** Low
**Files:** Project root

**Description:**

The `.editorconfig` enforces basic whitespace rules (4-space indentation, UTF-8, trailing whitespace trimming), but there is no `.clang-format` file to enforce the specific formatting rules from `coding_guidelines.md`:

- Allman braces for classes, functions, namespaces
- K&R braces for control flow (`if`, `for`, `while`, `switch`)
- Include ordering (standard → third-party → project)
- Spacing rules

The current codebase is consistently formatted because it was written carefully, but without automated enforcement, formatting drift is possible as the codebase grows and more contributors participate.

**Proposed Solution:**

Add a `.clang-format` file to the project root. The mixed brace style (Allman + K&R) is achievable with clang-format 14+ using `BreakBeforeBraces: Custom` and `BraceWrapping`:

```yaml
---
Language: Cpp
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
AccessModifierOffset: -4

BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: true
  AfterFunction: true
  AfterNamespace: true
  AfterStruct: true
  AfterEnum: true
  AfterControlStatement: Never
  BeforeElse: false
  BeforeCatch: false
  SplitEmptyFunction: false
  SplitEmptyRecord: false
  SplitEmptyNamespace: false

AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false

IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<[a-z_]+>'        # Standard library
    Priority: 1
  - Regex: '^<Q'               # Qt
    Priority: 2
  - Regex: '^<spdlog/'         # spdlog
    Priority: 2
  - Regex: '^<nlohmann/'       # nlohmann
    Priority: 2
  - Regex: '^"'                # Project headers
    Priority: 3

SortIncludes: CaseInsensitive
InsertBraces: true
SpaceBeforeParens: ControlStatements
```

**Recommendation:** Add a `.clang-format` file. Validate it against the existing codebase with `clang-format --dry-run` before committing to ensure it doesn't reformat existing code unexpectedly. This can be done independently of any phase.

---

### F-8: `placeholder_test.cpp` can be removed

**Severity:** Cosmetic
**File:** `tests/unit/placeholder_test.cpp`

**Description:**

This file was created during Phase 0 to verify the GoogleTest pipeline:

```cpp
TEST(TestPipeline, Placeholder_AlwaysPasses)
{
    EXPECT_TRUE(true);
}
```

There are now 25+ real tests across four test files. The placeholder no longer serves a purpose.

**Proposed Solution:**

1. Delete `tests/unit/placeholder_test.cpp`.
2. Remove the `unit/placeholder_test.cpp` entry from `tests/CMakeLists.txt`.

**Recommendation:** Remove when convenient. Trivial change.

---

## Items Verified — No Issues Found

The following areas were specifically checked and found to be fully compliant:

- **Module boundaries:** `ui/` has zero Windows headers; `domain/` has zero Qt or Windows dependencies; all Windows API usage isolated in `platform/windows/` (currently empty, as expected pre-Phase 2).
- **Immutable snapshot data flow:** `SystemSnapshot` is a value type. Built on the scheduler thread, emitted via `Qt::QueuedConnection`. No shared mutable state crosses thread boundaries.
- **`Q_DECLARE_METATYPE` + `qRegisterMetaType`** for `SystemSnapshot` — correctly configured for queued signal transport.
- **Qt signal/slot syntax:** Pointer-to-member-function syntax used everywhere (compile-time type-safe). No string-based `SIGNAL()`/`SLOT()`.
- **Qt parent-child ownership:** All widgets created with `new QLabel("...", this)` use parent ownership. No mixing of smart pointers with Qt parent-child.
- **`WIN32_LEAN_AND_MEAN` and `NOMINMAX`** defined project-wide in the root `CMakeLists.txt`.
- **spdlog used consistently** — no `qDebug()`, `std::cout`, or `printf` anywhere.
- **No exceptions, no C-style casts, no `std::bind`, no `volatile`, no `using namespace std;`, no `using namespace` in headers, no commented-out code.**
- **CMake `INTERFACE` library** for `sysmon_domain` — correctly documented in `design_decisions.md`.
- **vcpkg baseline** in `vcpkg-configuration.json` matches the CI workflow's `vcpkgGitCommitId`.
- **`test_main.cpp`** handles `--gtest_list_tests` without requiring a display — smart design for CI and IDE integration.

