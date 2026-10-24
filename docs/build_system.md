# Build system guide

Verdandi uses CMake to describe how the application is built and vcpkg to
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
the module build hierarchy. Every compiler and linker warning is an error, in
every preset ([DC-139](design_decisions.md#dc-139)). Module `CMakeLists.txt` files, added as their
modules are implemented, define the application, library, and test targets.
The root file also defines `verdandi_deploy_qt_plugins()`, which the app, the
tests, and the benchmarks call. It copies the Qt plugins that Qt loads at run
time rather than links next to each executable: the Windows platform plugin
into `platforms/`, and the Windows 11 and Vista styles into `styles/`. Without
the style plugin Qt falls back to its classic Windows style.

The root file writes `generated\verdandi_version.h` in the build directory from
`cmake/verdandi_version.h.in`: the version, description and homepage that
`project()` gives, and the copyright line beside it. Its `verdandi_version`
target gives the header to the code that shows them
([DC-136](design_decisions.md#dc-136)). It also compiles and links every
target with Control Flow Guard and CET shadow-stack compatibility, and gives
Release builds a program database, such as `verdandi.pdb` beside
`verdandi.exe` ([DC-137](design_decisions.md#dc-137)).

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
selects it. Its `build\release\src\app` is what a release ships: beside
`verdandi.exe` the build puts the Qt DLLs and plugins, the Microsoft C++
runtime, which only Release builds may redistribute, `LICENSE.txt`, and
`THIRD_PARTY_NOTICES.txt` ([DC-137](design_decisions.md#dc-137)).
`cmake/write_third_party_notices.cmake` writes the notices in every build:
for each DLL in the directory, the copyright file that vcpkg installed for the
package that installed it, and for nlohmann/json, which is compiled in. A DLL
that no vcpkg package installed fails the build
([DC-138](design_decisions.md#dc-138)). `.\scripts\build.ps1 -Benchmark` also selects it, then runs the
two executables in `benchmarks/`: `verdandi_benchmark`, which prints the
Performance tab's cost per tick for each page and chart window and the Network
tab's for 2 to 20 adapters, including a list with "Show all adapters" and an
adapter that changes state, and
`verdandi_process_benchmark`, which prints the Processes tab's cost per
tick for flat and tree views of 400 to 2,000 processes. The benchmarks are
built with every preset so they keep compiling, but they are not registered
with CTest and CI does not run them, because their timings depend on the
machine. For the same reason the script first prints the processor, the power
plan and the Windows 11 power mode, and each benchmark prints the compiler, Qt
and Windows versions it runs with. [performance.md](performance.md) records
the figures with those lines, and says how to record a new run.

A `coverage` configure, build, and test preset inherits from `default`, so it
is a Debug build, but builds in `build/coverage` with clang-cl instead of MSVC
([DC-118](design_decisions.md#dc-118)). The compiler is the `clang-cl.exe` in
`VC\Tools\Llvm\x64\bin` of the Visual Studio installation whose developer
environment is active, which the preset finds through `VCINSTALLDIR`. Its
version matches that installation's STL, and another LLVM on `PATH`, such as
the standalone LLVM on GitHub's Windows runners, need not. The preset sets
`VERDANDI_COVERAGE`, which the root `CMakeLists.txt` turns into
`-fprofile-instr-generate -fcoverage-mapping` for every target of the project,
tests and benchmarks included; configuring fails if it is on and the compiler
is not clang-cl. vcpkg still builds the dependencies with MSVC and its own
flags, so they are not instrumented, and the coverage build restores the same
packages from the binary cache as the other presets. Each instrumented object
names the profile runtime, `clang_rt.profile-x86_64.lib`, as a default
library, and `lld-link`, which CMake uses with clang-cl, finds it in the
compiler's own directories, so linking needs no options.

`.\scripts\build.ps1 -Coverage` selects the coverage preset and runs the
tests, so `-Test` is implied; it cannot be combined with `-Release`,
`-Benchmark` or `-Run`. It empties `build\coverage\profiles`, then runs CTest
with `LLVM_PROFILE_FILE` set to `build\coverage\profiles\%p_%m.profraw`, so
each test process writes its raw profile to a file of its own, named by its
process ID and its binary's signature. The shards run in parallel, and
Windows reuses process IDs: with `%m`, a later process of the same binary
that gets an earlier one's ID adds its counts to that file instead of
overwriting it. The script needs Visual Studio's "C++ Clang tools for
Windows" (`Microsoft.VisualStudio.Component.VC.Llvm.Clang`). Without an active
developer environment it imports one from an installation that has the
component, and when the active environment's installation lacks
`clang-cl.exe`, it stops and says so.

When the tests pass, the script merges the profiles with `llvm-profdata merge`
into `build\coverage\coverage.profdata` and reports on `src\` alone.
`llvm-cov export -summary-only` gives each file's line and branch counts. It
reads the two test executables and the application, which no test runs, so
that `main.cpp` counts as not covered instead of being absent. The script keeps
the files under `src\`, which leaves out test, moc and vcpkg sources, and
prints the coverage of each directory (`app`, `domain`, `monitoring`,
`platform/windows`, `ui`, `ui/charts`) and of `src\` in total, rounded down so
that only full coverage reads 100%. It chooses the files itself, regardless of
case, because `llvm-cov` compares paths case-sensitively: given `src` with its
drive letter in another case than CMake's, as VS Code's terminal may give it,
`llvm-cov` matches no file and reports every file instead. Then
`llvm-cov show -format=html -show-branches=count` writes `build\coverage\html`,
which gives each line's count and each branch's true and false counts, and a
summary of each directory; open `build\coverage\html\index.html`. A template's
instantiations are titled with MSVC's decorated names, because the LLVM tools
that come with Visual Studio have no demangler for them that `llvm-cov` can
call. The script leaves out `llvm-cov`'s warning that a few functions "have
mismatched data". A binary records an inline function that it compiles but
never calls with hash 0, which matches no profile, and the application's
`main()` has no profile at all; each is still counted, from the binary that
calls it or as not covered. A run removes the previous run's profiles and
report before its tests start, so a run whose tests fail leaves no coverage
report.

Then `scripts\coverage_dashboard.ps1` writes `build\coverage\index.html`, a
dashboard of the run's tests and coverage and of their trend
([DC-120](design_decisions.md#dc-120)). It reads the shards' GoogleTest reports,
the output of `llvm-cov export -summary-only`, which `build.ps1` saves as
`build\coverage\coverage_summary.json`, and the history,
`build\coverage\history.jsonl` or the file that `-CoverageHistoryPath` names,
and fills in `scripts\coverage_dashboard.html`. The page draws its charts as inline
SVG with an inline script and loads nothing else, so it opens from disk. It shows
the tests by kind (the directory under `tests\` of each test's source file) and
by suite, with how many passed, failed, were skipped or are disabled; their
total time, both added up and as CTest's run took from start to end, and the
slowest tests; the coverage of each directory and the least-covered files, each
linking to its page in `build\coverage\html`; and the trend of the test count,
the time and the coverage, one point per run. The times come from the
instrumented Debug build, so they compare only with other coverage runs. Each
run whose tests pass adds a line to the history first, with its date, commit
(and whether the working tree had changes), test counts, times, and covered and
total lines and branches of `src\`; the script removes the run's other outputs
before the tests, but keeps the history. When tests fail, the script still
writes the page, without coverage, listing the failed tests with their messages
and naming any shard that wrote no report, which `ctest --show-only=json-v1`
lists with the report each shard was told to write. It adds nothing to the
history, then fails. Either way it also writes `build\coverage\summary.md`, a
Markdown summary of the run that CI adds to its job summary: the failed tests
and the shards that wrote no report, or a table of the line and branch coverage
and the test count beside those of the previous run in the history, with their
change.

The integration tests call real Windows APIs, so the coverage of
`platform/windows` depends on the machine and on what runs on it. Three runs on
2026-10-19 on one machine covered 83.4% to 84.1% of its lines, because
`process_details_reader.cpp` reads the details of whichever processes are
running. CI's figures can differ again, so neither `-Coverage` nor CI's
`Coverage` job fails because of coverage, only because of the build or a test,
until CI's history shows how much it varies on the runner
([DC-122](design_decisions.md#dc-122)).

vcpkg installs release and debug libraries for the `x64-windows` triplet, so
each further build directory restores its dependencies from the binary cache
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

`resources/verdandi.ico` is the application icon. `resources/verdandi.svg` is
the same design at 256 px, for pages and documents, and
`resources/verdandi_256.png` the 256 px image, for places that take no SVG.
`resources/social_preview.png` is a 1280 x 640 px banner of the icon and the
name, which GitHub shows when a link to the repository is shared; it is
uploaded by hand in the repository's settings (Settings, General, Social
preview), which take no SVG. `resources/social_preview_wordmark.png` is an
alternative banner, with the icon's line in place of the name's V and no tile.
All five are generated:
`.\scripts\generate_icon.ps1` holds the icon's geometry, draws each size that
Windows picks from (16, 20, 24, 32, 40, 48, 64, 96 and 256 px) with GDI+, and
writes the files ([DC-130](design_decisions.md#dc-130)). Change the icon in the
script, not in the files, and commit the script with the files it writes.
`-PngDirectory` also saves each size as a PNG, to look at. `src/app/verdandi.rc`
compiles `verdandi.ico` into `verdandi.exe` as the resource `IDI_ICON1`, which
Explorer and the taskbar show and Qt gives every window
([DC-132](design_decisions.md#dc-132)). It also compiles in the version
information that Explorer's Details tab shows. Its product version is the
version followed by the commit, such as `0.1.0 (edbcb13)`, which
`cmake/write_product_version_header.cmake` reads with git on every build and
writes to `verdandi_product_version.h`, rewriting it only when it changes, so
a new commit recompiles only `main.cpp` and the resource script. `--version`,
Help > About Verdandi and the log's first line give the same
([DC-136](design_decisions.md#dc-136)).

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
Tools may require elevation. It also checks for Visual Studio's "C++ Clang
tools for Windows", which the coverage build needs, and with `-InstallMissing`
adds them to the Visual Studio installation it found, through the Visual
Studio Installer, which asks for elevation; a new Build Tools installation
gets them at once. Without the switch, missing Clang tools are a warning,
because the other builds do not need them. By default, the script only
reports missing tools; the `-InstallMissing` switch is an explicit opt-in. The
`-PersistEnvironment` switch saves the vcpkg environment variables for future
terminals.

For the current default preset, Ninja must be available on `PATH`. The MSVC
environment must be initialized as well. `scripts/build.ps1` does this
automatically (via `Enter-VsDevShell`), so it works from any terminal,
including VS Code's built-in one. The script builds without launching the
app; pass `-Run` to launch it after the build, and after the tests when they
pass. The script then prints the app's log as the app writes it, and returns
when its window closes ([DC-134](design_decisions.md#dc-134)). Pass `-Test` to
run CTest after the build
(`.\scripts\build.ps1 -Test`). Each test executable runs as GoogleTest shards,
one CTest test per shard (about two per logical processor for
`verdandi_tests`), and CTest runs one shard per logical processor at a
time. One more CTest test lists the tests of `verdandi_tests` with no Qt
platform plugin to load, which checks that listing them creates no
`QApplication` ([DC-21](design_decisions.md#dc-21)). A failed shard's output
lists the tests that failed. To run one test,
or a suite, start the executable with a filter, e.g.
`build\default\tests\verdandi_tests.exe --gtest_filter=RingBuffer.*`.
Each shard also writes GoogleTest's JSON report of the tests it ran to
`test_results\<executable>_shard_<index>.json` in the build directory, such as
`build\default\test_results\verdandi_tests_shard_0.json`
([DC-119](design_decisions.md#dc-119)). It gives each test's suite, name,
result and failures, time, and source file and line, and the directory of the
file tells unit, platform and integration tests apart. `tests/CMakeLists.txt`
sets `GTEST_OUTPUT` in each shard's environment, so every preset writes the
reports, and so does CTest run directly, as in CI. The script removes the
previous run's reports before the tests run, so that a shard that crashed
before writing its report, or one that a build with fewer shards no longer
has, leaves no stale file among them.
Builds are incremental, and the script runs the configure step only when the
build directory needs it ([DC-62](design_decisions.md#dc-62)). It records each
successful configure in `build_ps1_configure.stamp` in the build directory, and
configures when that stamp or `build.ninja` is missing, or when
`CMakePresets.json` or `CMakeUserPresets.json` is newer than the stamp; Ninja
re-runs configuration itself when a `CMakeLists.txt` or `vcpkg.json` changes.
Pass `-Configure` to force it, for example after changing an environment
variable that configuration reads. The stamp is removed before each configure
and written only after it succeeds, so a failed configure is repeated on the
next run. If configuring fails in a build directory that already has a CMake
cache, the script warns, removes `CMakeCache.txt` and `CMakeFiles`, keeping
`vcpkg_installed` and the Ninja build outputs, and tries once more
([DC-69](design_decisions.md#dc-69)). A stale cache, such as one from before a
Visual Studio update or from a moved checkout, therefore does not have to be
deleted by hand. A failure without an existing cache, or a second failure,
stops the script. The script checks native tools by exit code
rather than stderr output, so redirecting or capturing its output (as CI and
agents do) cannot turn a CMake warning into a failure. If you invoke `cmake`/`ctest` directly
instead of going through `build.ps1`, use the **Developer PowerShell for VS 2022**
or **x64 Native Tools Command Prompt**, let VS Code's CMake Tools extension select
an MSVC kit, or first run `.\scripts\build.ps1 -MsvcEnvironment`, which imports
the x64 environment into the PowerShell session that runs it and builds nothing.
The `default` preset pins `x64` and explicitly sets `CMAKE_CXX_COMPILER`
to `cl`, ensuring that CMake never silently falls back to an incompatible compiler
(such as MinGW GCC) if found on `PATH`.

## Why these prerequisites

| Tool | Why this project uses it |
| --- | --- |
| MSVC Build Tools and Windows SDK | Build the Windows desktop application and provide the supported C++ compiler, linker, and Windows headers. The full Visual Studio IDE is optional. |
| C++ Clang tools for Windows (Visual Studio component) | Provide clang-cl, `lld-link`, the profile runtime, `llvm-profdata` and `llvm-cov` for the coverage build, and clang-format for `-Format` and `-CheckFormat`. The other builds do not need them. |
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

The GitHub Actions workflow's `Build and Test` job runs the same three preset
commands for the `default` preset and then for `release`, the configuration
that ships, whose configure restores from vcpkg's binary cache the packages
that the first restored or built ([DC-139](design_decisions.md#dc-139)). Its
`Set up MSVC` step runs `.\scripts\build.ps1 -MsvcEnvironment`, which imports
the x64 MSVC environment as every build of the script does and writes each
variable that the import set or changed to `GITHUB_ENV`, so that the steps
that run `cmake` and `ctest` have it ([DC-141](design_decisions.md#dc-141)).
The job then uses `run-vcpkg` to set up the vcpkg commit pinned by this
repository. The checked-in presets, manifest, and baseline therefore keep
developer and CI dependency resolution aligned.

That job and the `Coverage` job keep vcpkg's binary cache in GitHub's cache
([DC-140](design_decisions.md#dc-140)). Each restores the newest cache that
either saved into `$env:RUNNER_TEMP\vcpkg-binary-cache`, and
`scripts\vcpkg_cache.ps1 -Prepare` sets `VCPKG_BINARY_SOURCES` to
`clear;files,<that directory>,readwrite`, so that vcpkg restores each package
whose archive is there and writes there the archive of each package it builds.
`run-vcpkg` keeps that value; left unset, it would set its `x-gha` provider,
which vcpkg has removed. After the steps that run vcpkg, also when one of them
failed, `-Prune` removes the restored archives that no package installed in a
build directory whose configure finished uses, and sets the step output
`changed`. Only when an archive was added or removed does the job save the
directory, under a key of its own (`vcpkg-x64-windows-` followed by the run,
its attempt and the job), which the next run's restore finds by that prefix.
The cache's path is part of its version, so the two jobs give the same one.
GitHub evicts a cache that no run has restored for 7 days.

A `Coverage` job runs `.\scripts\build.ps1 -Coverage` on each push to `main`
and each pull request, and keeps the history of the runs on `main` on the
`gh-pages` branch, which GitHub Pages serves at
<https://dundergg.github.io/verdandi/>
([DC-121](design_decisions.md#dc-121)). It has no `Set up MSVC` step: the
script imports the developer environment of a Visual Studio installation with
"C++ Clang tools for Windows", which the `windows-latest` runner's has (on
2026-10-18, Windows Server 2025 with Visual Studio Enterprise 2026 18.10). Its
steps run in Windows PowerShell 5.1, which the scripts are tried with. Before
the build, `scripts\coverage_pages.ps1 -Fetch` clones the branch into
`$env:RUNNER_TEMP\coverage-site`, or starts an empty clone when the branch does
not exist yet, and the build is given the clone's `history.jsonl` with
`-CoverageHistoryPath`. The run's dashboard therefore draws the published
trend, and the job summary, which the job copies from
`build\coverage\summary.md`, compares the run with the last run on `main`. The
job uploads the dashboard and `llvm-cov`'s HTML as the `coverage-dashboard`
artifact, which opens from disk once extracted. After a push to `main` whose
tests pass, `coverage_pages.ps1 -Publish` replaces the clone's files with the
run's, keeps the history that the run added to, writes `coverage_badge.json`,
the run's line coverage, which the README's coverage badge shows
([DC-133](design_decisions.md#dc-133)), adds an empty `.nojekyll` so that Pages
serves the files as they are, commits, and pushes the branch. A pull
request's run publishes nothing; its line goes only into its own clone's
history.

Only the `Coverage` job has `contents: write`. Its checkout keeps no
credentials, so the build and the tests run without the token, and the
publishing step gives it to git for that push alone. Every action in the
workflow is pinned by commit, with its release in a comment, so that a tag
moved to other code cannot run before that step; update an action's commit and
comment together. Runs on `main` share a
concurrency group, so that each fetches the history that the one before it
published; a newer run of a pull request cancels the older one. The clone turns
off `core.autocrlf`, which Git for Windows sets, so that the site's files are
stored as the build writes them. `coverage_pages.ps1 -Remote` takes another
URL or a local path, such as a bare repository, to try the script outside CI.
GitHub Pages has to be turned on once, in the repository's Pages settings, to
deploy from the root of the `gh-pages` branch.

A separate `Format Check` job installs clang-format 22.1.3 from PyPI and runs
`.\scripts\build.ps1 -CheckFormat`, which fails if any C++ source or header
under `src/`, `tests/`, or `benchmarks/` differs from `.clang-format`
([DC-23](design_decisions.md#dc-23)). It checks every file before it fails, so
the log lists all of them. Run `.\scripts\build.ps1 -Format` locally before
pushing; it formats the same files in place. Both take the files that git
tracks and the new ones that `.gitignore` does not exclude, so a file is
formatted before its first commit.

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
may cite it.

Both then check the fields of each entry
([DC-117](design_decisions.md#dc-117)). An entry that is not a stub must have
`**Decision:**`, `**Rationale:**`, an optional `**Consequences:**` and
`**History:**`, in that order, each at the start of a line, and no other field,
with nothing before its Decision and its History line last. A stub must have
one line and no field. The checks do not count words: the budget of about 250
words is the writer's to keep. Every problem from clang-format and both checks
is listed before the script fails.

`build.ps1` finds clang-format through `CLANG_FORMAT`, `PATH`,
the Visual Studio LLVM tools, or `%ProgramFiles%\LLVM`. Formatting output can
change between clang-format major versions, so the pinned major version is also
recorded in `build.ps1`: locally a different version is a warning, and in CI it
is an error. `bootstrap.ps1` reports whether it finds clang-format, which comes
with the C++ Clang tools that it adds with `-InstallMissing`. A missing
clang-format is only a warning there, because building does not need it.
