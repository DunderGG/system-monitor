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

.EXAMPLE
.\scripts\build.ps1

.EXAMPLE
.\scripts\build.ps1 -NoRun
#>
[CmdletBinding()]
param(
    [switch]$NoRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Resolve paths from this script's location so it can be invoked from any
# PowerShell working directory.
$projectRoot = Split-Path -Parent $PSScriptRoot
$applicationPath = Join-Path $projectRoot "build\default\src\app\system_monitor.exe"

# The default CMake preset reads this variable to locate vcpkg's CMake toolchain.
# Failing early makes a missing bootstrap step easier to diagnose.
if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT))
{
    throw "VCPKG_ROOT is not set. Run .\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment, then open a new PowerShell window."
}

Push-Location $projectRoot
try
{
    # Configure generates Ninja build files and causes vcpkg to restore missing
    # dependencies from its binary cache or build them as needed.
    & cmake --preset default
    if ($LASTEXITCODE -ne 0)
    {
        throw "CMake configuration failed."
    }

    # Build the default target defined by the project's default build preset.
    & cmake --build --preset default
    if ($LASTEXITCODE -ne 0)
    {
        throw "CMake build failed."
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
