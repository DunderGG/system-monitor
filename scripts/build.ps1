<#
.SYNOPSIS
Configures, builds, and optionally runs the System Monitor desktop application.

.DESCRIPTION
Runs CMake's default preset, which uses vcpkg to resolve project dependencies
and Ninja/MSVC to compile the executable. Unless -NoRun is supplied, the script
launches the executable after a successful build.

Run scripts/bootstrap.ps1 first to install prerequisites and configure the
VCPKG_ROOT environment variable.

.PARAMETER NoRun
Configures and builds the application without launching it.

.PARAMETER Test
Runs the test suite with CTest after a successful build. The script fails if
any test fails. Combine with -NoRun to build and test without launching.

.PARAMETER Format
Formats every C++ source and header under src/ and tests/ in place with
clang-format, using the repository's .clang-format, then exits without building.
New files count even before they are added to git; files ignored by .gitignore
do not.

.PARAMETER CheckFormat
Checks that every C++ source and header under src/ and tests/ (the same files
as -Format) matches
.clang-format, without changing files, then exits without building. Fails and
lists the differences if any file needs formatting. CI runs this check.

clang-format is taken from the CLANG_FORMAT environment variable if set, then
PATH, then the Visual Studio LLVM tools, then %ProgramFiles%\LLVM. CI pins major
version 22; other versions may format differently. In CI a different major
version is an error; locally it is a warning.

.EXAMPLE
.\scripts\build.ps1

.EXAMPLE
.\scripts\build.ps1 -NoRun

.EXAMPLE
.\scripts\build.ps1 -NoRun -Test

.EXAMPLE
.\scripts\build.ps1 -Format

.EXAMPLE
.\scripts\build.ps1 -CheckFormat
#>
[CmdletBinding()]
param(
    [switch]$NoRun,
    [switch]$Test,
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

# Resolve paths from this script's location so it can be invoked from any
# PowerShell working directory.
$projectRoot = Split-Path -Parent $PSScriptRoot
$applicationPath = Join-Path $projectRoot "build\default\src\app\system_monitor.exe"

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
        $files = @(& git ls-files --cached --others --exclude-standard -- "src/*.h" "src/*.cpp" "tests/*.h" "tests/*.cpp")
        if ($LASTEXITCODE -ne 0)
        {
            throw "git ls-files failed with exit code $LASTEXITCODE."
        }
        $files = @($files | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })

        # Batches keep each command line well below the Windows length limit.
        $batchSize = 50
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
                    throw "Some files do not match .clang-format (see above). Run .\scripts\build.ps1 -Format to fix them."
                }
            }
            else
            {
                Invoke-NativeCommand -Description "Formatting" -FilePath $clangFormat -Arguments (@("-i") + $batch)
            }
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

if ($Format -and $CheckFormat)
{
    throw "Use either -Format or -CheckFormat, not both."
}

# Formatting needs neither vcpkg nor the MSVC environment, so it runs first and exits.
if ($Format -or $CheckFormat)
{
    Invoke-ClangFormat -IsCheckOnly $CheckFormat.IsPresent
    return
}

# The default CMake preset reads this variable to locate vcpkg's CMake toolchain.
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

    $devShellModule = Join-Path $vsInstallPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $vsInstallPath -SkipAutomaticLocation `
        -DevCmdArguments "-arch=x64 -host_arch=x64 -no_logo"

    if ($env:VSCMD_ARG_TGT_ARCH -ne "x64")
    {
        throw "Failed to initialize an x64 MSVC developer environment."
    }
}

Import-X64DevShell

Push-Location $projectRoot
try
{
    # Configure generates Ninja build files and causes vcpkg to restore missing
    # dependencies from its binary cache or build them as needed.
    Invoke-NativeCommand -Description "CMake configuration" -FilePath "cmake" -Arguments @("--preset", "default")

    # Build the default target defined by the project's default build preset.
    Invoke-NativeCommand -Description "CMake build" -FilePath "cmake" -Arguments @("--build", "--preset", "default")

    if ($Test)
    {
        Invoke-NativeCommand -Description "Tests" -FilePath "ctest" `
            -Arguments @("--preset", "default", "--output-on-failure")
    }

    if ($NoRun)
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
