<#
.SYNOPSIS
Configures, builds, and optionally runs the System Monitor desktop application.

.DESCRIPTION
Runs CMake's default (Debug) preset, or the release preset with -Release, which
use vcpkg to resolve project dependencies and Ninja/MSVC to compile the
executable. With -Run, the script launches the executable after a successful
build (and tests, if -Test is also supplied).

Builds are incremental: Ninja recompiles only what changed. The configure step
runs only when the build directory is not configured yet or a CMake presets file
changed since the last configure; Ninja re-runs it by itself when a
CMakeLists.txt or vcpkg.json changes. Use -Configure to force it. If configuring
fails in a build directory that already has a CMake cache, the script removes
CMakeCache.txt and CMakeFiles (keeping vcpkg_installed) and tries once more, so
a stale cache does not need to be deleted by hand.

Run scripts/bootstrap.ps1 first to install prerequisites and configure the
VCPKG_ROOT environment variable.

.PARAMETER Run
Launches the application after a successful build. Without it, the script only
configures and builds (and tests, with -Test).

.PARAMETER Test
Runs the test suite with CTest after a successful build. Each test executable
runs as GoogleTest shards, one CTest test each, and CTest runs one shard per
logical processor at a time. A failed shard's output names the tests that
failed; run one test with the executable's --gtest_filter, for example
build\default\tests\system_monitor_tests.exe --gtest_filter=RingBuffer.*.
The script fails if any test fails. Combine with -Run to launch the
application after the tests pass.

.PARAMETER Configure
Runs CMake's configure step even when the build directory looks up to date, for
example after changing an environment variable that configuration reads.

.PARAMETER Release
Builds (and tests or runs) the optimized Release configuration in build\release
instead of the Debug one in build\default. Use it to measure performance; Debug
Qt libraries are many times slower, especially for painting.

.PARAMETER Benchmark
Builds the Release configuration (implies -Release) and runs the tick-cost
benchmarks: the Performance tab's (benchmarks\performance_tick_benchmark.cpp),
which prints the mean and maximum time per tick for each page and chart
window and then for the Network tab with 2 to 20 adapters, then the
Processes tab's (benchmarks\process_table_benchmark.cpp),
which prints the time per tick for flat and tree views of 400 to 2,000
processes. They run after the tests when -Test is also supplied. Their
timings depend on the machine, so they are not part of the test suite and CI
does not run them. The script first prints the processor, the power plan and
the Windows 11 power mode, and each benchmark prints the compiler, Qt and
Windows versions it runs with; record them with the figures in
docs\performance.md.

.PARAMETER Format
Formats every C++ source and header under src/, tests/, and benchmarks/ in
place with clang-format, using the repository's .clang-format, then checks the
design decision links (see -CheckFormat) and exits without building. New files
count even before they are added to git; files ignored by .gitignore do not.

.PARAMETER CheckFormat
Checks that every C++ source and header under src/, tests/, and benchmarks/
(the same files as -Format) matches .clang-format, without changing files, then
checks the design decision links and exits without building. Fails and lists
the differences in every file that needs formatting and every link that does
not resolve, after checking them all. CI runs this check.

The link check reads docs\design_decisions.md and the text files that are
tracked or new. Each entry's "### DC-NN:" heading needs <a id="dc-NN"></a> on
the line above it. A link into the file must name such an anchor or a "## "
section heading, a link whose text is DC-NN must name that entry's anchor, and
every DC-NN that a file cites must be an entry. An entry whose heading reads
"Merged into ..." or "Retired" is a stub: only its line in the contents may link
to it, and no file but docs\design_decisions.md and those in docs\code_reviews\
may cite it.

clang-format is taken from the CLANG_FORMAT environment variable if set, then
PATH, then the Visual Studio LLVM tools, then %ProgramFiles%\LLVM. CI pins major
version 22; other versions may format differently. In CI a different major
version is an error; locally it is a warning.

.EXAMPLE
.\scripts\build.ps1

.EXAMPLE
.\scripts\build.ps1 -Run

.EXAMPLE
.\scripts\build.ps1 -Test

.EXAMPLE
.\scripts\build.ps1 -Test -Release

.EXAMPLE
.\scripts\build.ps1 -Benchmark

.EXAMPLE
.\scripts\build.ps1 -Format

.EXAMPLE
.\scripts\build.ps1 -CheckFormat
#>
[CmdletBinding()]
param(
    [switch]$Run,
    [switch]$Test,
    [switch]$Configure,
    [switch]$Release,
    [switch]$Benchmark,
    [switch]$Format,
    [switch]$CheckFormat
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# The clang-format major version CI uses (.github/workflows/ci.yml). Different
# major versions can format the same code differently.
$requiredClangFormatMajorVersion = 22

# Runs a native tool and fails on a non-zero exit code. In Windows PowerShell 5.1,
# when the caller redirects stderr (2>&1, *>, CI log capture), every stderr line
# becomes an ErrorRecord, and with ErrorActionPreference=Stop the first CMake
# warning would abort the script. Exit codes are the source of truth instead.
function Invoke-NativeCommand
{
    param(
        [Parameter(Mandatory)][string]$Description,
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @()
    )

    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try
    {
        & $FilePath @Arguments
    }
    finally
    {
        $ErrorActionPreference = $previousPreference
    }

    if ($LASTEXITCODE -ne 0)
    {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

# Prints the processor and the power settings, which the benchmarks' timings
# depend on as much as on the code. The benchmarks print the compiler, Qt and
# Windows versions themselves. Recorded with a table of figures, they tell a
# change in the machine apart from a regression. Values that cannot be read
# are printed as "unknown".
function Write-BenchmarkMachine
{
    $processor = "unknown"
    $processorKey = Get-ItemProperty -LiteralPath "HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0" `
        -ErrorAction SilentlyContinue
    if ($null -ne $processorKey -and $processorKey.PSObject.Properties["ProcessorNameString"])
    {
        $processor = $processorKey.ProcessorNameString.Trim()
    }

    # powercfg prints "Power Scheme GUID: <guid>  (<name>)".
    $powerPlan = "unknown"
    try
    {
        $schemeText = (& powercfg /getactivescheme | Out-String).Trim()
        if ($LASTEXITCODE -eq 0 -and $schemeText -match "\(([^)]+)\)$")
        {
            $powerPlan = $Matches[1]
        }
    }
    catch
    {
        $powerPlan = "unknown"
    }

    # Windows 11's power mode is an overlay on the plan, chosen separately for
    # mains and battery power.
    $onBattery = $false
    try
    {
        Add-Type -AssemblyName System.Windows.Forms
        $onBattery = [System.Windows.Forms.SystemInformation]::PowerStatus.PowerLineStatus -eq "Offline"
    }
    catch
    {
        $onBattery = $false
    }
    $overlayName = if ($onBattery) { "ActiveOverlayDcPowerScheme" } else { "ActiveOverlayAcPowerScheme" }
    $powerSchemes = Get-ItemProperty -LiteralPath "HKLM:\SYSTEM\CurrentControlSet\Control\Power\User\PowerSchemes" `
        -ErrorAction SilentlyContinue
    $overlay = if ($null -ne $powerSchemes -and $powerSchemes.PSObject.Properties[$overlayName])
    {
        $powerSchemes.$overlayName
    }
    else
    {
        ""
    }
    $powerMode = switch ($overlay)
    {
        "961cc777-2547-4f9d-8174-7d86181b8a7a" { "Best power efficiency" }
        "ded574b5-45a0-4f42-8737-46345c09c238" { "Best performance" }
        { $_ -eq "" -or $_ -eq "00000000-0000-0000-0000-000000000000" } { "Balanced" }
        default { "unknown ($overlay)" }
    }
    $powerSource = if ($onBattery) { "battery" } else { "mains" }

    Write-Host "Processor: $processor"
    Write-Host "Power plan: $powerPlan; power mode: $powerMode, on $powerSource power"
}

# Resolve paths from this script's location so it can be invoked from any
# PowerShell working directory.
$projectRoot = Split-Path -Parent $PSScriptRoot
# CMakePresets.json defines each preset's build directory as build\<preset name>.
# Benchmarks are only meaningful with optimized code, so -Benchmark implies -Release.
$preset = if ($Release -or $Benchmark) { "release" } else { "default" }
$buildDirectory = Join-Path $projectRoot "build\$preset"
$applicationPath = Join-Path $buildDirectory "src\app\system_monitor.exe"
$benchmarkPaths = @(
    (Join-Path $buildDirectory "benchmarks\system_monitor_benchmark.exe"),
    (Join-Path $buildDirectory "benchmarks\system_monitor_process_benchmark.exe")
)

# Refresh process PATH from User and Machine values so that any tools installed
# by bootstrap.ps1 (such as ninja) are visible in the current session.
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathEntries = @($env:Path, $machinePath, $userPath) |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
    ForEach-Object { $_ -split ";" } |
    Select-Object -Unique
$env:Path = $pathEntries -join ";"

# Finds clang-format: CLANG_FORMAT, then PATH, then the LLVM tools that ship with
# Visual Studio ("C++ Clang tools for Windows"), then a standalone LLVM install.
function Find-ClangFormat
{
    if (-not [string]::IsNullOrWhiteSpace($env:CLANG_FORMAT))
    {
        if (-not (Test-Path -LiteralPath $env:CLANG_FORMAT))
        {
            throw "CLANG_FORMAT is set to '$env:CLANG_FORMAT', which does not exist."
        }
        return $env:CLANG_FORMAT
    }

    $command = Get-Command "clang-format" -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $command)
    {
        return $command.Source
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswherePath)
    {
        # Search every instance, newest first: the newest (e.g. Build Tools) may
        # not include the LLVM tools while another installation does. Collect all
        # output before taking the first line: piping into Select-Object -First
        # stops vswhere early, which often leaves $LASTEXITCODE at -1.
        $foundPaths = @(& $vswherePath -products * -sort -find "VC\Tools\Llvm\x64\bin\clang-format.exe")
        $found = $foundPaths | Select-Object -First 1
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($found))
        {
            return $found.Trim()
        }
    }

    $standalonePath = Join-Path $env:ProgramFiles "LLVM\bin\clang-format.exe"
    if (Test-Path -LiteralPath $standalonePath)
    {
        return $standalonePath
    }

    throw "clang-format was not found. Run .\scripts\bootstrap.ps1 -InstallMissing, or set CLANG_FORMAT to its path."
}

# Formats (-Format) or checks (-CheckFormat) all C++ files that are tracked or
# new and not ignored, then returns.
function Invoke-ClangFormat
{
    param(
        [Parameter(Mandatory)][bool]$IsCheckOnly
    )

    $clangFormat = Find-ClangFormat
    $versionText = (& $clangFormat --version | Out-String).Trim()
    Write-Host "Using $clangFormat ($versionText)"

    if ($versionText -match "version (\d+)\.")
    {
        $majorVersion = [int]$Matches[1]
        if ($majorVersion -ne $requiredClangFormatMajorVersion)
        {
            $message = "clang-format $majorVersion differs from the version CI uses ($requiredClangFormatMajorVersion); results may differ."
            if ($env:CI -eq "true")
            {
                throw $message
            }
            Write-Warning $message
        }
    }

    Push-Location $projectRoot
    try
    {
        # Tracked files plus new files not yet added (but not ignored ones), so
        # files created since the last commit are formatted before they are
        # committed. Tracked files deleted from the working tree are skipped.
        $files = @(& git ls-files --cached --others --exclude-standard -- "src/*.h" "src/*.cpp" "tests/*.h" "tests/*.cpp" `
                "benchmarks/*.h" "benchmarks/*.cpp")
        if ($LASTEXITCODE -ne 0)
        {
            throw "git ls-files failed with exit code $LASTEXITCODE."
        }
        $files = @($files | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })

        # Batches keep each command line well below the Windows length limit.
        # A check runs every batch before it fails, so that every unformatted
        # file is listed, not only those in the first failing batch.
        $batchSize = 50
        $failedBatches = 0
        for ($index = 0; $index -lt $files.Count; $index += $batchSize)
        {
            $batch = $files[$index..([Math]::Min($index + $batchSize, $files.Count) - 1)]
            if ($IsCheckOnly)
            {
                try
                {
                    Invoke-NativeCommand -Description "Format check" -FilePath $clangFormat `
                        -Arguments (@("--dry-run", "--Werror") + $batch)
                }
                catch
                {
                    $failedBatches++
                }
            }
            else
            {
                Invoke-NativeCommand -Description "Formatting" -FilePath $clangFormat -Arguments (@("-i") + $batch)
            }
        }

        if ($failedBatches -gt 0)
        {
            throw "Some files do not match .clang-format (see above). Run .\scripts\build.ps1 -Format to fix them."
        }
        if ($IsCheckOnly)
        {
            Write-Host "All $($files.Count) files match .clang-format."
        }
        else
        {
            Write-Host "Formatted $($files.Count) files."
        }
    }
    finally
    {
        Pop-Location
    }
}

# Returns the line number, from 1, of a character index in a text.
function Get-LineNumber
{
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][int]$Index
    )

    return $Text.Substring(0, $Index).Split("`n").Count
}

# Checks that links and citations of design decisions resolve (DC-114). Every
# "### DC-NN:" heading in docs\design_decisions.md needs <a id="dc-NN"></a> on
# the line above it. A Markdown link into that file must name one of these
# anchors or a "## " section heading. An entry heading's own slug changes when
# the entry is retitled, so a link to it fails. A link whose text is DC-NN must
# name that entry, and every DC-NN that a tracked or new text file cites must be
# an entry. An entry merged into another or retired is a stub (DC-115): only its
# line in the contents may link to it, and only that file and docs\code_reviews\
# may cite it. Prints each problem and returns whether there were none.
function Test-DesignDecisionLinks
{
    $decisionsName = "docs/design_decisions.md"
    $decisionsPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $decisionsName))
    $problems = New-Object System.Collections.Generic.List[string]

    # The entries' IDs, and what a link may name. URL fragments are case-sensitive.
    $entries = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::Ordinal)
    $targets = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::Ordinal)
    # Each stub's ID and the title that says where its decision is now.
    $stubs = New-Object "System.Collections.Generic.Dictionary[string,string]" ([StringComparer]::Ordinal)
    $decisionLines = [System.IO.File]::ReadAllLines($decisionsPath)
    for ($index = 0; $index -lt $decisionLines.Count; $index++)
    {
        $line = $decisionLines[$index]
        if ($line -cmatch '^<a id="([^"]+)"></a>$')
        {
            if (-not $targets.Add($Matches[1]))
            {
                $problems.Add("${decisionsName}:$($index + 1): the anchor $($Matches[1]) is defined more than once.")
            }
        }
        elseif ($line -cmatch '^### DC-(\d+):\s*(.*)$')
        {
            $number = $Matches[1]
            $title = $Matches[2]
            [void]$entries.Add("DC-$number")
            if ($title -cmatch '^(Merged into|Retired)\b')
            {
                $stubs["DC-$number"] = $title
            }
            $anchor = "<a id=`"dc-$number`"></a>"
            if ($index -eq 0 -or $decisionLines[$index - 1] -cne $anchor)
            {
                $problems.Add("${decisionsName}:$($index + 1): DC-$number needs $anchor on the line above its heading.")
            }
        }
        elseif ($line -cmatch '^## (.+)$')
        {
            # GitHub's heading slug: lower case, without characters other than
            # letters, digits, underscores, hyphens and spaces, and with each
            # space replaced by a hyphen.
            $slug = ($Matches[1].ToLowerInvariant() -replace '[^\p{L}\p{M}\p{N}\p{Pc} -]', '') -replace ' ', '-'
            [void]$targets.Add($slug)
        }
    }

    Push-Location $projectRoot
    try
    {
        $files = @(& git ls-files --cached --others --exclude-standard -- "*.md" "*.h" "*.cpp" "*.txt" "*.cmake" `
                "*.json" "*.yml" "*.ps1" "*.manifest")
        if ($LASTEXITCODE -ne 0)
        {
            throw "git ls-files failed with exit code $LASTEXITCODE."
        }
        $files = @($files | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    }
    finally
    {
        Pop-Location
    }

    foreach ($file in $files)
    {
        $path = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $file))
        $text = [System.IO.File]::ReadAllText($path)
        $isDecisions = $path -eq $decisionsPath
        # The code reviews record what each review led to at the time.
        $isReview = $file.StartsWith("docs/code_reviews/")
        $mayCiteStubs = $isDecisions -or $isReview

        foreach ($citation in [regex]::Matches($text, '\bDC-\d+'))
        {
            if (-not $entries.Contains($citation.Value))
            {
                $problems.Add("${file}:$(Get-LineNumber $text $citation.Index): " +
                    "$($citation.Value) is not an entry in $decisionsName.")
            }
            elseif (-not $mayCiteStubs -and $stubs.ContainsKey($citation.Value))
            {
                $problems.Add("${file}:$(Get-LineNumber $text $citation.Index): " +
                    "$($citation.Value) is a stub ($($stubs[$citation.Value])); cite the entry that has the decision now.")
            }
        }

        if (-not $file.EndsWith(".md"))
        {
            continue
        }
        foreach ($link in [regex]::Matches($text, '\[([^\]]*)\]\(([^)\s]*)\)'))
        {
            $linkText = $link.Groups[1].Value
            $destination = $link.Groups[2].Value
            $hashIndex = $destination.IndexOf("#")
            $linkPath = if ($hashIndex -ge 0) { $destination.Substring(0, $hashIndex) } else { $destination }
            $fragment = if ($hashIndex -ge 0) { $destination.Substring($hashIndex + 1) } else { $null }
            $citedEntry = if ($linkText -cmatch '^DC-(\d+)$') { "dc-$($Matches[1])" } else { $null }

            # Only a relative path can lead to the file; a URL's colon would
            # make GetFullPath throw.
            $leadsToDecisions = if ($linkPath -eq "")
            {
                $path -eq $decisionsPath
            }
            elseif ($linkPath.EndsWith("design_decisions.md") -and -not $linkPath.Contains(":"))
            {
                $linkedPath = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($path), $linkPath)
                [System.IO.Path]::GetFullPath($linkedPath) -eq $decisionsPath
            }
            else
            {
                $false
            }

            $problem = $null
            if (-not $leadsToDecisions)
            {
                if ($null -ne $citedEntry -or $linkPath.EndsWith("design_decisions.md"))
                {
                    $problem = "does not lead to $decisionsName"
                }
            }
            elseif ($null -ne $fragment -and -not $targets.Contains($fragment))
            {
                $problem = "names no anchor or section heading in $decisionsName"
            }
            elseif ($null -ne $citedEntry -and $fragment -cne $citedEntry)
            {
                $problem = "should end in #$citedEntry"
            }
            elseif (-not $isReview -and $null -ne $fragment -and $fragment -cmatch '^dc-(\d+)$' -and
                $stubs.ContainsKey("DC-$($Matches[1])"))
            {
                $stubId = "DC-$($Matches[1])"
                # The contents list each entry once, stubs included, as
                # "  - [DC-NN](#dc-NN): <title>".
                $lineStart = $text.LastIndexOf("`n", [Math]::Max($link.Index - 1, 0)) + 1
                $linePrefix = $text.Substring($lineStart, $link.Index - $lineStart)
                $isContentsLine = $isDecisions -and $linkText -ceq $stubId -and $linePrefix -cmatch '^\s*- $'
                if (-not $isContentsLine)
                {
                    $problem = "leads to a stub ($($stubs[$stubId])); link to the entry that has the decision now"
                }
            }
            if ($null -ne $problem)
            {
                $problems.Add("${file}:$(Get-LineNumber $text $link.Index): [$linkText]($destination) $problem.")
            }
        }
    }

    foreach ($problem in $problems)
    {
        Write-Host $problem
    }
    if ($problems.Count -eq 0)
    {
        Write-Host "All design decision links and citations in $($files.Count) files resolve."
    }
    return $problems.Count -eq 0
}

if ($Format -and $CheckFormat)
{
    throw "Use either -Format or -CheckFormat, not both."
}

# Formatting and the link check need neither vcpkg nor the MSVC environment, so
# they run first and exit. Both run before either fails, so that every problem
# is listed.
if ($Format -or $CheckFormat)
{
    $formatError = $null
    try
    {
        Invoke-ClangFormat -IsCheckOnly $CheckFormat.IsPresent
    }
    catch
    {
        $formatError = $_
    }
    $linksResolve = Test-DesignDecisionLinks
    if ($null -ne $formatError)
    {
        throw $formatError
    }
    if (-not $linksResolve)
    {
        throw "Some design decision links or citations do not resolve or name a stub (see above)."
    }
    return
}

# The CMake presets read this variable to locate vcpkg's CMake toolchain.
# Check user/machine environment variables if not yet set in the current process.
if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT))
{
    $env:VCPKG_ROOT = [Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
    if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT))
    {
        $env:VCPKG_ROOT = [Environment]::GetEnvironmentVariable("VCPKG_ROOT", "Machine")
    }
}

if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT))
{
    throw "VCPKG_ROOT is not set. Run .\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment, then open a new PowerShell window."
}

# Without an initialized x64 MSVC environment, CMake silently falls back to the
# x86 toolset. vcpkg then targets x86-windows and the build can fail later with
# confusing linker errors (e.g. LNK1104) instead of a clear message here. Import
# it into this process so the script works from any terminal, including VS
# Code's built-in one, without requiring a separate Developer Prompt.
function Import-X64DevShell
{
    if ($env:VSCMD_ARG_TGT_ARCH -eq "x64")
    {
        return
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswherePath))
    {
        throw "vswhere.exe was not found. Install Visual Studio Build Tools with the C++ workload (see scripts\bootstrap.ps1)."
    }

    $vsInstallPath = & $vswherePath -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($vsInstallPath))
    {
        throw "No Visual Studio installation with the C++ workload was found. Run .\scripts\bootstrap.ps1 -InstallMissing."
    }
    $vsInstallPath = $vsInstallPath.Trim()

    # VsDevCmd.bat invokes vswhere.exe by name; without its directory on PATH it
    # prints "'vswhere.exe' is not recognized" on every run.
    $env:Path = "$(Split-Path -Parent $vswherePath);$env:Path"

    # Visual Studio's vcpkg extension (vsdevcmd\ext\vcpkg.bat) points VCPKG_ROOT
    # at the vcpkg bundled with Visual Studio. The presets would then use that
    # toolchain instead of the configured one and fail to find dependencies.
    $vcpkgRoot = $env:VCPKG_ROOT

    $devShellModule = Join-Path $vsInstallPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation `
        -DevCmdArguments "-arch=x64 -host_arch=x64 -no_logo"

    $env:VCPKG_ROOT = $vcpkgRoot

    if ($env:VSCMD_ARG_TGT_ARCH -ne "x64")
    {
        throw "Failed to initialize an x64 MSVC developer environment."
    }
}

Import-X64DevShell

# Configuring takes about 3 seconds even when nothing changed, so it runs only
# when needed. Ninja re-runs it by itself when a CMakeLists.txt or vcpkg.json
# changes (vcpkg registers its manifest as a configure dependency), but it does
# not track the presets files. The script stamps each successful configure
# itself, because CMake rewrites CMakeCache.txt only when its contents change.
$configureStampPath = Join-Path $buildDirectory "build_ps1_configure.stamp"

function Test-ConfigureNeeded
{
    if ($Configure)
    {
        return $true
    }

    foreach ($requiredPath in @($configureStampPath, (Join-Path $buildDirectory "build.ninja")))
    {
        if (-not (Test-Path -LiteralPath $requiredPath))
        {
            return $true
        }
    }

    $configureTime = (Get-Item -LiteralPath $configureStampPath).LastWriteTimeUtc
    foreach ($presetsName in @("CMakePresets.json", "CMakeUserPresets.json"))
    {
        $presetsPath = Join-Path $projectRoot $presetsName
        if ((Test-Path -LiteralPath $presetsPath) -and
            (Get-Item -LiteralPath $presetsPath).LastWriteTimeUtc -gt $configureTime)
        {
            return $true
        }
    }

    return $false
}

# Configures with the preset. A configure can fail on an existing cache that a
# clean one would accept. A cache created for another source directory (a moved
# or copied checkout) is one case. Another: when a cached compiler differs from
# the preset's "cl" (seen after a Visual Studio update), CMake deletes its cache
# and re-runs, the re-run does not find vcpkg's packages, and the cache it
# leaves behind fails every later configure the same way. So a failure with an
# existing cache is retried once from a clean cache. vcpkg_installed is kept,
# so dependencies are not restored again.
function Invoke-Configure
{
    # A configure that fails must not leave an earlier stamp claiming the build
    # directory is up to date.
    if (Test-Path -LiteralPath $configureStampPath)
    {
        Remove-Item -LiteralPath $configureStampPath -Force
    }

    $cachePath = Join-Path $buildDirectory "CMakeCache.txt"
    $hadCache = Test-Path -LiteralPath $cachePath
    try
    {
        Invoke-NativeCommand -Description "CMake configuration" -FilePath "cmake" -Arguments @("--preset", $preset)
    }
    catch
    {
        if (-not $hadCache)
        {
            throw
        }
        Write-Warning ("CMake configuration failed with an existing cache in build\$preset. " +
            "Retrying once with CMakeCache.txt and CMakeFiles removed.")
        if (Test-Path -LiteralPath $cachePath)
        {
            Remove-Item -LiteralPath $cachePath -Force
        }
        $cmakeFilesPath = Join-Path $buildDirectory "CMakeFiles"
        if (Test-Path -LiteralPath $cmakeFilesPath)
        {
            Remove-Item -LiteralPath $cmakeFilesPath -Recurse -Force
        }
        Invoke-NativeCommand -Description "CMake configuration from a clean cache" -FilePath "cmake" `
            -Arguments @("--preset", $preset)
    }

    Set-Content -LiteralPath $configureStampPath -Value "Last configured by scripts\build.ps1." -Encoding ASCII
}

Push-Location $projectRoot
try
{
    # Configure generates Ninja build files and causes vcpkg to restore missing
    # dependencies from its binary cache or build them as needed.
    if (Test-ConfigureNeeded)
    {
        Invoke-Configure
    }
    else
    {
        Write-Host "Skipping CMake configuration: build\$preset is up to date (use -Configure to force it)."
    }

    # Build the default target with the matching build preset.
    Invoke-NativeCommand -Description "CMake build" -FilePath "cmake" -Arguments @("--build", "--preset", $preset)

    if ($Test)
    {
        # The tests run as GoogleTest shards, about two per logical processor
        # (tests\CMakeLists.txt), so running one shard per processor at a time
        # keeps every processor busy until the slowest shards finish.
        Invoke-NativeCommand -Description "Tests" -FilePath "ctest" `
            -Arguments @("--preset", $preset, "--output-on-failure", "--parallel", [Environment]::ProcessorCount)
    }

    if ($Benchmark)
    {
        Write-BenchmarkMachine
        foreach ($benchmarkPath in $benchmarkPaths)
        {
            if (-not (Test-Path -LiteralPath $benchmarkPath))
            {
                throw "The benchmark executable was not found at '$benchmarkPath'."
            }
            Invoke-NativeCommand -Description "Benchmark" -FilePath $benchmarkPath
        }
    }

    if (-not $Run)
    {
        return
    }

    # Qt places the executable in the app module's build directory. Verify it
    # exists before attempting to launch it for a clearer failure message.
    if (-not (Test-Path -LiteralPath $applicationPath))
    {
        throw "The application executable was not found at '$applicationPath'."
    }

    & $applicationPath
}
finally
{
    # Restore the caller's original directory even when configuration or build
    # commands fail.
    Pop-Location
}
