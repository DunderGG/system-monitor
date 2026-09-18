<#
.SYNOPSIS
Checks or installs the Windows build prerequisites for System Monitor.

.DESCRIPTION
By default, the script checks for the required tools. With -InstallMissing, it
uses WinGet to install missing tools, installs vcpkg in the selected directory,
and creates a vcpkg binary cache. It never installs project dependencies
directly; CMake and the vcpkg manifest do that during configuration.

.PARAMETER InstallMissing
Allows WinGet and Git to install missing prerequisites and vcpkg.

.PARAMETER PersistEnvironment
Saves VCPKG_ROOT and VCPKG_DEFAULT_BINARY_CACHE for future terminals.

.PARAMETER VcpkgRoot
Directory in which vcpkg is installed. Defaults to %LOCALAPPDATA%\vcpkg.

.PARAMETER BinaryCache
Directory used for vcpkg binary caching. Defaults to %LOCALAPPDATA%\vcpkg-cache.

.EXAMPLE
.\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
#>
[CmdletBinding()]
param(
    [switch]$InstallMissing,
    [switch]$PersistEnvironment,
    [string]$VcpkgRoot = (Join-Path $env:LOCALAPPDATA "vcpkg"),
    [string]$BinaryCache = (Join-Path $env:LOCALAPPDATA "vcpkg-cache")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-CommandAvailable
{
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Update-ProcessPath
{
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathEntries = @($env:Path, $machinePath, $userPath) |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ -split ";" } |
        Select-Object -Unique
    $env:Path = $pathEntries -join ";"
}

function Find-VsInstallPath
{
    $vswherePaths = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    )

    foreach ($vswherePath in $vswherePaths)
    {
        if (Test-Path -LiteralPath $vswherePath)
        {
            $installPath = & $vswherePath -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath
            if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installPath))
            {
                return $installPath.Trim()
            }
        }
    }

    return $null
}

function Install-WingetPackage
{
    param(
        [Parameter(Mandatory)]
        [string]$Id,
        [string[]]$AdditionalArguments = @()
    )

    & winget install --exact --id $Id --accept-package-agreements --accept-source-agreements @AdditionalArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "winget could not install '$Id'. Install it manually, then rerun this script."
    }
}

function Require-Tool
{
    param(
        [Parameter(Mandatory)]
        [string]$Command,
        [Parameter(Mandatory)]
        [string]$WingetId
    )

    if (Test-CommandAvailable $Command)
    {
        Write-Host "Found $Command."
        return
    }

    if (-not $InstallMissing)
    {
        throw "'$Command' was not found. Rerun with -InstallMissing or install it manually."
    }

    if (-not (Test-CommandAvailable "winget"))
    {
        throw "winget is required to install '$Command' automatically. Install it manually, then rerun this script."
    }

    Write-Host "Installing $Command..."
    Install-WingetPackage -Id $WingetId
    Update-ProcessPath
    if (-not (Test-CommandAvailable $Command))
    {
        throw "'$Command' was installed but is not available in this PowerShell session. Open a new terminal and rerun this script."
    }
}

Require-Tool -Command "git" -WingetId "Git.Git"
Require-Tool -Command "cmake" -WingetId "Kitware.CMake"
Require-Tool -Command "ninja" -WingetId "Ninja-build.Ninja"

$vsInstallPath = Find-VsInstallPath
if ($null -eq $vsInstallPath)
{
    if (-not $InstallMissing)
    {
        throw "Visual Studio Build Tools with the C++ workload was not found. Rerun with -InstallMissing or install it manually."
    }

    if (-not (Test-CommandAvailable "winget"))
    {
        throw "winget is required to install Visual Studio Build Tools automatically. Install it manually, then rerun this script."
    }

    Write-Host "Installing Visual Studio Build Tools with the C++ workload..."
    Install-WingetPackage -Id "Microsoft.VisualStudio.2022.BuildTools" -AdditionalArguments @(
        "--override",
        "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    )
    $vsInstallPath = Find-VsInstallPath
    if ($null -eq $vsInstallPath)
    {
        throw "Visual Studio Build Tools installation could not be verified. Restart Windows if prompted, then rerun this script."
    }
}

Write-Host "Found MSVC Build Tools at $vsInstallPath."

if (-not (Test-Path -LiteralPath $VcpkgRoot))
{
    if (-not $InstallMissing)
    {
        throw "vcpkg was not found at '$VcpkgRoot'. Rerun with -InstallMissing or provide -VcpkgRoot."
    }

    Write-Host "Cloning vcpkg into $VcpkgRoot..."
    & git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
    if ($LASTEXITCODE -ne 0)
    {
        throw "Could not clone vcpkg. Check network access and rerun this script."
    }
}

$vcpkgExecutable = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path -LiteralPath $vcpkgExecutable))
{
    $bootstrapScript = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
    if (-not (Test-Path -LiteralPath $bootstrapScript))
    {
        throw "'$VcpkgRoot' is not a valid vcpkg checkout. Use a different -VcpkgRoot path."
    }

    Write-Host "Bootstrapping vcpkg..."
    & $bootstrapScript -disableMetrics
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $vcpkgExecutable))
    {
        throw "vcpkg bootstrap failed."
    }
}

New-Item -ItemType Directory -Force -Path $BinaryCache | Out-Null
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DEFAULT_BINARY_CACHE = $BinaryCache

if ($PersistEnvironment)
{
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgRoot, "User")
    [Environment]::SetEnvironmentVariable("VCPKG_DEFAULT_BINARY_CACHE", $BinaryCache, "User")
    Write-Host "Persisted VCPKG_ROOT and VCPKG_DEFAULT_BINARY_CACHE for future terminals."
}

Write-Host "Bootstrap completed successfully."
Write-Host "VCPKG_ROOT=$VcpkgRoot"
Write-Host "VCPKG_DEFAULT_BINARY_CACHE=$BinaryCache"
Write-Host "For command-line builds, use a Developer PowerShell or Developer Command Prompt."
Write-Host "Then run: cmake --preset default"
