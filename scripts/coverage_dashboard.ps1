<#
.SYNOPSIS
Writes the dashboard of a coverage run: one HTML page of its tests, its coverage and their trend.

.DESCRIPTION
scripts\build.ps1 -Coverage runs this script after the tests, whether they pass
or fail. It reads GoogleTest's JSON report of each test shard, the summary that
llvm-cov export -summary-only writes when the tests pass, and a history file of
one JSON line per run. From coverage_dashboard.html, next to this script, it
writes one self-contained page: the page draws its charts as inline SVG with an
inline script and loads nothing else, so it opens from disk.

The page shows the tests by kind and by suite, with how many passed, failed,
were skipped or are disabled. A test's kind is the directory under tests\ of its
source file: unit, platform or integration. It shows the tests' total time and
the slowest tests, the line and branch coverage of each directory under src\ and
of src\ in total, and the least-covered files, each linking to llvm-cov's
annotated source. Last comes the trend of the test count, the time and the
coverage, one point per run in the history.

When the tests passed and the coverage summary is given, the script first
appends this run to the history: its date, commit, test counts, times, and the
covered and total lines and branches of src\. A run whose tests fail is shown
but not added, so every run in the history has its coverage.

.PARAMETER TestResultsDirectory
The directory of the shards' GoogleTest JSON reports, such as
build\coverage\test_results.

.PARAMETER HistoryPath
The history file, such as build\coverage\history.jsonl. It is created when it
does not exist. A line that is not a JSON object is left out of the trend, with a
warning.

.PARAMETER OutputPath
The page to write, such as build\coverage\index.html.

.PARAMETER CoverageSummaryPath
What llvm-cov export -summary-only wrote for the run. Without it, the page has
no coverage and the run is not added to the history.

.PARAMETER AnnotatedSourceDirectory
The directory llvm-cov show -format=html wrote, such as build\coverage\html.
Each directory and file under src\ links to its page there.

.PARAMETER ExpectedReportPaths
The report that each shard was told to write. The page names each one that is
missing: a shard writes its report when it ends, so one that crashed or timed
out writes none, and its tests are not counted.

.PARAMETER TestRunSeconds
How long the test run took from start to end, which the page shows next to the
sum of the tests' own times.

.PARAMETER TestsFailed
CTest reported a failure. The page says so even when no report records one.

.EXAMPLE
.\scripts\coverage_dashboard.ps1 -TestResultsDirectory build\coverage\test_results `
    -CoverageSummaryPath build\coverage\coverage_summary.json `
    -AnnotatedSourceDirectory build\coverage\html -HistoryPath build\coverage\history.jsonl `
    -OutputPath build\coverage\index.html
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TestResultsDirectory,
    [Parameter(Mandatory)][string]$HistoryPath,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$CoverageSummaryPath = "",
    [string]$AnnotatedSourceDirectory = "",
    [string[]]$ExpectedReportPaths = @(),
    [Nullable[double]]$TestRunSeconds = $null,
    [switch]$TestsFailed
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePrefix = (Join-Path $projectRoot "src") + "\"
$testsPrefix = (Join-Path $projectRoot "tests") + "\"
$utf8 = New-Object System.Text.UTF8Encoding $false
# The kinds of test in the order the page lists them; a kind not listed follows them.
$kindOrder = @("unit", "platform", "integration")
$slowestCount = 10

# Resolves a path against PowerShell's current location, which .NET's own
# current directory need not follow.
function Resolve-FullPath
{
    param(
        [Parameter(Mandatory)][string]$Path
    )

    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

$TestResultsDirectory = Resolve-FullPath $TestResultsDirectory
$HistoryPath = Resolve-FullPath $HistoryPath
$OutputPath = Resolve-FullPath $OutputPath
$outputDirectoryUri = New-Object System.Uri ((Split-Path -Parent $OutputPath).TrimEnd("\") + "\")

# Returns a property of a parsed JSON object, which strict mode would reject when
# the object has none. A missing or null property returns nothing, so that @()
# of it is empty rather than one $null, and an array comes back unrolled, so
# callers that expect a list wrap the result in @().
function Get-JsonProperty
{
    param(
        [Parameter(Mandatory)][object]$Object,
        [Parameter(Mandatory)][string]$Name
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value)
    {
        return
    }
    return $property.Value
}

# Returns a path under the project root as the project names it, such as
# tests/unit/ring_buffer_test.cpp. The compiler was given absolute paths, whose
# drive letter's case can differ from this script's.
function Get-ProjectPath
{
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$Path
    )

    $prefix = $projectRoot + "\"
    $path = $Path.Replace("/", "\")
    if ($path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase))
    {
        $path = $path.Substring($prefix.Length)
    }
    return $path.Replace("\", "/")
}

# Returns a test's kind: the directory under tests\ of its source file, or
# "other" for a file outside such a directory.
function Get-TestKind
{
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string]$File
    )

    $path = $File.Replace("/", "\")
    if (-not $path.StartsWith($testsPrefix, [StringComparison]::OrdinalIgnoreCase))
    {
        return "other"
    }
    $relativePath = $path.Substring($testsPrefix.Length)
    $separator = $relativePath.IndexOf("\")
    if ($separator -lt 0)
    {
        return "other"
    }
    return $relativePath.Substring(0, $separator).ToLowerInvariant()
}

# Returns the link from the page to a file of llvm-cov's HTML, or $null when
# there is no such file. llvm-cov show writes a source file to
# coverage\<its path without the drive>.html, and a directory's summary to
# index.html in the matching directory.
function Get-AnnotatedLink
{
    param(
        [Parameter(Mandatory)][string]$RecordedPath
    )

    if ([string]::IsNullOrEmpty($AnnotatedSourceDirectory))
    {
        return $null
    }
    $rootLength = [System.IO.Path]::GetPathRoot($RecordedPath).Length
    $target = Join-Path (Join-Path $AnnotatedSourceDirectory "coverage") $RecordedPath.Substring($rootLength)
    if (-not (Test-Path -LiteralPath $target -PathType Leaf))
    {
        return $null
    }
    return $outputDirectoryUri.MakeRelativeUri((New-Object System.Uri $target)).OriginalString
}

# Returns the commit the run built, or $null outside a git checkout.
function Get-Commit
{
    $git = Get-Command "git" -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $git)
    {
        return $null
    }

    # git writes UTF-8, which PowerShell would otherwise decode with the
    # console's code page.
    $previousPreference = $ErrorActionPreference
    $previousEncoding = [Console]::OutputEncoding
    $ErrorActionPreference = "Continue"
    [Console]::OutputEncoding = $utf8
    try
    {
        $hash = (& $git.Source -C $projectRoot rev-parse HEAD 2>$null | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $hash -eq "")
        {
            return $null
        }
        $subject = (& $git.Source -C $projectRoot log -1 "--format=%s" 2>$null | Out-String).Trim()
        $changes = @(& $git.Source -C $projectRoot status --porcelain --untracked-files=no 2>$null)
        return [ordered]@{
            hash = $hash
            subject = $subject
            changes = $changes.Count -gt 0
        }
    }
    finally
    {
        [Console]::OutputEncoding = $previousEncoding
        $ErrorActionPreference = $previousPreference
    }
}

function New-Tally
{
    return [ordered]@{
        tests = 0
        passed = 0
        failed = 0
        skipped = 0
        disabled = 0
        seconds = 0.0
    }
}

function Add-ToTally
{
    param(
        [Parameter(Mandatory)][System.Collections.IDictionary]$Tally,
        [Parameter(Mandatory)][object]$Test
    )

    $Tally.tests++
    $Tally[$Test.outcome]++
    $Tally.seconds += $Test.seconds
}

function Complete-Tally
{
    param(
        [Parameter(Mandatory)][System.Collections.IDictionary]$Tally
    )

    $Tally.seconds = [Math]::Round($Tally.seconds, 3)
    return $Tally
}

function Get-KindRank
{
    param(
        [Parameter(Mandatory)][string]$Kind
    )

    $rank = [Array]::IndexOf($kindOrder, $Kind)
    if ($rank -lt 0)
    {
        return $kindOrder.Count
    }
    return $rank
}

function New-CoverageCount
{
    param(
        [Parameter(Mandatory)][object[]]$Files,
        [Parameter(Mandatory)][string]$Kind
    )

    return [ordered]@{
        covered = [long]($Files | ForEach-Object { $_.$Kind.covered } | Measure-Object -Sum).Sum
        count = [long]($Files | ForEach-Object { $_.$Kind.count } | Measure-Object -Sum).Sum
    }
}

# The tests, from each shard's report. GoogleTest gives a test's time as seconds
# with a unit, such as "1.405s", and lists a disabled test as not run.
$tests = New-Object System.Collections.Generic.List[object]
$reportCount = 0
$missingReports = New-Object System.Collections.Generic.List[string]
$reportFiles = @()
if (Test-Path -LiteralPath $TestResultsDirectory -PathType Container)
{
    $reportFiles = @(Get-ChildItem -LiteralPath $TestResultsDirectory -Filter "*.json" -File | Sort-Object Name)
}
foreach ($reportFile in $reportFiles)
{
    try
    {
        $report = [System.IO.File]::ReadAllText($reportFile.FullName) | ConvertFrom-Json
    }
    catch
    {
        Write-Warning "$($reportFile.FullName) is not a GoogleTest JSON report; its tests are left out."
        $missingReports.Add("$($reportFile.Name) (unreadable)")
        continue
    }
    $reportCount++
    foreach ($suite in @(Get-JsonProperty $report "testsuites"))
    {
        foreach ($test in @(Get-JsonProperty $suite "testsuite"))
        {
            $file = [string](Get-JsonProperty $test "file")
            $status = Get-JsonProperty $test "status"
            $result = Get-JsonProperty $test "result"
            $failures = @(Get-JsonProperty $test "failures" | ForEach-Object { Get-JsonProperty $_ "failure" })
            $skips = @(Get-JsonProperty $test "skipped" | ForEach-Object { Get-JsonProperty $_ "message" })
            $outcome = if ($status -eq "NOTRUN" -or $result -eq "SUPPRESSED")
            {
                "disabled"
            }
            elseif ($failures.Count -gt 0)
            {
                "failed"
            }
            elseif ($result -eq "SKIPPED")
            {
                "skipped"
            }
            else
            {
                "passed"
            }
            $messages = @()
            if ($outcome -eq "failed")
            {
                $messages = $failures
            }
            elseif ($outcome -eq "skipped")
            {
                $messages = $skips
            }
            # A message starts with the absolute path of the line that wrote it.
            $messages = @(foreach ($message in $messages)
                {
                    [regex]::Replace([string]$message, [regex]::Escape($projectRoot + "\"), "",
                        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase).TrimEnd()
                })
            $time = [string](Get-JsonProperty $test "time")
            $seconds = if ($time -eq "") { 0.0 } else
            {
                [double]::Parse($time.TrimEnd("s"), [System.Globalization.CultureInfo]::InvariantCulture)
            }
            $tests.Add([pscustomobject]@{
                    kind = Get-TestKind $file
                    suite = [string]$suite.name
                    name = [string](Get-JsonProperty $test "name")
                    file = Get-ProjectPath $file
                    line = Get-JsonProperty $test "line"
                    seconds = $seconds
                    outcome = $outcome
                    messages = $messages
                })
        }
    }
}
foreach ($expectedReport in $ExpectedReportPaths)
{
    if (-not (Test-Path -LiteralPath $expectedReport -PathType Leaf))
    {
        $missingReports.Add((Split-Path -Leaf $expectedReport))
    }
}

$total = New-Tally
$kindTallies = @{}
$suiteTallies = @{}
foreach ($test in $tests)
{
    Add-ToTally $total $test
    if (-not $kindTallies.ContainsKey($test.kind))
    {
        $kindTallies[$test.kind] = New-Tally
    }
    Add-ToTally $kindTallies[$test.kind] $test
    $suiteKey = "$($test.kind)|$($test.suite)"
    if (-not $suiteTallies.ContainsKey($suiteKey))
    {
        $suiteTallies[$suiteKey] = [ordered]@{ kind = $test.kind; name = $test.suite } + (New-Tally)
    }
    Add-ToTally $suiteTallies[$suiteKey] $test
}
$kinds = @($kindTallies.Keys | Sort-Object { Get-KindRank $_ }, { $_ } | ForEach-Object {
        [ordered]@{ kind = $_ } + (Complete-Tally $kindTallies[$_])
    })
$suites = @($suiteTallies.Values | Sort-Object { Get-KindRank $_.kind }, { $_.name } | ForEach-Object {
        Complete-Tally $_
    })
$testRows = @{
    slowest = @($tests | Where-Object { $_.outcome -ne "disabled" } |
            Sort-Object @{ Expression = "seconds"; Descending = $true }, suite, name |
            Select-Object -First $slowestCount)
    failed = @($tests | Where-Object { $_.outcome -eq "failed" })
    skipped = @($tests | Where-Object { $_.outcome -eq "skipped" -or $_.outcome -eq "disabled" })
}

# The coverage of each file under src\, chosen regardless of case as build.ps1
# chooses them (DC-118).
$coverage = $null
if (-not [string]::IsNullOrEmpty($CoverageSummaryPath))
{
    $CoverageSummaryPath = Resolve-FullPath $CoverageSummaryPath
    if (-not [string]::IsNullOrEmpty($AnnotatedSourceDirectory))
    {
        $AnnotatedSourceDirectory = Resolve-FullPath $AnnotatedSourceDirectory
    }
    $export = [System.IO.File]::ReadAllText($CoverageSummaryPath) | ConvertFrom-Json
    $sourceFiles = @($export.data[0].files | Where-Object {
            $_.filename.Replace("/", "\").StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase)
        })
    if ($sourceFiles.Count -eq 0)
    {
        throw "$CoverageSummaryPath has no file under $sourcePrefix."
    }

    $files = @(foreach ($sourceFile in $sourceFiles)
        {
            $recordedPath = $sourceFile.filename.Replace("/", "\")
            $relativePath = $recordedPath.Substring($sourcePrefix.Length)
            [pscustomobject]@{
                path = $relativePath.Replace("\", "/")
                directory = [System.IO.Path]::GetDirectoryName($relativePath).Replace("\", "/")
                recordedDirectory = [System.IO.Path]::GetDirectoryName($recordedPath)
                link = Get-AnnotatedLink ($recordedPath + ".html")
                lines = [ordered]@{
                    covered = [long]$sourceFile.summary.lines.covered
                    count = [long]$sourceFile.summary.lines.count
                }
                branches = [ordered]@{
                    covered = [long]$sourceFile.summary.branches.covered
                    count = [long]$sourceFile.summary.branches.count
                }
            }
        })
    $directories = @($files | Group-Object directory | Sort-Object Name | ForEach-Object {
            [ordered]@{
                name = $_.Name
                link = Get-AnnotatedLink (Join-Path $_.Group[0].recordedDirectory "index.html")
                lines = New-CoverageCount -Files $_.Group -Kind "lines"
                branches = New-CoverageCount -Files $_.Group -Kind "branches"
            }
        })
    $index = if ([string]::IsNullOrEmpty($AnnotatedSourceDirectory)) { $null } else
    {
        $indexPath = Join-Path $AnnotatedSourceDirectory "index.html"
        if (Test-Path -LiteralPath $indexPath -PathType Leaf)
        {
            $outputDirectoryUri.MakeRelativeUri((New-Object System.Uri $indexPath)).OriginalString
        }
    }
    $coverage = [ordered]@{
        index = $index
        lines = New-CoverageCount -Files $files -Kind "lines"
        branches = New-CoverageCount -Files $files -Kind "branches"
        directories = $directories
        files = @($files | Select-Object path, link, lines, branches)
    }
}

$commit = Get-Commit
$runDate = (Get-Date).ToString("yyyy-MM-ddTHH:mm:sszzz", [System.Globalization.CultureInfo]::InvariantCulture)
$roundedRunSeconds = if ($null -eq $TestRunSeconds) { $null } else { [Math]::Round($TestRunSeconds, 2) }

# The history, and this run added to it when its tests passed.
$history = New-Object System.Collections.Generic.List[object]
if (Test-Path -LiteralPath $HistoryPath -PathType Leaf)
{
    $lineNumber = 0
    foreach ($historyLine in [System.IO.File]::ReadAllLines($HistoryPath))
    {
        $lineNumber++
        if ($historyLine.Trim() -eq "")
        {
            continue
        }
        $run = $null
        try
        {
            $run = $historyLine | ConvertFrom-Json
        }
        catch
        {
            $run = $null
        }
        if ($run -isnot [System.Management.Automation.PSCustomObject])
        {
            Write-Warning "${HistoryPath}:${lineNumber} is not a JSON object; it is left out of the trend."
            continue
        }
        $history.Add($run)
    }
}
$inHistory = $null -ne $coverage -and -not $TestsFailed
if ($inHistory)
{
    $kindCounts = [ordered]@{}
    foreach ($kind in $kinds)
    {
        $kindCounts[$kind.kind] = $kind.tests
    }
    $run = [ordered]@{
        date = $runDate
        commit = if ($null -eq $commit) { $null } else { $commit.hash }
        subject = if ($null -eq $commit) { $null } else { $commit.subject }
        changes = if ($null -eq $commit) { $null } else { $commit.changes }
        tests = $total.tests
        kinds = $kindCounts
        skipped = $total.skipped
        disabled = $total.disabled
        testSeconds = [Math]::Round($total.seconds, 3)
        runSeconds = $roundedRunSeconds
        lines = $coverage.lines
        branches = $coverage.branches
    }
    $historyLine = ConvertTo-Json -InputObject $run -Depth 4 -Compress
    $historyDirectory = Split-Path -Parent $HistoryPath
    if (-not (Test-Path -LiteralPath $historyDirectory))
    {
        New-Item -ItemType Directory -Path $historyDirectory | Out-Null
    }
    [System.IO.File]::AppendAllText($HistoryPath, $historyLine + "`n", $utf8)
    $history.Add(($historyLine | ConvertFrom-Json))
}

$data = [ordered]@{
    run = [ordered]@{
        date = $runDate
        commit = $commit
        testsFailed = [bool]$TestsFailed
        runSeconds = $roundedRunSeconds
        inHistory = $inHistory
    }
    shards = [ordered]@{
        reports = $reportCount
        missing = $missingReports.ToArray()
    }
    tests = [ordered]@{
        total = Complete-Tally $total
        kinds = $kinds
        suites = $suites
        slowest = $testRows.slowest
        failed = $testRows.failed
        skipped = $testRows.skipped
    }
    coverage = $coverage
    history = $history.ToArray()
}

# The data goes into a <script> element, which the first "</" would end. "<"
# appears in JSON only within strings, where its escape, a backslash and u003c,
# is the same character.
$json = (ConvertTo-Json -InputObject $data -Depth 8 -Compress).Replace("<", "\" + "u003c")
$templatePath = Join-Path $PSScriptRoot "coverage_dashboard.html"
$template = [System.IO.File]::ReadAllText($templatePath)
$placeholder = "/*DASHBOARD_DATA*/"
if (-not $template.Contains($placeholder))
{
    throw "$templatePath has no $placeholder for the data."
}
[System.IO.File]::WriteAllText($OutputPath, $template.Replace($placeholder, $json), $utf8)
