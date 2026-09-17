# GitHub Copilot Instructions — System Monitor

This is a Windows desktop system monitor built with C++20 and Qt 6 Widgets.
The rules below are self-contained. For deeper context, see `architecture.md`, `docs/coding_guidelines.md`, or `AGENTS.md`.

## Build & test commands

- Configure: `cmake --preset default`
- Build: `cmake --build --preset default`
- Test: `ctest --preset default --output-on-failure`

## Critical rules

### Architecture
- Module boundaries are strict: `ui/` has no Windows headers, `domain/` has no Qt or Windows headers, `platform/windows/` has no Qt headers.
- Data flows through immutable `SystemSnapshot` values via `Qt::QueuedConnection`.
- Use `NtQuerySystemInformation` for processes, `GetSystemTimes` for CPU, `GlobalMemoryStatusEx` for memory, `GetIfTable2` for network, `GetNetworkConnectivityHint` for connectivity.
- Do NOT use PDH for core metrics, Tool Help for processes, or WMI for polling.

### Code style
- No exceptions. Return `std::optional` or result structs.
- `PascalCase` types, `camelCase` functions/variables, `m_` members, `k` constants, `snake_case` files.
- Allman braces for classes/functions, K&R for control flow. Always use braces. 4-space indent.
- `std::jthread` + `std::stop_token` for threads. RAII for all resources.
- spdlog for logging, not qDebug or cout.

### Security
- Only `PROCESS_QUERY_LIMITED_INFORMATION` for OpenProcess.
- Never open handles to lsass.exe or csrss.exe.
- Run unelevated by default.

