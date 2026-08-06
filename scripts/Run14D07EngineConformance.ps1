param(
    [Parameter(Mandatory = $true)]
    [string]$WorkerPath,

    [string]$GatePath = "",

    [string]$EvidenceRoot = "output/benchmarks/14d_07/manual"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedWorker = (Resolve-Path -LiteralPath $WorkerPath).Path
$resolvedGate = if ($GatePath)
{
    (Resolve-Path -LiteralPath $GatePath).Path
}
else
{
    Join-Path (Split-Path -Parent $resolvedWorker) "stage14d07_engine_conformance_gate.exe"
}
$evidencePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $EvidenceRoot))

python "$repoRoot/tests/stage14d_07/ValidateEngineConformanceDefinition.py" `
    --repo-root $repoRoot
if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

python "$repoRoot/tests/stage14d_07/RunEngineConformance.py" `
    --repo-root $repoRoot `
    --worker $resolvedWorker `
    --gate $resolvedGate `
    --evidence-root $evidencePath
exit $LASTEXITCODE
