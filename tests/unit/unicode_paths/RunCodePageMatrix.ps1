param(
    [string]$BuildDir = 'build-slicesoft/main',
    [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
    [Parameter(Mandatory = $true)][string]$ManifestTool,
    [string]$RealModel = ''
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$source = (Resolve-Path (Join-Path $repo "$BuildDir/$Config")).Path
$root = Join-Path $repo ('output/unipath/codepages-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$matrix = @(
    @{ CodePage = 936; Manifest = 'LegacyCodePage.manifest' },
    @{ CodePage = 1252; Manifest = 'WesternCodePage.manifest' },
    @{ CodePage = 65001; Manifest = 'Utf8CodePage.manifest' }
)
$baselineChecksums = $null
foreach ($case in $matrix) {
    $directory = Join-Path $root ([string]$case.CodePage)
    New-Item -ItemType Directory -Path $directory | Out-Null
    Get-ChildItem -LiteralPath $source -Filter '*.dll' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $directory
    }
    foreach ($name in @('unicode_path_contract_tests.exe', 'slicer_worker.exe')) {
        $copy = Join-Path $directory $name
        Copy-Item -LiteralPath (Join-Path $source $name) -Destination $copy
        # Only isolated test copies are modified, never build/runtime executables.
        & $ManifestTool -nologo -manifest (Join-Path $PSScriptRoot $case.Manifest) "-outputresource:$copy;#1"
        if ($LASTEXITCODE -ne 0) { throw "Manifest injection failed: $copy" }
    }
    $test = Join-Path $directory 'unicode_path_contract_tests.exe'
    $arguments = @($repo)
    if ($RealModel) { $arguments += $RealModel }
    $savedTemp = $env:TEMP
    $savedTmp = $env:TMP
    $testTemp = Join-Path ([System.IO.Path]::GetTempPath()) (
        'up-' + [Guid]::NewGuid().ToString('N').Substring(0, 8) + '-' + [char]0x4e2d + [char]0x6587)
    New-Item -ItemType Directory -Path $testTemp | Out-Null
    try {
        $env:TEMP = $testTemp
        $env:TMP = $testTemp
        $result = & $test @arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $env:TEMP = $savedTemp
        $env:TMP = $savedTmp
    }
    $result | Set-Content -LiteralPath (Join-Path $directory 'test.log') -Encoding utf8
    $result | Write-Output
    if ($exitCode -ne 0) { throw "Code page $($case.CodePage) test failed: $directory" }
    if (-not ($result -match "^ACTIVE_CODE_PAGE=$($case.CodePage)$")) {
        throw 'The requested code page was not activated; Windows 11 is required for the legacy matrix.'
    }
    $checksum = @($result -match '^TIFF_CHECKSUMS=')
    if ($checksum.Count -ne 1) { throw 'Layer checksum evidence is missing.' }
    if ($null -eq $baselineChecksums) { $baselineChecksums = $checksum[0] }
    elseif ($baselineChecksums -cne $checksum[0]) { throw 'TIFF layer checksums differ between code pages.' }
}
Write-Output "UNICODE_CODE_PAGE_MATRIX_PASS root=$root"
