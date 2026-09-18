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

`vcpkg.json` will be the dependency manifest. It lists the packages the project
needs, such as Qt, spdlog, GoogleTest, and nlohmann-json. When CMake loads the
vcpkg toolchain during configuration, vcpkg reads this manifest, obtains the
declared packages, and makes them available to CMake's `find_package()` calls.

`vcpkg-configuration.json` will define the vcpkg registry baseline. The
baseline fixes the exact vcpkg ports snapshot used to resolve `vcpkg.json`, so a
fresh build later uses compatible package definitions instead of silently moving
to newer ones.

`CMakeUserPresets.json` is an optional, local-only companion file. It is ignored
by Git and can inherit from `default` to set machine-specific values, such as a
local Qt path or a different build directory. Do not put those values in the
shared `CMakePresets.json`.

## Required local setup

Install vcpkg and set `VCPKG_ROOT` to its installation directory. For the
current default preset, Ninja must also be available on `PATH`. The MSVC
environment must be initialized as well: use a Developer PowerShell/Command
Prompt, or let VS Code's CMake Tools extension select an MSVC kit.

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

The GitHub Actions workflow runs the same three preset commands. It sets a
vcpkg binary-cache location and restores it between runs. The checked-in
presets, manifest, and baseline therefore keep developer and CI dependency
resolution aligned.
