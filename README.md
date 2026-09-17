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
- **Git**

All dependencies (Qt 6, spdlog, GoogleTest, nlohmann/json) are managed by vcpkg and downloaded automatically.

### Build

```powershell
git clone https://github.com/DunderGG/system-monitor.git
cd system-monitor

cmake --preset default
cmake --build --preset default
```

The first build can be slow while Qt compiles from source. Set up [vcpkg binary caching](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching) to speed up subsequent clean builds.

### Run tests

```powershell
ctest --preset default
```

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

