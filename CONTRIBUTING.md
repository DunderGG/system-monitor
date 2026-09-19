# Contributing to System Monitor

Thank you for your interest in contributing! This guide will help you get set up and explain how to submit changes.

## Prerequisites

System Monitor is built for Windows 10 (version 2004+) or Windows 11 using the **MSVC x64 compiler** (C++20), **CMake**, **Ninja**, and **vcpkg**.

| Prerequisite | Purpose | WinGet package ID |
| --- | --- | --- |
| **Windows 10/11** | Supported target platform | — |
| **MSVC C++ Build Tools & Windows SDK** | Compiler (`cl.exe`), linker, and Windows headers | `Microsoft.VisualStudio.2022.BuildTools` (or Visual Studio IDE) |
| **CMake 3.25+** | Build configuration and build preset driver | `Kitware.CMake` |
| **Ninja** | Fast build generator used by CMake presets | `Ninja-build.Ninja` |
| **Git** | Repository clone and vcpkg baseline checkout | `Git.Git` |
| **vcpkg** | Manages C++ dependencies (Qt 6, spdlog, GoogleTest, nlohmann/json) | Bundled with Visual Studio or standalone |

> You do **not** need to install Qt separately. All dependencies, including Qt 6, are declared in `vcpkg.json` and compiled/cached automatically by vcpkg during the first build.

For an overview of how CMake, presets, vcpkg, and CI work together, see [the build system guide](docs/build_system.md).

---

## Setup and Building

You can set up and build the project using either **automated scripts** or **manual CLI steps**.

### Option 1: Automated setup (Recommended)

The repository provides PowerShell scripts that automate tool detection, installation, vcpkg configuration, and MSVC environment initialization:

1. **Clone the repository:**
   ```powershell
   git clone https://github.com/DunderGG/system-monitor.git
   cd system-monitor
   ```

2. **Run the bootstrap script:**
   ```powershell
   .\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
   ```
   - Checks for Git, CMake, Ninja, and MSVC C++ Build Tools (installs any missing tools via WinGet with `-InstallMissing`).
   - Detects an existing vcpkg installation (such as Visual Studio's bundled `VC\vcpkg` or `vcpkg.path.txt`) or clones and bootstraps a fresh copy in `%LOCALAPPDATA%\vcpkg-root`.
   - Persists `VCPKG_ROOT` and `VCPKG_DEFAULT_BINARY_CACHE` to your User environment (`-PersistEnvironment`).

3. **Build from any terminal:**
   ```powershell
   # Build without launching the GUI
   .\scripts\build.ps1 -NoRun

   # Or build and launch the application
   .\scripts\build.ps1
   ```
   `scripts/build.ps1` automatically detects and activates the x64 MSVC developer environment into the running PowerShell process, so it works from any terminal (PowerShell, Windows Terminal, VS Code) without manual environment setup.

4. **Run tests:**
   ```powershell
   ctest --preset default --output-on-failure
   ```

---

### Option 2: Manual setup and build

If you prefer to configure your machine and invoke CMake manually without using the helper scripts:

#### Step 1: Install prerequisites
Install the required tools via WinGet:
```powershell
winget install --exact Git.Git Kitware.CMake Ninja-build.Ninja
winget install --exact Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```
*(Alternatively, install the [Visual Studio 2022 Community IDE](https://visualstudio.microsoft.com/) and check the "Desktop development with C++" workload).*

#### Step 2: Set up vcpkg
The project uses vcpkg to resolve dependencies declared in `vcpkg.json`. CMake expects the `VCPKG_ROOT` environment variable to point to your vcpkg installation:

- **If you have Visual Studio installed:** Visual Studio already bundles a complete copy of vcpkg:
  - Build Tools: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg`
  - Community: `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg` (or your custom VS install directory)
- **If installing standalone:** Clone vcpkg to a dedicated directory (e.g. `C:\vcpkg` or `$env:LOCALAPPDATA\vcpkg-root`; do **not** clone into `%LOCALAPPDATA%\vcpkg` as that directory is reserved for vcpkg's internal user metadata):
  ```powershell
  git clone --depth 1 https://github.com/microsoft/vcpkg.git "$env:LOCALAPPDATA\vcpkg-root"
  & "$env:LOCALAPPDATA\vcpkg-root\bootstrap-vcpkg.bat" -disableMetrics
  ```

Set the environment variables for your terminal:
```powershell
# Set in the current terminal session:
$env:VCPKG_ROOT = "$env:LOCALAPPDATA\vcpkg-root"   # Or path to your VC\vcpkg
$env:VCPKG_DEFAULT_BINARY_CACHE = "$env:LOCALAPPDATA\vcpkg-cache"

# (Recommended) Persist to User environment variables for all future terminals:
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $env:VCPKG_ROOT, "User")
[Environment]::SetEnvironmentVariable("VCPKG_DEFAULT_BINARY_CACHE", $env:VCPKG_DEFAULT_BINARY_CACHE, "User")
```

#### Step 3: Open an x64 MSVC Developer Terminal
The project requires MSVC (`cl.exe`). A standard PowerShell or Command Prompt window does **not** have MSVC on `PATH`.

Open one of the following from your Windows Start Menu:
- **Developer PowerShell for VS 2022** (or Build Tools)
- **x64 Native Tools Command Prompt for VS 2022** (or Build Tools)

*(You can verify that MSVC is active by running `cl.exe`. It should output `Microsoft (R) C/C++ Optimizing Compiler Version ... for x64`).*

#### Step 4: Configure, build, and test
From the project root inside the Developer terminal:
```powershell
# 1. Configure (downloads and compiles dependencies on first run; takes 30-90 minutes)
cmake --preset default

# 2. Build the application and test targets
cmake --build --preset default

# 3. Run all tests
ctest --preset default --output-on-failure
```

---

### Option 3: VS Code workflow

1. Install [VS Code](https://code.visualstudio.com/) with extensions:
   - **C/C++** (`ms-vscode.cpptools`)
   - **CMake Tools** (`ms-vscode.cmake-tools`)
2. Open the project folder in VS Code.
3. CMake Tools automatically detects `CMakePresets.json`:
   - Set the configure preset to **`default`** (Debug).
   - Set the build preset to **`default`**.
   - Set the test preset to **`default`**.
4. In the status bar or command palette (`Ctrl+Shift+P`), select **CMake: Configure**, then **CMake: Build**.

---

### Optional: use a local Qt installation

If you already have Qt 6 installed via the official installer, you can skip the vcpkg Qt build by setting `CMAKE_PREFIX_PATH` to your Qt installation in a custom CMake preset or as a command-line argument:

```powershell
cmake --preset default -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
```

---

## Troubleshooting & Common Pitfalls

### 1. `CMake Error: CMAKE_CXX_COMPILER could not be found: cl`
- **Cause:** Running `cmake --preset default` in a standard PowerShell or Command Prompt where MSVC (`cl.exe`) is not on `PATH`.
- **Fix:** Either run `.\scripts\build.ps1 -NoRun` (which initializes the MSVC environment automatically), or run from a **Developer PowerShell for VS 2022** / **x64 Native Tools Command Prompt**.

### 2. `undefined reference to 'testing::Test::Test()'` or MinGW/GCC linker errors
- **Cause:** If MinGW or MSYS2 (`c++.exe`) is on your system `PATH` and you ran `cmake` outside of an MSVC developer environment, CMake may have previously configured the project with GCC. GCC cannot link against MSVC-compiled dependencies from vcpkg.
- **Fix:**
  1. Clear the stale build cache:
     ```powershell
     Remove-Item -Path "build/default/CMakeCache.txt", "build/default/build.ninja" -Force
     Remove-Item -Recurse -Path "build/default/CMakeFiles" -Force
     ```
  2. Rebuild using `.\scripts\build.ps1 -NoRun` or from an x64 Developer PowerShell session.

### 3. `VCPKG_ROOT is not set` / Toolchain file not found
- **Cause:** The `default` preset relies on `$env{VCPKG_ROOT}` to locate vcpkg's CMake toolchain (`$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`).
- **Fix:** Set `$env:VCPKG_ROOT = "<path-to-vcpkg>"` in your session, or run `.\scripts\bootstrap.ps1 -PersistEnvironment` to set it permanently.

### 4. `File ... cannot be loaded because running scripts is disabled on this system`
- **Cause:** Windows PowerShell default `ExecutionPolicy Restricted`.
- **Fix:** Run the script with bypass:
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
  ```
  Or allow scripts for your current session:
  ```powershell
  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
  ```

### 5. `throw "'...' is not a valid vcpkg checkout. Use a different -VcpkgRoot path."`
- **Cause:** Using `%LOCALAPPDATA%\vcpkg` as the vcpkg checkout root. On Windows, `%LOCALAPPDATA%\vcpkg` is used by vcpkg to store user-level configuration and registration files (`vcpkg.path.txt`), so it is not a Git repository checkout.
- **Fix:** Use an existing vcpkg installation (such as Visual Studio's `VC\vcpkg`), or clone to `%LOCALAPPDATA%\vcpkg-root` or `C:\vcpkg`.

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
