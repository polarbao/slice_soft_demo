[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RuntimeRoot,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [ValidateRange(5, 60)][int]$TimeoutSeconds = 30
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))

function Require([bool]$Value, [string]$Message)
{ if (-not $Value) { throw $Message } }

function ResolveRepoPath([string]$Path)
{
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function RequireUnder([string]$Path, [string]$Root)
{
    $prefix = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    Require ($Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) "Path must remain under ${Root}: $Path"
    $cursor = $Path
    while (-not [string]::IsNullOrEmpty($cursor))
    {
        if (Test-Path -LiteralPath $cursor)
        {
            Require (((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) `
                "Reparse paths are not accepted: $cursor"
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}

function GetPlainTree([string]$Root)
{
    $pending = [Collections.Generic.Queue[string]]::new()
    $pending.Enqueue($Root)
    while ($pending.Count -gt 0)
    {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Dequeue() -Force)
        {
            Require (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) "Package contains a reparse entry: $($item.FullName)"
            if ($item.PSIsContainer) { $pending.Enqueue($item.FullName) }
            $item
        }
    }
}

function PayloadPath([string]$Root, [string]$Relative)
{
    Require (-not [string]::IsNullOrWhiteSpace($Relative) -and -not [IO.Path]::IsPathRooted($Relative)) `
        "Manifest payload paths must be relative."
    $path = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    RequireUnder $path $Root
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Manifest payload is missing: $Relative"
    return $path
}

function RequireHash([string]$Root, [string]$Relative, [string]$Expected)
{
    Require ($Expected -match '^[a-fA-F0-9]{64}$') "Manifest SHA-256 is invalid: $Relative"
    $path = PayloadPath $Root $Relative
    Require ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $Expected) "Payload SHA-256 differs: $Relative"
}

function SaveJson([string]$Path, [object]$Value)
{ [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false)) }

function RunOwnedProcess([string]$Executable, [string]$Argument, [string]$Name)
{
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.Arguments = $Argument
    $info.WorkingDirectory = $copiedRuntime
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    $info.EnvironmentVariables["PATH"] = Join-Path $env:SystemRoot "System32"
    foreach ($nameToRemove in @("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH", "QTDIR", "Qt5_DIR"))
    { $info.EnvironmentVariables.Remove($nameToRemove) }
    $info.EnvironmentVariables["QT_QPA_PLATFORM"] = "windows"
    $info.EnvironmentVariables["QT_QPA_PLATFORM_PLUGIN_PATH"] = Join-Path $copiedRuntime "platforms"
    $info.EnvironmentVariables["SLICESOFT_DIAGNOSTICS_ENABLED"] = "1"
    $info.EnvironmentVariables["SLICESOFT_DUMP_ENABLED"] = "1"
    $info.EnvironmentVariables["SLICESOFT_LOG_LEVEL"] = "info"
    $info.EnvironmentVariables["SLICESOFT_DIAGNOSTICS_DIR"] = $logRoot
    $info.EnvironmentVariables["LOCALAPPDATA"] = Join-Path $testRoot "local-appdata"
    $info.EnvironmentVariables["APPDATA"] = Join-Path $testRoot "appdata"
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $started = $false
    try
    {
        Require ($process.Start()) "Could not start $Name"
        $started = $true
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $finished = $process.WaitForExit($TimeoutSeconds * 1000)
        if (-not $finished)
        {
            # This Process object belongs only to the child created immediately above.
            $process.Kill()
            Require ($process.WaitForExit(5000)) "Owned test child did not exit after timeout: $Name"
        }
        Require ($stdout.Wait(5000) -and $stderr.Wait(5000)) "Timed out collecting test output: $Name"
        $stdoutPath = Join-Path $testRoot "$Name.stdout.txt"
        $stderrPath = Join-Path $testRoot "$Name.stderr.txt"
        [IO.File]::WriteAllText($stdoutPath, $stdout.Result, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderrPath, $stderr.Result, [Text.UTF8Encoding]::new($false))
        $record = [ordered]@{
            name = $Name; pid = $process.Id; exitCode = $process.ExitCode; timedOut = -not $finished
            elapsedMs = $watch.ElapsedMilliseconds; stdout = "$Name.stdout.txt"; stderr = "$Name.stderr.txt"
        }
        $runs.Add($record)
        Require $finished "Owned test child exceeded timeout: $Name"
        Require ($process.ExitCode -eq 0) "Test child $Name exited $($process.ExitCode); see $stderrPath"
    }
    finally
    {
        if ($started -and -not $process.HasExited)
        {
            $process.Kill()
            [void]$process.WaitForExit(5000)
        }
        $process.Dispose()
    }
}

$runtime = ResolveRepoPath $RuntimeRoot
$testRoot = ResolveRepoPath $EvidenceRoot
RequireUnder $runtime (Join-Path $repoRoot "runtime")
RequireUnder $testRoot (Join-Path $repoRoot "build-slicesoft/main")
Require (Test-Path -LiteralPath $runtime -PathType Container) "Runtime package is missing: $runtime"
Require (-not (Test-Path -LiteralPath $testRoot)) "EvidenceRoot must be a new directory: $testRoot"
$tree = @(GetPlainTree $runtime)
Require (@($tree | Where-Object { -not $_.PSIsContainer -and $_.Extension -eq ".pdb" }).Count -eq 0) `
    "Runtime package contains private PDB files."
New-Item -ItemType Directory -Path $testRoot | Out-Null
$chineseName = -join ([char[]](0x4E2D, 0x6587, 0x90E8, 0x7F72))
$copiedRuntime = Join-Path $testRoot $chineseName
$logRoot = Join-Path $testRoot "logs"
$runs = [Collections.Generic.List[object]]::new()
$summary = [ordered]@{
    schema = "slicesoft.diagnostics.runtime.v1"; result = "FAIL"
    sourceRuntime = $runtime; copiedRuntime = $chineseName; createdAtUtc = [DateTime]::UtcNow.ToString("o")
    restrictedPath = (Join-Path $env:SystemRoot "System32"); qtPlatform = "windows"
    copiedFileCount = @($tree | Where-Object { -not $_.PSIsContainer }).Count
    processes = $runs; checks = @(); error = $null
    limits = @("Local restricted-PATH smoke, not clean-machine certification", "No model slicing, external RIP or physical printing")
}
try
{
    Copy-Item -LiteralPath $runtime -Destination $copiedRuntime -Recurse
    $manifest = Get-Content -LiteralPath (Join-Path $copiedRuntime "runtime_manifest.json") -Raw -Encoding UTF8 | ConvertFrom-Json
    Require ($manifest.schema -eq "slicesoft.runtime.1") "Unexpected runtime manifest schema."
    Require ($manifest.diagnostics.eventSchema -eq "diagnostics.event.v1" -and
        $manifest.diagnostics.crashSchema -eq "diagnostics.crash.v1") "Unexpected diagnostics schemas."
    RequireHash $copiedRuntime $manifest.diagnostics.crashReporter $manifest.diagnostics.crashReporterSha256
    Require (@($manifest.diagnostics.runtimeLibraries).Count -eq 2) "Expected spdlog and fmt inventory."
    foreach ($component in @("spdlog", "fmt"))
    {
        $entries = @($manifest.diagnostics.runtimeLibraries | Where-Object { $_.component -eq $component })
        Require ($entries.Count -eq 1) "Missing or duplicate diagnostic dependency: $component"
        $entry = $entries[0]
        RequireHash $copiedRuntime $entry.license $entry.licenseSha256
        Require ($entry.linkage -in @("shared", "static")) "Unknown linkage: $component"
        if ($entry.linkage -eq "shared") { RequireHash $copiedRuntime $entry.file $entry.librarySha256 }
        else { Require ([string]::IsNullOrEmpty($entry.file)) "Static dependency unexpectedly has a runtime file." }
    }
    Require (-not $manifest.diagnostics.symbols.includedInRuntime -and $manifest.diagnostics.symbols.binaryCount -eq 6 -and
        $manifest.diagnostics.symbols.archiveId -match '^[A-Za-z0-9_-]+$') "Symbol archive reference is not private/relative."
    [void](PayloadPath $copiedRuntime $manifest.diagnostics.userGuide)
    Require (@(GetPlainTree $copiedRuntime | Where-Object { -not $_.PSIsContainer -and $_.Extension -eq ".pdb" }).Count -eq 0) `
        "Copied runtime contains private PDB files."
    $summary.checks += "diagnostics hashes/licenses, archive reference, no published PDB"
    RunOwnedProcess (PayloadPath $copiedRuntime $manifest.capabilityPackage.worker) "--contract-info" "worker_contract"
    $contract = Get-Content -LiteralPath (Join-Path $testRoot "worker_contract.stdout.txt") -Raw -Encoding UTF8 | ConvertFrom-Json
    Require ($contract.contract -eq "file_contract" -and $contract.major -eq $manifest.capabilityPackage.fileContract.major -and
        $contract.minor -eq $manifest.capabilityPackage.fileContract.minor -and
        @($contract.produces) -contains "p0.rgbwsv.2") "Packaged worker contract query differed."
    $summary.checks += "worker contract query with restricted PATH"
    [void](PayloadPath $copiedRuntime "platforms/qwindows.dll")
    RunOwnedProcess (PayloadPath $copiedRuntime $manifest.tools.ui) "--diagnostics-self-test" "host_smoke"
    $appLogs = @(Get-ChildItem -LiteralPath $logRoot -Filter app.log -File -Recurse)
    Require ($appLogs.Count -eq 1) "Expected one isolated host app.log."
    $events = @(Get-Content -LiteralPath $appLogs[0].FullName -Encoding UTF8 | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { ConvertFrom-Json $_ })
    $hostPid = $runs[$runs.Count - 1].pid
    foreach ($event in $events)
    {
        Require ($event.schema -eq "diagnostics.event.v1" -and $event.sourcePid -eq $hostPid -and
            $event.sourceProcessRole -eq "host") "Host event schema or process identity differed."
    }
    foreach ($expected in @(@("crash_reporter", "ready"), @("module_logging", "attached"), @("shutdown", "end")))
    {
        Require (@($events | Where-Object { $_.action -eq $expected[0] -and $_.phase -eq $expected[1] }).Count -gt 0) `
            "Missing host event: $($expected -join '/')"
    }
    Require (@($events | Where-Object { $_.action -eq "qt" -and $_.phase -eq "message" -and
        $_.message -eq "LOGDUMP_QT_BRIDGE_SMOKE" }).Count -eq 1) "Qt logging bridge marker is missing or duplicated."
    Require (@(Get-ChildItem -LiteralPath $logRoot -File -Recurse | Where-Object { $_.Extension -in @(".dmp", ".partial") }).Count -eq 0) `
        "Normal smoke shutdown unexpectedly produced a crash artifact."
    $summary.checks += "Chinese-path host startup, reporter ready, module callback bound, Qt bridge, orderly shutdown"
    $summary.result = "PASS"
    SaveJson (Join-Path $testRoot "summary.json") $summary
    Write-Output "Diagnostics runtime PASS: $testRoot"
}
catch
{
    $summary.error = $_.Exception.Message
    SaveJson (Join-Path $testRoot "summary.json") $summary
    throw
}
