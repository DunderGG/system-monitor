<#
.SYNOPSIS
Prepares a Windows development environment for building System Monitor.

.DESCRIPTION
Checks whether the required command-line tools, Visual C++ Build Tools, and
vcpkg are present. With -InstallMissing, it uses WinGet to install missing
prerequisites, clones and bootstraps vcpkg, and creates a local binary cache.

The script intentionally does not install this project's dependencies directly.
CMake installs the versions declared in vcpkg.json during project configuration.
Run this script once before using scripts/build.ps1 or invoking CMake manually.

.PARAMETER InstallMissing
Allows WinGet and Git to install missing prerequisites and vcpkg.

.PARAMETER PersistEnvironment
Saves VCPKG_ROOT and VCPKG_DEFAULT_BINARY_CACHE for future terminals.

.PARAMETER VcpkgRoot
Directory in which vcpkg is installed. Automatically detects existing installations
(Visual Studio, vcpkg.path.txt, or VCPKG_ROOT), or defaults to %LOCALAPPDATA%\vcpkg-root.

.PARAMETER BinaryCache
Directory used for vcpkg binary caching. Defaults to %LOCALAPPDATA%\vcpkg-cache.

.EXAMPLE
.\scripts\bootstrap.ps1 -InstallMissing -PersistEnvironment
#>
[CmdletBinding()]
param(
    [switch]$InstallMissing,
    [switch]$PersistEnvironment,
    [string]$VcpkgRoot,
    [string]$BinaryCache = (Join-Path $env:LOCALAPPDATA "vcpkg-cache")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Returns whether a command can be resolved from the current PowerShell PATH.
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
    # Refresh PATH after WinGet installs a tool so it can be used immediately
    # in this PowerShell process. The persisted user and machine values remain
    # unchanged by this function.
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathEntries = @($env:Path, $machinePath, $userPath) |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ -split ";" } |
        Select-Object -Unique
    $env:Path = $pathEntries -join ";"
}

# Refresh PATH immediately so any tools previously installed by WinGet are available.
Update-ProcessPath

function Test-ValidVcpkgRoot
{
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path))
    {
        return $false
    }

    $toolchain = Join-Path $Path "scripts\buildsystems\vcpkg.cmake"
    $hasToolchain = Test-Path -LiteralPath $toolchain
    $hasExe = Test-Path -LiteralPath (Join-Path $Path "vcpkg.exe")
    $hasBootstrap = Test-Path -LiteralPath (Join-Path $Path "bootstrap-vcpkg.bat")

    return $hasToolchain -and ($hasExe -or $hasBootstrap)
}

function Find-ExistingVcpkgRoot
{
    param(
        [string]$VsInstallPath
    )

    # 1. Active environment variable
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT) -and (Test-ValidVcpkgRoot $env:VCPKG_ROOT))
    {
        return $env:VCPKG_ROOT
    }

    # 2. Persisted User or Machine environment variables
    $userVcpkg = [Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
    if (-not [string]::IsNullOrWhiteSpace($userVcpkg) -and (Test-ValidVcpkgRoot $userVcpkg))
    {
        return $userVcpkg
    }
    $machineVcpkg = [Environment]::GetEnvironmentVariable("VCPKG_ROOT", "Machine")
    if (-not [string]::IsNullOrWhiteSpace($machineVcpkg) -and (Test-ValidVcpkgRoot $machineVcpkg))
    {
        return $machineVcpkg
    }

    # 3. Registered vcpkg path from %LOCALAPPDATA%\vcpkg\vcpkg.path.txt
    $vcpkgPathFile = Join-Path $env:LOCALAPPDATA "vcpkg\vcpkg.path.txt"
    if (Test-Path -LiteralPath $vcpkgPathFile)
    {
        $registeredPath = (Get-Content -LiteralPath $vcpkgPathFile -Raw -ErrorAction SilentlyContinue)
        if (-not [string]::IsNullOrWhiteSpace($registeredPath))
        {
            $registeredPath = $registeredPath.Trim()
            if (Test-ValidVcpkgRoot $registeredPath)
            {
                return $registeredPath
            }
        }
    }

    # 4. Visual Studio bundled vcpkg
    if (-not [string]::IsNullOrWhiteSpace($VsInstallPath))
    {
        $vsVcpkg = Join-Path $VsInstallPath "VC\vcpkg"
        if (Test-ValidVcpkgRoot $vsVcpkg)
        {
            return $vsVcpkg
        }
    }

    # 5. vcpkg executable on PATH
    $vcpkgCmd = Get-Command "vcpkg" -ErrorAction SilentlyContinue
    if ($null -ne $vcpkgCmd)
    {
        $vcpkgDir = Split-Path -Parent $vcpkgCmd.Source
        if (Test-ValidVcpkgRoot $vcpkgDir)
        {
            return $vcpkgDir
        }
    }

    return $null
}

function Find-VsInstallPath
{
    # Locate a Visual Studio installation that includes the MSVC x64/x86 tools.
    # vswhere ships with Visual Studio and Build Tools installations.
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
    # Install a prerequisite non-interactively while accepting WinGet's package
    # and source agreements. Callers verify the resulting command afterward.
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
    # Ensure a command-line prerequisite exists. Installation is opt-in so a
    # default invocation remains a safe diagnostic check.
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

# CMake's default preset uses Ninja and requires MSVC to compile the C++
# application and its dependencies.
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

# Locate an existing vcpkg installation or choose a non-conflicting default
# checkout directory outside the repository.
if ([string]::IsNullOrWhiteSpace($VcpkgRoot))
{
    $existingVcpkg = Find-ExistingVcpkgRoot -VsInstallPath $vsInstallPath
    if ($null -ne $existingVcpkg)
    {
        $VcpkgRoot = $existingVcpkg
        Write-Host "Found existing vcpkg at $VcpkgRoot."
    }
    else
    {
        # Default checkout directory when no existing vcpkg installation is found.
        # Note: We use vcpkg-root rather than %LOCALAPPDATA%\vcpkg because the latter
        # is reserved by vcpkg for user-level caches, registries, and configuration.
        $VcpkgRoot = Join-Path $env:LOCALAPPDATA "vcpkg-root"
    }
}

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
    # A source checkout needs a one-time bootstrap step to build vcpkg.exe.
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

# Binary caching avoids rebuilding already-compiled vcpkg packages on later
# configurations. These assignments make the cache available immediately.
New-Item -ItemType Directory -Force -Path $BinaryCache | Out-Null
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DEFAULT_BINARY_CACHE = $BinaryCache

if ($PersistEnvironment)
{
    # Persist the locations for new PowerShell windows. The current process was
    # already configured above, so it can continue without being restarted.
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgRoot, "User")
    [Environment]::SetEnvironmentVariable("VCPKG_DEFAULT_BINARY_CACHE", $BinaryCache, "User")
    Write-Host "Persisted VCPKG_ROOT and VCPKG_DEFAULT_BINARY_CACHE for future terminals."
}

Write-Host "Bootstrap completed successfully."
Write-Host "VCPKG_ROOT=$VcpkgRoot"
Write-Host "VCPKG_DEFAULT_BINARY_CACHE=$BinaryCache"
Write-Host "Run: .\scripts\build.ps1"
Write-Host "It initializes the x64 MSVC environment automatically, so any terminal works."
