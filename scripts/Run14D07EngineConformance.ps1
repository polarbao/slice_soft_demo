param(
    [Parameter(Mandatory = $true)]
    [string]$WorkerPath,

    [string]$EvidenceRoot = "output/benchmarks/14d_07/manual"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedWorker = (Resolve-Path -LiteralPath $WorkerPath).Path
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
    --evidence-root $evidencePath
exit $LASTEXITCODE
