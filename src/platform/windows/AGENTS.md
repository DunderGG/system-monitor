# Platform/Windows Module Rules

This module contains all Windows API wrappers. It is the only module that includes Windows headers.

## Dependency constraints

- **No Qt headers.** This module must not depend on Qt. It returns domain types, not Qt types.
- **This is the only module allowed to include Windows headers** (`<Windows.h>`, `<winternl.h>`, `<iphlpapi.h>`, `<pdh.h>`, etc.).

## API usage rules

- Use `NtQuerySystemInformation(SystemProcessInformation)` for process enumeration. Do not use `CreateToolhelp32Snapshot`.
- Use `GetSystemTimes` for total CPU. Use `NtQuerySystemInformation(SystemProcessorPerformanceInformation)` for per-core CPU.
- Use `GlobalMemoryStatusEx` for memory. Use `GetDiskFreeSpaceExW` for disk space.
- Use `GetIfTable2` for network throughput (64-bit counters). Never use legacy `GetIfTable` (32-bit counters that overflow).
- Use `GetNetworkConnectivityHint` for connectivity status.
- Use PDH only for advanced per-instance counters where no direct Win32 alternative exists. Always use `PdhAddEnglishCounterW`.
- Use WMI only for one-shot startup queries (hardware inventory). Never poll WMI.

## Security rules

- Only request `PROCESS_QUERY_LIMITED_INFORMATION` when calling `OpenProcess`. Never use `PROCESS_ALL_ACCESS` or `PROCESS_VM_READ`.
- Never call `OpenProcess` on `lsass.exe` or `csrss.exe` (triggers EDR heuristic alerts).
- Use `W` (wide/Unicode) variants of all Windows APIs.

## Implementation rules

- Define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before including `<Windows.h>` (or define project-wide in CMake).
- Resolve `NtQuerySystemInformation` and other ntdll functions dynamically via `GetProcAddress` at initialization.
- Wrap all Windows handles (process handles, PDH queries, MIB tables) in RAII types that release in the destructor.
- Convert all Windows types (FILETIME, SYSTEM_PROCESS_INFORMATION, MIB_IF_ROW2, etc.) to domain types at the boundary. Never expose Windows types outside this module.
- Check return values from every Windows API call. Convert error codes to domain error types.
- Use the resizing-buffer loop pattern for `NtQuerySystemInformation` calls.
- Cache process static identity (path, command line) by `(PID, creation time)`. Do not re-query on every refresh.
- Release `GetIfTable2` results with `FreeMibTable`.

