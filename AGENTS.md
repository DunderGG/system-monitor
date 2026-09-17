# System Monitor — AI Agent Rules

You are working on a Windows desktop system monitor built with C++20 and Qt 6 Widgets.
The rules below are self-contained. For deeper rationale or design context, consult:

- [architecture.md](architecture.md) — System design, threading model, Windows API choices, and module boundaries.
- [docs/coding_guidelines.md](docs/coding_guidelines.md) — Naming, formatting, error handling, memory management, and testing conventions.
- [roadmap.md](roadmap.md) — Phased implementation plan.

## Build & test commands

- Configure: `cmake --preset default`
- Build: `cmake --build --preset default`
- Test: `ctest --preset default --output-on-failure`

## Architecture rules

- **Module boundaries are strict.** The `ui` module never includes Windows headers or calls Windows APIs directly. The `domain` module has no Qt or Windows dependencies. All Windows API calls live in `platform/windows/`.
- **Data flows through immutable snapshots.** Collectors produce data on background threads. The UI receives `SystemSnapshot` values via `Qt::QueuedConnection`. No shared mutable state crosses thread boundaries.
- **Use the correct Windows APIs.** Use `NtQuerySystemInformation` for process enumeration, `GetSystemTimes` for CPU, `GlobalMemoryStatusEx` for memory, `GetIfTable2` for network, `GetNetworkConnectivityHint` for connectivity. Do not use PDH for core metrics. Do not use Tool Help for process enumeration. Do not poll WMI.
- **Process identity is `(PID, creation time)`.** Never use PID alone.

## Code style rules

- **C++20.** Use `std::jthread`, `std::stop_token`, `std::format`, `std::optional`, concepts, designated initializers.
- **No exceptions.** Return `std::optional` or result structs for expected errors. Use assertions for programming bugs.
- **Naming:** `PascalCase` for types, `camelCase` for functions/variables, `m_` prefix for members, `k` prefix for constants, `snake_case` for file names. Getters omit `get` prefix. Namespaces are lowercase under `sysmon::`.
- **Formatting:** 4-space indentation, Allman braces for classes/functions, K&R braces for control flow. Always use braces.
- **Memory:** RAII everywhere. `std::unique_ptr` for single ownership. Qt parent-child for widgets. Never mix smart pointers with Qt parent ownership. Wrap all Windows handles in RAII types.
- **Includes:** Own header first, then standard library, then third-party, then project headers. Separate groups with blank lines. Use `#pragma once`.

## Threading rules

- Use `std::jthread` with `std::stop_token` for all background threads.
- Fast collectors (CPU, memory, network counters) run on the scheduler thread.
- Slow collectors (process enumeration, connectivity) run on separate threads at their own cadence.
- Never call widget methods from a non-UI thread.
- Use `std::mutex` with `std::lock_guard` or `std::scoped_lock`. Never lock manually.

## Windows API rules

- Define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` project-wide.
- Use `W` (wide/Unicode) API variants, never `A` (ANSI).
- Only request `PROCESS_QUERY_LIMITED_INFORMATION` when opening process handles.
- Never call `OpenProcess` on `lsass.exe` or `csrss.exe`.
- Convert Windows types to domain types at the `platform/windows` boundary.

## Testing rules

- Use GoogleTest. Name tests `TypeName_Scenario_ExpectedResult`.
- Use `EXPECT_*` by default, `ASSERT_*` only when the test is meaningless without the assertion.
- Use fake collectors, not mocking frameworks.
- Unit tests must be fast and deterministic with no OS dependencies. Integration tests may use real Windows APIs but must tolerate varying environments.

## Logging

- Use spdlog, not `qDebug()`, `std::cout`, or `printf`.

## What NOT to do

- Do not add exceptions.
- Do not use `new`/`delete` directly (use smart pointers or Qt parent ownership).
- Do not add PDH for core metrics (CPU, memory, process data).
- Do not use `CreateToolhelp32Snapshot` for process enumeration.
- Do not use `using namespace std;`.
- Do not use `volatile` for threading.
- Do not use C-style casts.
- Do not use `std::bind` (use lambdas).
- Do not commit commented-out code.

