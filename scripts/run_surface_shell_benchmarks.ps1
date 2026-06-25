param(
    [string]$BuildDir = "build-openvdb-09b-r2-release",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Find-Executable([string]$Name) {
    $candidates = @(
        (Join-Path $BuildDir "$Config/$Name.exe"),
        (Join-Path $BuildDir "$Name.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "$Name.exe was not found under $BuildDir"
}

function Write-SubdividedBoxFixture([string]$Name, [int]$Segments) {
    $dir = "output/SurfaceShellBenchmarkFixtures/$Name"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $objPath = Join-Path $dir "$Name.obj"
    $configPath = Join-Path $dir "$Name.json"
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("mtllib $Name.mtl")
    $lines.Add("usemtl bench")
    $lines.Add("vt 0 0")
    $lines.Add("vt 1 0")
    $lines.Add("vt 1 1")
    $lines.Add("vt 0 1")

    $script:lines = $lines
    $script:vertexIndex = 1
    function Add-Quad([double]$x0, [double]$y0, [double]$z0, [double]$x1, [double]$y1, [double]$z1, [double]$x2, [double]$y2, [double]$z2, [double]$x3, [double]$y3, [double]$z3) {
        $script:lines.Add("v $x0 $y0 $z0")
        $script:lines.Add("v $x1 $y1 $z1")
        $script:lines.Add("v $x2 $y2 $z2")
        $script:lines.Add("v $x3 $y3 $z3")
        $a = $script:vertexIndex
        $b = $script:vertexIndex + 1
        $c = $script:vertexIndex + 2
        $d = $script:vertexIndex + 3
        $script:lines.Add("f $a/1 $c/3 $b/2")
        $script:lines.Add("f $a/1 $d/4 $c/3")
        $script:vertexIndex += 4
    }

    $w = 3.0
    $d = 3.0
    $h = 0.5
    for ($i = 0; $i -lt $Segments; ++$i) {
        for ($j = 0; $j -lt $Segments; ++$j) {
            $x0 = $w * $i / $Segments
            $x1 = $w * ($i + 1) / $Segments
            $y0 = $d * $j / $Segments
            $y1 = $d * ($j + 1) / $Segments
            Add-Quad $x0 $y0 0.0 $x1 $y0 0.0 $x1 $y1 0.0 $x0 $y1 0.0
            Add-Quad $x0 $y0 $h $x0 $y1 $h $x1 $y1 $h $x1 $y0 $h
        }
    }
    for ($i = 0; $i -lt $Segments; ++$i) {
        $x0 = $w * $i / $Segments
        $x1 = $w * ($i + 1) / $Segments
        Add-Quad $x0 0.0 0.0 $x0 0.0 $h $x1 0.0 $h $x1 0.0 0.0
        Add-Quad $x0 $d 0.0 $x1 $d 0.0 $x1 $d $h $x0 $d $h
        $y0 = $d * $i / $Segments
        $y1 = $d * ($i + 1) / $Segments
        Add-Quad 0.0 $y0 0.0 0.0 $y1 0.0 0.0 $y1 $h 0.0 $y0 $h
        Add-Quad $w $y0 0.0 $w $y0 $h $w $y1 $h $w $y1 0.0
    }
    Set-Content -LiteralPath $objPath -Value $lines -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $dir "$Name.mtl") -Value "newmtl bench`nKd 0.7 0.7 0.7`n" -Encoding ASCII

    $json = @{
        slicingMode = "relief_heightfield"
        input = @{ modelPath = "$Name.obj"; format = "obj" }
        output = @{ packageDir = "output/SurfaceShellBenchmarkCache/$Name"; dpiX = 600; dpiY = 600; layerThicknessMm = 0.01; channelOrder = @("R", "G", "B", "W", "S", "V"); bitDepth = 8; planarConfig = "contiguous"; storageMode = "stripped"; rowsPerStrip = 64 }
        autoOrient = @{ enabled = $false; maxHeightMm = 6.0 }
        texture = @{ enabled = $false; applyMode = "solid_volume_from_top_surface"; sampler = "nearest"; uvAddressMode = "clamp"; flipV = $true; fallbackRgb = @(12, 34, 56); missingTexturePolicy = "warn_and_fallback" }
        support = @{ enabled = $false; mode = "none"; value = 0; offsetMm = 0.0; minAreaPx = 0 }
    } | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $configPath -Value $json -Encoding ASCII
    return $configPath
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "OpenVDB 09B-R2 release build directory is not configured: $BuildDir"
}

Write-Host "== build surface shell robustness demo"
& cmake --build $BuildDir --config $Config --target surface_shell_robustness_demo
if ($LASTEXITCODE -ne 0) { throw "surface_shell_robustness_demo build failed" }

$demoExe = Find-Executable "surface_shell_robustness_demo"
$fixtures = @(
    @{ Name = "bench_1k"; Segments = 16 },
    @{ Name = "bench_10k"; Segments = 50 },
    @{ Name = "bench_50k"; Segments = 112 }
)

foreach ($fixture in $fixtures) {
    $configPath = Write-SubdividedBoxFixture $fixture.Name $fixture.Segments
    $outputDir = "output/SurfaceShellBenchmark/$($fixture.Name)"
    Write-Host "== benchmark $($fixture.Name)"
    & $demoExe --config $configPath --fixture-id $fixture.Name --output $outputDir --voxel-mm 0.10 --shell-mm 0.10 --mesh-policy strict_closed --build-config $Config
    if ($LASTEXITCODE -ne 0) { throw "benchmark failed: $($fixture.Name)" }
    $report = Get-Content -Raw -LiteralPath (Join-Path $outputDir "reports/surface_shell_benchmark_report.json") | ConvertFrom-Json
    Assert-True ($report.schema -eq "p0.surface_shell_benchmark_report.1") "benchmark schema mismatch"
    Assert-True ($report.mesh.acceptedTriangles -ge 1000) "benchmark expected 1k+ triangles"
    Assert-True ($report.bvh.queryCount -gt 0) "benchmark expected BVH queries"
}

Write-Host "Surface shell benchmarks complete."
