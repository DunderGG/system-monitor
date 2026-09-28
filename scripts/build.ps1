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

.EXAMPLE
.\scripts\build.ps1

.EXAMPLE
.\scripts\build.ps1 -NoRun

.EXAMPLE
.\scripts\build.ps1 -NoRun -Test
#>
[CmdletBinding()]
param(
    [switch]$NoRun,
    [switch]$Test
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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
