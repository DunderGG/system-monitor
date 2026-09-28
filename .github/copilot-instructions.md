# GitHub Copilot Instructions — System Monitor

This is a Windows desktop system monitor built with C++20 and Qt 6 Widgets.
The rules below are self-contained. For deeper context, see `docs/architecture.md`, `docs/coding_guidelines.md`, or `AGENTS.md`. Known departures from these rules are tracked in `docs/known_deviations.md`; record any new deviation there.

## Build & test commands

- Build: `.\scripts\build.ps1 -NoRun`
- Build and launch: `.\scripts\build.ps1`
- Build and test: `.\scripts\build.ps1 -NoRun -Test`
- Format C++ files: `.\scripts\build.ps1 -Format` (run before committing; CI fails on unformatted code)
- Check formatting only: `.\scripts\build.ps1 -CheckFormat`

Do not invoke `cmake` or `ctest` directly: `build.ps1` initializes the x64 MSVC developer environment the build requires. If the script is missing something you need, improve it instead of working around it, and keep its help and these commands in sync.

## Critical rules

### Architecture
- Module boundaries are strict: `ui/` has no Windows headers, `domain/` has no Qt or Windows headers, `platform/windows/` has no Qt headers. Among tests, only `tests/platform/` (SDK types, no API calls) and `tests/integration/` may include Windows headers.
- Data flows through immutable `SystemSnapshot` values via `Qt::QueuedConnection`.
- Use `NtQuerySystemInformation` for processes, `GetSystemTimes` for CPU, `GlobalMemoryStatusEx` for memory, `GetIfTable2` for network, `GetNetworkConnectivityHint` for connectivity.
- Do NOT use PDH for core metrics, Tool Help for processes, or WMI for polling.

### Code style
- No exceptions. Return `std::optional` or result structs.
- `PascalCase` types, `camelCase` functions/variables, `m_` members, `k` constants, `snake_case` files.
- Allman braces for classes/functions, K&R for control flow. Always use braces. 4-space indent. `Type* name`, `Type& name`.
- `std::jthread` + `std::stop_token` for threads the app creates; OS notification callbacks only store into mutex-protected state and are cancelled via RAII before teardown (see `docs/architecture.md`). RAII for all resources.
- spdlog for logging, not qDebug or cout.

### Security
- Only `PROCESS_QUERY_LIMITED_INFORMATION` for OpenProcess.
- Never open handles to lsass.exe or csrss.exe.
- Run unelevated by default.

