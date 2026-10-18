# Build system guide

System Monitor uses CMake to describe how the application is built and vcpkg to
provide its third-party C++ dependencies. CMake presets make the command-line,
VS Code, and CI builds use the same configuration.

## How the pieces fit together

```text
cmake --preset default
        |
        v
CMakePresets.json ──> generator, build directory, and vcpkg toolchain
        |
        v
CMakeLists.txt ──> project options, compiler settings, and module targets
        |
        v
vcpkg toolchain ──> vcpkg.json dependency manifest
        |                    |
        |                    v
        |             vcpkg-configuration.json registry baseline
        v
installed dependency packages ──> CMake find_package() calls and targets
```

## Repository files

`CMakeLists.txt` is the build definition. The root file establishes the C++20
standard, common warning flags, Windows preprocessor definitions, testing, and
the module build hierarchy. Module `CMakeLists.txt` files, added as their
modules are implemented, define the application, library, and test targets.
The root file also defines `sysmon_deploy_qt_plugins()`, which the app, the
tests, and the benchmarks call. It copies the Qt plugins that Qt loads at run
time rather than links next to each executable: the Windows platform plugin
into `platforms/`, and the Windows 11 and Vista styles into `styles/`. Without
the style plugin Qt falls back to its classic Windows style.

`CMakePresets.json` is the checked-in build entry point. Its `default`
configure preset selects the Ninja generator, uses `build/default` for generated
files, requests a Debug build, and sets `CMAKE_TOOLCHAIN_FILE` to:

```text
$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
```

The same file also defines `default` build and test presets. This is why the
following commands use the same preset name:

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

A `release` configure, build, and test preset inherits everything from
`default` except `CMAKE_BUILD_TYPE=Release`, and builds in `build/release`.
Use it to measure performance: the Debug Qt libraries that `default` links are
many times slower, especially for painting. `.\scripts\build.ps1 -Release`
selects it. `.\scripts\build.ps1 -Benchmark` also selects it, then runs the
two executables in `benchmarks/`: `system_monitor_benchmark`, which prints the
Performance tab's cost per tick for each page and chart window and the Network
tab's for 2 to 20 adapters, including a list with "Show all adapters" and an
adapter that changes state, and
`system_monitor_process_benchmark`, which prints the Processes tab's cost per
tick for flat and tree views of 400 to 2,000 processes. The benchmarks are
built with every preset so they keep compiling, but they are not registered
with CTest and CI does not run them, because their timings depend on the
machine. For the same reason the script first prints the processor, the power
plan and the Windows 11 power mode, and each benchmark prints the compiler, Qt
and Windows versions it runs with. Record those lines with any figures.

vcpkg installs release and debug libraries for the `x64-windows` triplet, so
the second build directory restores its dependencies from the binary cache
instead of rebuilding them.

`vcpkg.json` is the dependency manifest. It lists the packages the project
needs, including Qt, spdlog, GoogleTest, and nlohmann-json. It disables Qt's
default features and requests only Qt GUI, Network, PNG, and Widgets support to
keep the dependency build focused. PNG is needed because Qt's Windows platform
plugin loads its split, hand, and drag cursors from PNG resources. When CMake loads the vcpkg toolchain during
configuration, vcpkg reads this manifest, obtains the declared packages, and
makes them available to CMake's `find_package()` calls.

`vcpkg-configuration.json` defines the vcpkg registry baseline. The baseline
fixes the exact vcpkg ports snapshot used to resolve `vcpkg.json`, so a fresh
build uses compatible package definitions instead of silently moving to newer
ones.

`CMakeUserPresets.json` is an optional, local-only companion file. It is ignored
by Git and can inherit from `default` to set machine-specific values, such as a
local Qt path or a different build directory. Do not put those values in the
shared `CMakePresets.json`.

## Required local setup

The recommended setup is the repository bootstrap script:

```powershell
.\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
```

It detects Git, CMake, Ninja, and the MSVC C++ workload. With
`-InstallMissing`, it uses WinGet to install missing tools, automatically detects
an existing vcpkg installation (such as Visual Studio's bundled vcpkg,
`vcpkg.path.txt`, or `VCPKG_ROOT`) or clones and bootstraps a fresh checkout in
`%LOCALAPPDATA%\vcpkg-root`, creates a binary cache in
`%LOCALAPPDATA%\vcpkg-cache`, and verifies the result. Installation of Build
Tools may require elevation. By default, the script only reports missing tools;
the `-InstallMissing` switch is an explicit opt-in. The `-PersistEnvironment`
switch saves the vcpkg environment variables for future terminals.

For the current default preset, Ninja must be available on `PATH`. The MSVC
environment must be initialized as well. `scripts/build.ps1` does this
automatically (via `Enter-VsDevShell`), so it works from any terminal,
including VS Code's built-in one. Pass `-Test` to run CTest after the build
(`.\scripts\build.ps1 -Test`). Each test executable runs as GoogleTest shards,
one CTest test per shard (about two per logical processor for
`system_monitor_tests`), and CTest runs one shard per logical processor at a
time. A failed shard's output lists the tests that failed. To run one test,
or a suite, start the executable with a filter, e.g.
`build\default\tests\system_monitor_tests.exe --gtest_filter=RingBuffer.*`.
Builds are incremental, and the script runs the configure step only when the
build directory has not been configured by it yet or a presets file changed
since; Ninja re-runs configuration itself when a `CMakeLists.txt` or
`vcpkg.json` changes. Pass `-Configure` to force it, for example after changing
an environment variable that configuration reads. If configuring fails in a
build directory that already has a CMake cache, the script removes
`CMakeCache.txt` and `CMakeFiles` (keeping `vcpkg_installed`) and tries once
more. A stale cache, such as one from before a Visual Studio update or from a
moved checkout, therefore does not have to be deleted by hand. The script checks native tools by exit code
rather than stderr output, so redirecting or capturing its output (as CI and
agents do) cannot turn a CMake warning into a failure. If you invoke `cmake`/`ctest` directly
instead of going through `build.ps1`, use the **Developer PowerShell for VS 2022**
or **x64 Native Tools Command Prompt**, or let VS Code's CMake Tools extension select
an MSVC kit. The `default` preset pins `x64` and explicitly sets `CMAKE_CXX_COMPILER`
to `cl`, ensuring that CMake never silently falls back to an incompatible compiler
(such as MinGW GCC) if found on `PATH`.

## Why these prerequisites

| Tool | Why this project uses it |
| --- | --- |
| MSVC Build Tools and Windows SDK | Build the Windows desktop application and provide the supported C++ compiler, linker, and Windows headers. The full Visual Studio IDE is optional. |
| CMake | Translates the portable target definitions in `CMakeLists.txt` into build files for the selected compiler and coordinates vcpkg during configuration. |
| Ninja | Executes the generated build graph quickly and consistently. The checked-in `default` preset explicitly selects Ninja, so local, VS Code, and CI builds use the same backend. |
| vcpkg | Downloads, builds, and exposes the declared C++ dependencies from `vcpkg.json`; it removes the need to install Qt, spdlog, GoogleTest, or nlohmann-json by hand. |
| Git | Obtains this repository and lets the bootstrap script clone vcpkg at the reproducible baseline. |
| VS Code CMake Tools (optional) | Reads `CMakePresets.json`, selects the MSVC environment, and provides configure/build/test commands within VS Code. |
| VS Code C/C++ extension (optional) | Adds C++ language services such as IntelliSense, navigation, and debugging support. It does not replace the compiler or build system. |

Ninja is deliberately separate from CMake: CMake generates build instructions,
while Ninja performs the compilation. A Visual Studio generator could remove the
Ninja prerequisite, but it would make command-line and CI behavior less uniform
than the current single-generator setup.

For example, after installing vcpkg to `C:\vcpkg`:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset default
```

`VCPKG_DEFAULT_BINARY_CACHE` is optional. Point it at a writable directory to
reuse compiled vcpkg packages across clean build directories; it does not alter
the packages selected by the manifest and baseline.

## VS Code and CI

In VS Code, CMake Tools reads `CMakePresets.json`. Select an MSVC kit, configure
with the `default` preset, then build or test with the matching preset. CMake
Tools supplies the compiler environment; adding only `cl.exe` to `PATH` is not
enough because MSVC also needs its include and library environment variables.

The GitHub Actions workflow runs the same three preset commands. It initializes
the x64 MSVC environment and uses `run-vcpkg` to set up the vcpkg commit pinned
by this repository, with GitHub Actions-backed binary caching. The checked-in
presets, manifest, and baseline therefore keep developer and CI dependency
resolution aligned.

A separate `Format Check` job installs clang-format 22.1.3 from PyPI and runs
`.\scripts\build.ps1 -CheckFormat`, which fails if any file under `src/`,
`tests/`, or `benchmarks/` differs from `.clang-format`. It checks every file
before it fails, so the log lists all of them. Run `.\scripts\build.ps1 -Format` locally
before pushing.

Both `-Format` and `-CheckFormat` then check the links to design decisions
([DC-114](design_decisions.md#dc-114)). They fail when an entry heading in
`docs/design_decisions.md` lacks its `<a id="dc-NN"></a>` anchor, when a
Markdown link into that file names neither an anchor nor a section heading,
when a link whose text is `DC-NN` names another entry, or when any tracked or
new text file cites a `DC-NN` that is not an entry. They also fail when a link
or citation names a stub, an entry merged into another or retired, instead of
the entry that has its decision now
([DC-115](design_decisions.md#dc-115)): only the stub's line in the contents
may link to it, and only `docs/design_decisions.md` and `docs/code_reviews/`
may cite it. Every problem is listed before the script fails. `build.ps1` finds clang-format through `CLANG_FORMAT`, `PATH`,
the Visual Studio LLVM tools, or `%ProgramFiles%\LLVM`. Formatting output can
change between clang-format major versions, so the pinned major version is also
recorded in `build.ps1`: locally a different version is a warning, and in CI it
is an error.
