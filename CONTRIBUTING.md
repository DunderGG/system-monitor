# Contributing to System Monitor

Thank you for your interest in contributing! This guide will help you get set up and explain how to submit changes.

## Prerequisites

- **Windows 10** (version 2004 or later) or **Windows 11**
- **MSVC compiler and Windows SDK** (see options below)
- **CMake 3.25+**
- **Ninja** (the default CMake preset generator)
- **vcpkg**, with the `VCPKG_ROOT` environment variable set to its installation directory
- **Git**

You do **not** need to install Qt separately. All dependencies, including Qt 6, are managed through vcpkg and will be downloaded and built automatically on first configure.

For an overview of how CMake, presets, vcpkg, and CI work together, see [the build system guide](docs/build_system.md).

To check prerequisites and configure vcpkg automatically, run the bootstrap script from PowerShell. Add `-InstallMissing` to allow it to install missing tools through WinGet, and `-PersistEnvironment` to save the vcpkg environment variables for future terminals.

```powershell
.\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
```

### Option A: Visual Studio (full IDE)

Install [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community edition is free) with the **"Desktop development with C++"** workload. This includes the MSVC compiler, Windows SDK, and CMake.

### Option B: VS Code + Build Tools (lightweight)

1. Install [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) and select the **"Desktop development with C++"** workload. This installs only the MSVC compiler and Windows SDK without the full IDE (~3–5 GB).
2. Install [CMake](https://cmake.org/download/) (3.25 or later).
3. Install [VS Code](https://code.visualstudio.com/) with the following extensions:
   - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) (Microsoft)
   - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) (Microsoft)
4. When building from the command line, use the **Developer Command Prompt** or **Developer PowerShell** (installed by Build Tools) so that the compiler is on your PATH. In VS Code, the CMake Tools extension detects the Build Tools installation automatically.

## Getting started

### 1. Clone the repository

```powershell
git clone https://github.com/DunderGG/system-monitor.git
cd system-monitor
```

### 2. Configure and build

The project uses CMake presets for a consistent build environment. The first build will take longer (30–90 minutes) as vcpkg compiles Qt and other dependencies from source. Subsequent builds reuse cached binaries.

```powershell
# Configure (downloads and builds dependencies on first run)
cmake --preset default

# Build
cmake --build --preset default

# Run tests
ctest --preset default
```

### 3. Speed up rebuilds with binary caching

To avoid recompiling dependencies on clean builds, set up a local vcpkg binary cache:

```powershell
# Create a cache directory (run once)
mkdir C:\vcpkg-cache

# Set the environment variable (add to your shell profile for persistence)
$env:VCPKG_DEFAULT_BINARY_CACHE = "C:\vcpkg-cache"
```

### 4. Optional: use a local Qt installation

If you already have Qt 6 installed via the official installer, you can skip the vcpkg Qt build by setting `CMAKE_PREFIX_PATH` to your Qt installation in a custom CMake preset or as a command-line argument:

```powershell
cmake --preset default -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
```

## Project structure

```
system-monitor/
  CMakeLists.txt           # Root CMake configuration
  CMakePresets.json         # Build presets for consistent environments
  vcpkg.json                # Dependency manifest
  vcpkg-configuration.json  # vcpkg baseline pinning
  src/
    app/                    # Application entry point, wiring, logging
    domain/                 # Data types and business logic (no Qt/Windows deps)
    monitoring/             # Schedulers, collectors, ring buffers, snapshots
    platform/windows/       # Windows API wrappers
    persistence/            # Settings and future data storage
    ui/                     # Qt Widgets, views, and view models
      charts/               # Custom sparkline chart widgets
  tests/
    unit/                   # Unit tests (fast, no OS dependencies)
    integration/            # Integration tests (may use real Windows APIs)
  resources/                # Icons, manifests, and other assets
  docs/                     # Documentation
```

## Architecture overview

Read [architecture.md](architecture.md) for the full technical design. Key principles:

- **Layered separation**: UI → view models → monitoring facade → collectors → platform wrappers. The `ui` module never calls Windows APIs directly.
- **Immutable snapshots**: Collectors produce data on background threads; the UI receives immutable `SystemSnapshot` values via Qt queued signals.
- **Explicit error modeling**: Missing or inaccessible data is represented explicitly, never as zero.
- **`domain` is dependency-free**: Types in `src/domain/` have no Qt or Windows headers.

## How to contribute

### Reporting bugs

Open a GitHub issue with:

- Steps to reproduce
- Expected vs actual behavior
- Windows version and edition
- Whether you are running as a standard user or administrator

### Suggesting features

Open a GitHub issue describing the feature, its use case, and how it fits into the existing architecture. Check the [roadmap](roadmap.md) first — the feature may already be planned.

### Submitting code

1. **Fork** the repository and create a feature branch from `main`:
   ```
   git checkout -b feature/your-feature-name
   ```

2. **Follow the architecture**. Place code in the correct module. Keep `domain` free of Qt and Windows dependencies. Keep `ui` free of direct Windows API calls.

3. **Write tests**.
   - Unit tests for logic, calculations, data transformations, and ring buffer behavior.
   - Integration tests for Windows API wrappers (these should tolerate varying host configurations).
   - Tests go in `tests/unit/` or `tests/integration/` as appropriate.

4. **Keep commits focused**. One logical change per commit. Write clear commit messages.

5. **Ensure CI passes**. The GitHub Actions workflow builds and runs tests automatically on every push and pull request.

6. **Open a pull request** against `main` with a clear description of what the change does and why.

### Code style

Follow the project's [Coding Guidelines](docs/coding_guidelines.md). Key points:

- **C++20**. Use modern features: `std::jthread`, `std::stop_token`, `std::format`, concepts, designated initializers, `std::optional`.
- **Naming**: `PascalCase` for types, `camelCase` for functions and variables, `m_` prefix for members, `k` prefix for constants. Files use `snake_case`.
- **Error handling**: Return error types or `std::optional`. No exceptions.
- **Memory**: RAII everywhere. `std::unique_ptr` for ownership, Qt parent-child for widgets, RAII wrappers for Windows handles.
- **Threading**: Immutable snapshots across thread boundaries. `std::jthread` + `std::stop_token`. Never touch widgets from background threads.
- **Windows APIs**: Isolated in `src/platform/windows/`. Convert to domain types at the boundary. Use `W` (wide) API variants.
- **Formatting**: 4 spaces, Allman braces for classes/functions, K&R braces for control flow, always use braces.
- **Warnings**: The project builds with high warning levels. Fix all warnings before submitting.

### Commit messages

Use a clear, imperative style:

```
Add per-core CPU collector using NtQuerySystemInformation

Implements the per-core CPU usage collector by querying
SystemProcessorPerformanceInformation from ntdll.dll.
Calculates usage percentages from idle/kernel/user time deltas
over monotonic elapsed intervals.
```

## Testing

Run all tests:

```powershell
ctest --preset default
```

Run a specific test:

```powershell
ctest --preset default -R "RingBufferTest"
```

### Writing tests

- Use GoogleTest (`TEST`, `TEST_F`, `EXPECT_*`, `ASSERT_*`).
- Unit tests should be fast and deterministic. Use fake/mock collectors to avoid depending on the host system.
- Integration tests may call real Windows APIs. They should be tolerant of varying machine configurations (different core counts, network adapters, permissions).
- Test edge cases: PID reuse, sleep/resume time gaps, access-denied processes, missing performance counters.

## Getting help

If you have questions about the codebase or architecture, open a discussion on GitHub or comment on the relevant issue. We're happy to help new contributors get oriented.
