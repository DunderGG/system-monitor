<#
.SYNOPSIS
Points vcpkg at the package cache that CI restores, and prunes the cache before CI saves it.

.DESCRIPTION
CI keeps vcpkg's binary cache, a zip archive per package named by the package's
ABI hash, in a directory that actions/cache restores at the start of a job and
saves after the steps that run vcpkg (DC-140). vcpkg restores a package from an
archive with its ABI hash, and writes there the archive of each package it
builds.

-Prepare, after the restore and before run-vcpkg, creates the directory if the
restore found no cache, notes which archives it holds, and sets
VCPKG_BINARY_SOURCES to read and write that directory alone. run-vcpkg leaves a
VCPKG_BINARY_SOURCES that is already set as it is. In a GitHub Actions job the
variable goes to GITHUB_ENV, for the job's later steps; elsewhere the script
prints it.

-Prune, after the steps that run vcpkg, removes the restored archives that no
installed package uses. A package's ABI hash is the SHA-256 of the
vcpkg_abi_info.txt that vcpkg installs with it, under vcpkg_installed in each
build directory. Only the build directories whose configure finished, which
have a build.ninja, are read, so that a configure that failed before vcpkg
installed every package removes nothing. The archives that vcpkg wrote in this
job are kept. In a GitHub Actions job, the step output changed is true when
vcpkg wrote an archive or one was removed, so that the job saves the directory
only then.

.PARAMETER Prepare
Notes the restored archives and sets VCPKG_BINARY_SOURCES.

.PARAMETER Prune
Removes the restored archives that no installed package uses, and says whether
the cache changed.

.PARAMETER CacheDirectory
The directory that actions/cache restores and saves, outside the checkout, such
as $env:RUNNER_TEMP\vcpkg-binary-cache. vcpkg would read a comma, a semicolon or
a backtick in it as syntax, so it may have none.

.PARAMETER BuildDirectory
The directory that holds the presets' build directories. By default it is the
checkout's build directory.

.EXAMPLE
.\scripts\vcpkg_cache.ps1 -Prepare -CacheDirectory $env:RUNNER_TEMP\vcpkg-binary-cache
cmake --preset default
.\scripts\vcpkg_cache.ps1 -Prune -CacheDirectory $env:RUNNER_TEMP\vcpkg-binary-cache
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, ParameterSetName = "Prepare")][switch]$Prepare,
    [Parameter(Mandatory, ParameterSetName = "Prune")][switch]$Prune,
    [Parameter(Mandatory)][string]$CacheDirectory,
    [Parameter(ParameterSetName = "Prune")][string]$BuildDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$utf8 = New-Object System.Text.UTF8Encoding $false
$CacheDirectory = [System.IO.Path]::GetFullPath($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($CacheDirectory))
# Beside the directory rather than in it, so that it is not saved with the
# archives.
$restoredListPath = "$CacheDirectory.restored.txt"

# Returns the archives in the cache directory, where vcpkg keeps each in a
# subdirectory named by the first two characters of its ABI hash.
function Get-Archive
{
    if (-not (Test-Path -LiteralPath $CacheDirectory))
    {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $CacheDirectory -Recurse -File -Filter "*.zip")
}

function Format-ArchiveCount
{
    param([object[]]$Archives)

    $bytes = 0
    foreach ($archive in $Archives)
    {
        $bytes += $archive.Length
    }
    $noun = if ($Archives.Count -eq 1) { "package" } else { "packages" }
    return "{0} {1} ({2:N0} MB)" -f $Archives.Count, $noun, ($bytes / 1MB)
}

# Appends a name=value line to the file that GITHUB_ENV or GITHUB_OUTPUT names,
# without the byte order mark that Windows PowerShell's Out-File would write.
function Add-GitHubFileLine
{
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Line
    )

    [System.IO.File]::AppendAllText($Path, "$Line`n", $utf8)
}

if ($Prepare)
{
    if ($CacheDirectory -match '[,;`]')
    {
        throw "$CacheDirectory has a comma, a semicolon or a backtick, which vcpkg would read as syntax in VCPKG_BINARY_SOURCES; give another directory."
    }

    New-Item -ItemType Directory -Force -Path $CacheDirectory | Out-Null
    $archives = Get-Archive
    [System.IO.File]::WriteAllLines($restoredListPath, [string[]]@($archives | ForEach-Object { $_.Name }), $utf8)
    if ($archives.Count -eq 0)
    {
        Write-Host "The cache holds no packages, so vcpkg builds each one."
    }
    else
    {
        Write-Host "The cache holds $(Format-ArchiveCount $archives)."
    }

    $binarySources = "clear;files,$CacheDirectory,readwrite"
    if ([string]::IsNullOrEmpty($env:GITHUB_ENV))
    {
        Write-Host "Set VCPKG_BINARY_SOURCES to $binarySources before vcpkg runs."
    }
    else
    {
        Add-GitHubFileLine -Path $env:GITHUB_ENV -Line "VCPKG_BINARY_SOURCES=$binarySources"
        Write-Host "VCPKG_BINARY_SOURCES is $binarySources in the job's later steps."
    }
    return
}

if (-not (Test-Path -LiteralPath $restoredListPath))
{
    throw "$restoredListPath does not exist; run the script with -Prepare before vcpkg runs."
}
$restored = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($name in [System.IO.File]::ReadAllLines($restoredListPath))
{
    if ($name -ne "")
    {
        [void]$restored.Add($name)
    }
}

if ($BuildDirectory -eq "")
{
    $BuildDirectory = Join-Path $projectRoot "build"
}
$used = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
if (Test-Path -LiteralPath $BuildDirectory)
{
    foreach ($directory in Get-ChildItem -LiteralPath $BuildDirectory -Directory)
    {
        $installedDirectory = Join-Path $directory.FullName "vcpkg_installed"
        if (-not (Test-Path -LiteralPath (Join-Path $directory.FullName "build.ninja")) -or
            -not (Test-Path -LiteralPath $installedDirectory))
        {
            continue
        }
        # vcpkg_installed\<triplet>\share\<port>\vcpkg_abi_info.txt
        foreach ($tripletDirectory in Get-ChildItem -LiteralPath $installedDirectory -Directory)
        {
            $shareDirectory = Join-Path $tripletDirectory.FullName "share"
            if (-not (Test-Path -LiteralPath $shareDirectory))
            {
                continue
            }
            foreach ($portDirectory in Get-ChildItem -LiteralPath $shareDirectory -Directory)
            {
                $abiInfoPath = Join-Path $portDirectory.FullName "vcpkg_abi_info.txt"
                if (Test-Path -LiteralPath $abiInfoPath)
                {
                    [void]$used.Add((Get-FileHash -LiteralPath $abiInfoPath -Algorithm SHA256).Hash)
                }
            }
        }
    }
}

$archives = Get-Archive
$added = @($archives | Where-Object { -not $restored.Contains($_.Name) })
$removed = @()
if ($used.Count -eq 0)
{
    Write-Host "No build directory under $BuildDirectory finished configuring with vcpkg's packages, so every archive is kept."
}
else
{
    $removed = @($archives | Where-Object { $restored.Contains($_.Name) -and -not $used.Contains($_.BaseName) })
    foreach ($archive in $removed)
    {
        Remove-Item -LiteralPath $archive.FullName
    }
}
$changed = $added.Count -gt 0 -or $removed.Count -gt 0

Write-Host "vcpkg added $(Format-ArchiveCount $added)."
Write-Host "Removed $(Format-ArchiveCount $removed) that no installed package uses."
$kept = Get-Archive
if ($changed)
{
    Write-Host "The cache now holds $(Format-ArchiveCount $kept), to be saved."
}
else
{
    Write-Host "The cache holds $(Format-ArchiveCount $kept) as restored, so it need not be saved."
}
if (-not [string]::IsNullOrEmpty($env:GITHUB_OUTPUT))
{
    Add-GitHubFileLine -Path $env:GITHUB_OUTPUT -Line "changed=$($changed.ToString().ToLowerInvariant())"
}
