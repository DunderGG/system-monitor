# Verdandi — AI Agent Rules

You are working on Verdandi, a Windows desktop system monitor built with C++20 and Qt 6 Widgets.
The rules below are self-contained. For deeper rationale or design context, consult:

- [architecture.md](docs/architecture.md) — System design, threading model, Windows API choices, and module boundaries.
- [docs/build_system.md](docs/build_system.md) — Build prerequisites, CMake presets, vcpkg, and CI setup.
- [docs/coding_guidelines.md](docs/coding_guidelines.md) — Naming, formatting, error handling, memory management, and testing conventions.
- [docs/design_decisions.md](docs/design_decisions.md) — Rationale for non-obvious implementation choices made during development.
- [docs/performance.md](docs/performance.md) — Measured tick costs of each tab from `build.ps1 -Benchmark`, with the build and the machine they were measured on.
- [docs/known_deviations.md](docs/known_deviations.md) — Known places where the code departs from the architecture or guidelines, with plans to fix them.
- [roadmap.md](roadmap.md) — Phased implementation plan.

## Build & test commands

**Prerequisite:** Run `.\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment` once on a new machine to install git, CMake, Ninja, MSVC Build Tools with Visual Studio's C++ Clang tools, and vcpkg.

- Build: `.\scripts\build.ps1`
- Build and launch: `.\scripts\build.ps1 -Run`
- Build and test: `.\scripts\build.ps1 -Test`
- Release build, for measuring performance (Debug Qt paints many times slower): add `-Release`, e.g. `.\scripts\build.ps1 -Test -Release`
- Measure the Performance, Network and Processes tabs' tick cost (Release build; prints tables to record in [docs/performance.md](docs/performance.md); not a test): `.\scripts\build.ps1 -Benchmark`
- Coverage build (Debug, compiled with clang-cl for LLVM source-based coverage; needs Visual Studio's C++ Clang tools; runs the tests, prints the line and branch coverage of each directory under `src\` and in total, writes each file's annotated lines and branches to `build\coverage\html\index.html`, and writes a dashboard of the tests, the coverage and their trend over earlier runs to `build\coverage\index.html`): `.\scripts\build.ps1 -Coverage`
- CI's `Coverage` job runs the coverage build on each push to `main` and pull request, and publishes the dashboard of the runs on `main`, with their trend, at https://dundergg.github.io/verdandi/; a pull request's job summary compares its coverage with `main`'s ([DC-121](docs/design_decisions.md#dc-121)). It fails only when the build or a test fails, never because of coverage ([DC-122](docs/design_decisions.md#dc-122))
- Format C++ files and check the design decisions: `.\scripts\build.ps1 -Format` (run before committing; CI fails on unformatted code, a link that does not resolve, or an entry whose fields are missing or out of order)
- Check formatting and the design decisions only: `.\scripts\build.ps1 -CheckFormat`

Do **not** invoke `cmake` directly. `build.ps1` initializes the x64 MSVC developer environment automatically, which is required for the build to succeed regardless of which terminal the agent runs in. Calling the raw `cmake` commands without this initialization will fail with a broken compiler or linker error.

If `build.ps1` is missing something you need (a new option, clearer output, a fix for a failure mode), improve the script instead of working around it, and keep its comment-based help and these build commands in sync. Keep changes compatible with Windows PowerShell 5.1.

## Architecture rules

- **Module boundaries are strict.** The `ui` module never includes Windows headers or calls Windows APIs directly. The `domain` module has no Qt or Windows dependencies. All Windows API calls live in `platform/windows/`.
- **Data flows through immutable snapshots.** Collectors produce data on background threads. The UI receives `SystemSnapshot` values via `Qt::QueuedConnection`. No shared mutable state crosses thread boundaries.
- **Use the correct Windows APIs.** Use `NtQuerySystemInformation` for process enumeration, `GetSystemTimes` for CPU, `GlobalMemoryStatusEx` for memory (plus `GetPerformanceInfo` for cache, pool, and process/thread/handle counts), `GetIfTable2` for network, `GetNetworkConnectivityHint` for connectivity, `INetworkListManager` for network names (on the network-name collector's own worker thread, after a change, at most once a second), SetupAPI for which adapters Windows hides (`NCF_HIDDEN`, when an interface appears). Do not use PDH for core metrics. Do not use Tool Help for process enumeration. Do not poll WMI.
- **Process identity is `(PID, creation time)`.** Never use PID alone.

## Code style rules

- **C++20.** Use `std::jthread`, `std::stop_token`, `std::format`, `std::optional`, concepts, designated initializers.
- **Default member initializers.** A struct member that an initializer may leave out has a default member initializer (`{}` for a class type); the coverage build's clang-cl warns otherwise. Value-initialize a Windows SDK struct, then set its size member.
- **No exceptions.** Return `std::optional` or result structs for expected errors. Use assertions for programming bugs.
- **Naming:** `PascalCase` for types, `camelCase` for functions/variables, `m_` prefix for members, `k` prefix for constants, `snake_case` for file names. Getters omit `get` prefix. Namespaces are lowercase under `verdandi::`.
- **Formatting:** 4-space indentation, Allman braces for classes/functions, K&R braces for control flow. Always use braces. `Type* name` and `Type& name` (pointer and reference bind to the type). `.clang-format` is the source of truth; run `.\scripts\build.ps1 -Format`.
- **Memory:** RAII everywhere. `std::unique_ptr` for single ownership. Qt parent-child for widgets. Never mix smart pointers with Qt parent ownership. Wrap all Windows handles in RAII types.
- **Includes:** In `.h` files: standard library, then Windows SDK (`platform/windows` only), then third-party, then project headers — each group separated by a blank line. In `.cpp` files: own header first (to verify self-containment), then the same groups. Windows SDK headers keep the order the SDK requires (`winsock2.h`, `ws2tcpip.h`, `windows.h`, then others); `.clang-format` enforces it. Use `#pragma once`. Never use `using namespace` in headers. `using namespace std;` is banned everywhere; a file's own project namespace (e.g. `using namespace verdandi::monitoring;`) is allowed only in `.cpp` files.

## Threading rules

- Use `std::jthread` with `std::stop_token` for all background threads the application creates.
- Fast collectors (CPU, memory, network counters) run on the scheduler thread.
- Slow collectors (process enumeration, connectivity, network names) run on the scheduler's slow-collector thread at their own cadence. The network-name collector makes its COM query on a worker thread of its own, so that a stuck service cannot stall the others; its `collect()` only copies the latest result.
- Event-driven collectors may receive OS callbacks on OS thread-pool threads (e.g. `NotifyNetworkConnectivityHintChange`) only under the rules in [architecture.md — Event-driven collectors](docs/architecture.md#event-driven-collectors-os-callback-threads): store, then wake the scheduler; cancel before teardown; seed synchronously; read from the cache.
- Never call widget methods from a non-UI thread.
- Use `std::mutex` with `std::lock_guard` or `std::scoped_lock`. Never lock manually.

## Windows API rules

- Define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` project-wide.
- Use `W` (wide/Unicode) API variants, never `A` (ANSI).
- Only request `PROCESS_QUERY_LIMITED_INFORMATION` when opening process handles. The one exception is process termination (`terminateOwnedProcess()` in `platform/windows/process_terminator.cpp`), which also requests `SYNCHRONIZE`, and adds `PROCESS_TERMINATE` only after confirming that the current user owns the process and that it does not run elevated.
- Never call `OpenProcess` on `lsass.exe` or `csrss.exe`.
- Convert Windows types to domain types at the `platform/windows` boundary.

## Testing rules

- Use GoogleTest. Name tests `TypeName_Scenario_ExpectedResult`.
- Use `EXPECT_*` by default, `ASSERT_*` only when the test is meaningless without the assertion.
- Use fake collectors, not mocking frameworks.
- Unit tests must be fast and deterministic with no OS dependencies. Integration tests may use real Windows APIs but must tolerate varying environments.
- Test layout: `tests/unit/` never includes Windows headers. Unit tests of `platform/windows` code that need Windows SDK types or constants (e.g. mapping functions) go in `tests/platform/` (the separate `verdandi_platform_tests` target); they may include SDK headers but must not call Windows APIs. `tests/integration/` may include Windows headers and call Windows APIs.
- Builders, fakes and harnesses that more than one test file uses live in `tests/support/` (headers in `verdandi::tests`, no Windows headers). Look there before writing one, and move a helper there when a second file needs it instead of copying it.
- Test each rule once, at the layer that owns it (a mapping at its mapping function, an order at its comparator, wording at its formatter); tests of callers check the wiring. View tests compare text with the formatter's output instead of repeating it.
- Test a mapping's or a threshold's values in one test, as a table of named cases with `SCOPED_TRACE`. Do not test what the compiler guarantees, such as aggregate initialization or distinct enumerators.
- When you merge or remove a test, name in the commit message the test that still covers its behaviour.

## Logging

- Use spdlog, not `qDebug()`, `std::cout`, or `printf`.

## Design decisions

- When you make a non-obvious implementation choice (API selection, data representation, threading approach, CMake target type, etc.), document it in [docs/design_decisions.md](docs/design_decisions.md) at the end of its topic's section ([DC-116](docs/design_decisions.md#dc-116)), with the next unused `DC-xx` ID and an `<a id="dc-xx"></a>` anchor on the line above its heading, and add it to the table of contents before finishing the task. Link to an entry by that anchor (`design_decisions.md#dc-xx`), never by its heading.
- Write an entry as **Decision:** (what was chosen), **Rationale:** (why, and the alternatives rejected), **Consequences:** (optional: a cost or a constraint on later changes that the Rationale leaves out), and last a one-line **History:** (the phase or code-review finding that made it), each at the start of a line. Keep it to about 250 words, not counting the heading and the History line, and give each part of a merged entry a sentence. Leave out test lists, benchmark tables, UI text copied from the code, and any measurement beyond the one that justifies the choice: the tests, [docs/performance.md](docs/performance.md) and the formatters hold them ([DC-117](docs/design_decisions.md#dc-117)).
- When a change alters a decision that already has an entry, rewrite that entry so that it states the decision as it now stands, and add the change to its **History:** line. Do not add a note such as "Superseded in part by" or "Extended by", or a new entry that patches the old one. Never reuse, renumber or delete an ID: an entry merged into another or retired stays as a one-line stub, and links and citations move to the entry that now has its decision ([DC-115](docs/design_decisions.md#dc-115)).
- If a change knowingly deviates from [architecture.md](docs/architecture.md), [coding_guidelines.md](docs/coding_guidelines.md), or these rules, record it in [docs/known_deviations.md](docs/known_deviations.md) with a new ID, reference that ID in a comment at the deviating code, and tell the user. Check that list before working in an affected area, and mark entries Resolved when you fix them.

## What NOT to do

- Do not add exceptions.
- Do not use `new`/`delete` directly (use smart pointers or Qt parent ownership).
- Do not add PDH for core metrics (CPU, memory, process data).
- Do not use `CreateToolhelp32Snapshot` for process enumeration.
- Do not use `using namespace std;` (anywhere). Do not use `using namespace` in headers at all.
- Do not use `volatile` for threading.
- Do not use C-style casts.
- Do not use `std::bind` (use lambdas).
- Do not commit commented-out code.

