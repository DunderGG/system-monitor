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

`vcpkg.json` is the dependency manifest. It lists the packages the project
needs, including Qt, spdlog, GoogleTest, and nlohmann-json. It disables Qt's
default features and requests only Qt GUI, Network, and Widgets support to keep
the dependency build focused. When CMake loads the vcpkg toolchain during
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
`-InstallMissing`, it uses WinGet to install missing tools, clones and
bootstraps vcpkg in `%LOCALAPPDATA%\vcpkg`, creates a binary cache in
`%LOCALAPPDATA%\vcpkg-cache`, and verifies the result. Installation of Build
Tools may require elevation. By default, the script only reports missing tools;
the `-InstallMissing` switch is an explicit opt-in. The `-PersistEnvironment`
switch saves the vcpkg environment variables for future terminals.

For the current default preset, Ninja must be available on `PATH`. The MSVC
environment must be initialized as well. `scripts/build.ps1` does this
automatically (via `Enter-VsDevShell`), so it works from any terminal,
including VS Code's built-in one. If you invoke `cmake`/`ctest` directly
instead of going through `build.ps1`, use a Developer PowerShell/Command
Prompt, or let VS Code's CMake Tools extension select an MSVC kit. The
`default` preset pins `x64`, so CMake fails with a clear error instead of
silently falling back to the x86 toolset if the wrong environment is active.

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
