<#
.SYNOPSIS
Fetches the coverage history that CI keeps on the gh-pages branch, or publishes a coverage run there.

.DESCRIPTION
CI's Coverage job keeps the history of its coverage runs on the repository's
gh-pages branch, which GitHub Pages serves, so that the trend outlives the
runner and can be seen without building (DC-121).

-Fetch, before scripts\build.ps1 -Coverage, clones the branch into the site
directory, or starts an empty clone there when the branch does not exist yet.
The build is then given the clone's history.jsonl with -CoverageHistoryPath, so
that the run's dashboard draws the published trend, compares the run with the
last run in it, and adds the run to it when the tests pass.

-Publish, after a run on main whose tests passed, replaces the clone's files
with the run's dashboard (build\coverage\index.html) and llvm-cov's annotated
sources (build\coverage\html), keeps its history.jsonl, which the run has added
to, writes coverage_badge.json, the run's line coverage for the README's badge
(DC-133), commits them, and pushes the branch. An empty .nojekyll file makes
GitHub Pages serve the files as they are. To push to GitHub, the script needs a
token that may write the repository's contents in the GITHUB_TOKEN environment
variable, which it gives git for the push alone.

.PARAMETER Fetch
Clones the branch into the site directory, which must not exist or be empty.

.PARAMETER Publish
Commits the run to the clone that -Fetch made and pushes the branch.

.PARAMETER SiteDirectory
Where the clone of the branch goes, outside the checkout, such as
$env:RUNNER_TEMP\coverage-site.

.PARAMETER Remote
The repository to fetch from and push to. By default it is the GitHub
repository that GITHUB_SERVER_URL and GITHUB_REPOSITORY name in a GitHub Actions
job; another URL or a local path tries the script out elsewhere.

.EXAMPLE
.\scripts\coverage_pages.ps1 -Fetch -SiteDirectory $env:RUNNER_TEMP\coverage-site
.\scripts\build.ps1 -Coverage -CoverageHistoryPath $env:RUNNER_TEMP\coverage-site\history.jsonl
.\scripts\coverage_pages.ps1 -Publish -SiteDirectory $env:RUNNER_TEMP\coverage-site
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, ParameterSetName = "Fetch")][switch]$Fetch,
    [Parameter(Mandatory, ParameterSetName = "Publish")][switch]$Publish,
    [Parameter(Mandatory)][string]$SiteDirectory,
    [string]$Remote = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$coverageDirectory = Join-Path $projectRoot "build\coverage"
$branch = "gh-pages"
$historyName = "history.jsonl"
$badgeName = "coverage_badge.json"
$utf8 = New-Object System.Text.UTF8Encoding $false
$SiteDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SiteDirectory)

# Runs git, fails on a non-zero exit code, and returns what git writes to
# standard output. Each line git writes to standard error would otherwise become
# an ErrorRecord (see Invoke-NativeCommand in build.ps1). The description, not
# the arguments, goes into the error, so that a token among them is not shown.
function Invoke-Git
{
    param(
        [Parameter(Mandatory)][string]$Description,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try
    {
        $output = @(& git @Arguments)
    }
    finally
    {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
    return $output
}

if ($Remote -eq "")
{
    if ([string]::IsNullOrEmpty($env:GITHUB_REPOSITORY))
    {
        throw "Give -Remote, or run the script in a GitHub Actions job, which sets GITHUB_REPOSITORY."
    }
    $server = if ([string]::IsNullOrEmpty($env:GITHUB_SERVER_URL)) { "https://github.com" } else { $env:GITHUB_SERVER_URL }
    $Remote = "$server/$env:GITHUB_REPOSITORY.git"
}

if ($Fetch)
{
    if ((Test-Path -LiteralPath $SiteDirectory) -and @(Get-ChildItem -LiteralPath $SiteDirectory -Force).Count -gt 0)
    {
        throw "$SiteDirectory is not empty; give a new directory for the clone."
    }

    # With --exit-code, git ls-remote exits with 2 when no ref matches.
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try
    {
        & git ls-remote --exit-code --heads $Remote $branch | Out-Null
    }
    finally
    {
        $ErrorActionPreference = $previousPreference
    }
    # The site's files are stored as the build writes them. Git for Windows sets
    # core.autocrlf, which would check them out with CRLF line endings and warn
    # about each file that it would convert, so the clone turns it off.
    if ($LASTEXITCODE -eq 0)
    {
        # Only the branch's last commit is needed to add one on top of it.
        Invoke-Git -Description "Cloning the $branch branch" -Arguments @("clone", "--quiet", "--config",
            "core.autocrlf=false", "--depth", "1", "--single-branch", "--branch", $branch, $Remote, $SiteDirectory) |
            Out-Null
    }
    elseif ($LASTEXITCODE -eq 2)
    {
        Write-Host "$Remote has no $branch branch yet, so the history starts with this run."
        Invoke-Git -Description "Creating the clone" `
            -Arguments @("init", "--quiet", "--initial-branch=$branch", $SiteDirectory) | Out-Null
        Invoke-Git -Description "Configuring the clone" `
            -Arguments @("-C", $SiteDirectory, "config", "core.autocrlf", "false") | Out-Null
        Invoke-Git -Description "Adding the remote" -Arguments @("-C", $SiteDirectory, "remote", "add", "origin", $Remote) |
            Out-Null
    }
    else
    {
        throw "Listing the branches of $Remote failed with exit code $LASTEXITCODE."
    }

    $historyPath = Join-Path $SiteDirectory $historyName
    $runCount = if (Test-Path -LiteralPath $historyPath -PathType Leaf)
    {
        @([System.IO.File]::ReadAllLines($historyPath) | Where-Object { $_.Trim() -ne "" }).Count
    }
    else
    {
        0
    }
    Write-Host "The $branch branch's history has $runCount runs: $historyPath"
    return
}

# -Publish. The clone's files are replaced, so the directory must be the clone
# that -Fetch made, and the run must have written its dashboard and added itself
# to the clone's history.
$currentBranch = ""
if (Test-Path -LiteralPath (Join-Path $SiteDirectory ".git"))
{
    $currentBranch = (Invoke-Git -Description "Reading the clone's branch" `
            -Arguments @("-C", $SiteDirectory, "symbolic-ref", "--short", "HEAD") | Out-String).Trim()
}
if ($currentBranch -ne $branch)
{
    throw "$SiteDirectory is not a clone of the $branch branch; run the script with -Fetch first."
}
$dashboardPath = Join-Path $coverageDirectory "index.html"
$annotatedSourceDirectory = Join-Path $coverageDirectory "html"
if (-not (Test-Path -LiteralPath $dashboardPath -PathType Leaf) -or
    -not (Test-Path -LiteralPath $annotatedSourceDirectory -PathType Container))
{
    throw "build\coverage has no dashboard or annotated sources; run scripts\build.ps1 -Coverage first."
}
if (-not (Test-Path -LiteralPath (Join-Path $SiteDirectory $historyName) -PathType Leaf))
{
    throw ("$SiteDirectory has no $historyName. Run scripts\build.ps1 -Coverage with -CoverageHistoryPath naming " +
        "the clone's history, and publish only a run whose tests passed.")
}

# Every file but the history is the run's own, so that the pages of a source
# file that no longer exists go too.
Get-ChildItem -LiteralPath $SiteDirectory -Force |
    Where-Object { $_.Name -ne ".git" -and $_.Name -ne $historyName } |
    Remove-Item -Recurse -Force
Copy-Item -LiteralPath $dashboardPath -Destination $SiteDirectory
Copy-Item -LiteralPath $annotatedSourceDirectory -Destination (Join-Path $SiteDirectory "html") -Recurse
[System.IO.File]::WriteAllText((Join-Path $SiteDirectory ".nojekyll"), "")

# The README's coverage badge reads this file as a shields.io endpoint badge
# (DC-133): the line coverage of src\ in the run just added to the history,
# rounded down as build.ps1 and the dashboard show it, so that only full coverage
# reads 100%, in the colours that shields.io gives its own coverage badges.
$lastRun = [System.IO.File]::ReadAllLines((Join-Path $SiteDirectory $historyName)) | Where-Object { $_.Trim() -ne "" } |
    Select-Object -Last 1
$lineCounts = ($lastRun | ConvertFrom-Json).lines
$percent = [Math]::Floor(1000 * $lineCounts.covered / $lineCounts.count) / 10
$color = if ($percent -ge 100) { "brightgreen" }
elseif ($percent -ge 90) { "green" }
elseif ($percent -ge 80) { "yellowgreen" }
elseif ($percent -gt 0) { "yellow" }
else { "red" }
$badge = [ordered]@{
    schemaVersion = 1
    label = "coverage"
    message = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:0.0}%", $percent)
    color = $color
}
[System.IO.File]::WriteAllText((Join-Path $SiteDirectory $badgeName), ($badge | ConvertTo-Json -Compress), $utf8)

Invoke-Git -Description "Staging the site" -Arguments @("-C", $SiteDirectory, "add", "--all") | Out-Null
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try
{
    & git -C $SiteDirectory diff --cached --quiet
}
finally
{
    $ErrorActionPreference = $previousPreference
}
if ($LASTEXITCODE -eq 0)
{
    Write-Host "The $branch branch already has this run; nothing to publish."
    return
}

# The message goes through a file: Windows PowerShell passes double quotes in a
# native command's arguments on unescaped, and a commit subject may have some.
$hash = (Invoke-Git -Description "Reading the commit" -Arguments @("-C", $projectRoot, "rev-parse", "--short", "HEAD") |
        Out-String).Trim()
$subject = (Invoke-Git -Description "Reading the commit's subject" `
        -Arguments @("-C", $projectRoot, "log", "-1", "--format=%s") | Out-String).Trim()
$messagePath = Join-Path ([System.IO.Path]::GetTempPath()) "coverage_pages_message.txt"
[System.IO.File]::WriteAllText($messagePath, "Coverage of ${hash}: $subject`n", $utf8)
try
{
    Invoke-Git -Description "Committing the site" -Arguments @("-C", $SiteDirectory,
        "-c", "user.name=github-actions[bot]", "-c", "user.email=41898282+github-actions[bot]@users.noreply.github.com",
        "commit", "--quiet", "--file", $messagePath) | Out-Null
}
finally
{
    Remove-Item -LiteralPath $messagePath -Force
}

# The checkout keeps no credentials, so that the build and the tests cannot
# push; the token is given to this push alone, as actions/checkout gives it, and
# masked in the log.
$pushArguments = @("-C", $SiteDirectory)
if (-not [string]::IsNullOrEmpty($env:GITHUB_TOKEN))
{
    $credentials = [Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes("x-access-token:$env:GITHUB_TOKEN"))
    if ($env:GITHUB_ACTIONS -eq "true")
    {
        Write-Host "::add-mask::$credentials"
    }
    $pushArguments += @("-c", "http.extraheader=AUTHORIZATION: basic $credentials")
}
$pushArguments += @("push", "--quiet", "origin", "HEAD:refs/heads/$branch")
Invoke-Git -Description "Pushing the $branch branch" -Arguments $pushArguments | Out-Null
Write-Host "Published the coverage of $hash to the $branch branch of $Remote."
