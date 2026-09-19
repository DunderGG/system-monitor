# System Monitor

A lightweight Windows desktop system monitor. Provides a clear view of system health, resource usage, network connectivity, and running processes while keeping its own footprint minimal.

Built with C++20, Qt 6, and direct Windows APIs for accurate, low-overhead monitoring.

## Features (MVP)

- **Dashboard** — At-a-glance CPU, memory, disk, and network usage with health indicators and uptime.
- **Performance charts** — Live rolling sparkline charts for CPU (total and per-core), memory, disk, and network throughput.
- **Process explorer** — Sortable, filterable process list with CPU, memory, I/O, threads, handles, and process tree view.
- **Network view** — Adapter details, IP addresses, live throughput charts, and structured connectivity status.
- **Process management** — Terminate processes with confirmation (user-owned processes).

## Screenshots

*Coming soon — the application is under active development.*

## Building from source

### Prerequisites

- **Windows 10** (version 2004+) or **Windows 11**
- **Visual Studio 2022** with the "Desktop development with C++" workload, **or** the standalone [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) (for VS Code users)
- **CMake 3.25+**
- **Ninja**
- **vcpkg** (bundled with Visual Studio or standalone) with `VCPKG_ROOT` set
- **Git**

All dependencies (Qt 6, spdlog, GoogleTest, nlohmann/json) are managed by vcpkg and downloaded automatically during project configuration.

### Quick start (Automated)

The fastest way to get started is using the repository scripts. From PowerShell:

```powershell
# Check or install missing tools, locate/bootstrap vcpkg, and persist environment variables:
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment

# Build from any terminal (automatically initializes the x64 MSVC environment):
.\scripts\build.ps1 -NoRun

# Run tests:
ctest --preset default
```

### Manual build

If you prefer building manually without scripts:

1. Open the **Developer PowerShell for VS 2022** or **x64 Native Tools Command Prompt for VS 2022** (so `cl.exe` is active on `PATH`).
2. Ensure `VCPKG_ROOT` is set in your session (e.g. `$env:VCPKG_ROOT = "C:\vcpkg"` or to Visual Studio's `VC\vcpkg`).
3. Run:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The first build will compile dependencies (including Qt 6) from source. Set up [vcpkg binary caching](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching) to speed up subsequent clean builds.

See [CONTRIBUTING.md](CONTRIBUTING.md) for full manual setup details, IDE configuration, and troubleshooting.

## How it works

System Monitor uses direct Windows APIs for accurate, low-overhead data collection:

| Data | API |
| --- | --- |
| CPU usage | `GetSystemTimes`, `NtQuerySystemInformation` |
| Memory | `GlobalMemoryStatusEx` |
| Disk space | `GetDiskFreeSpaceExW` |
| Processes | `NtQuerySystemInformation` (same API Task Manager uses) |
| Network throughput | `GetIfTable2` (64-bit counters) |
| Connectivity | `GetNetworkConnectivityHint` |

Metrics are collected on background threads and delivered to the UI as immutable snapshots, keeping the interface responsive. See [architecture.md](architecture.md) for the full technical design.

## Roadmap

See [roadmap.md](roadmap.md) for the detailed implementation plan. Upcoming features include:

- System tray integration
- Dark mode support
- Alert rules and notifications
- GPU monitoring
- ETW-powered diagnostics
- Data export (CSV/JSON)

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for setup instructions, code style guidelines, and how to submit changes.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE). See the [LICENSE](LICENSE) file for the full text.

