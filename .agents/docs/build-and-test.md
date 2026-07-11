# Slice Build and Test

## Environment

```text
Windows x64
MSVC
CMake
Qt 5.15 Widgets for slicer_debug_ui
PowerShell scripts
OpenVDB is optional and must remain disabled by default
```

## Configure

```powershell
cmake -S . -B build
```

If vcpkg is used:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

OpenVDB experimental builds require an explicitly prepared dependency environment. Do not make this path mandatory for normal builds.

## Build

```powershell
cmake --build build --config Debug
```

UI only:

```powershell
cmake --build build --config Debug --target slicer_debug_ui
```

## Regression

```powershell
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_regression.ps1 -Mode full
.\scripts\run_regression.ps1 -Mode heavy
```

Quick CI gate:

```powershell
.\scripts\run_ci_quick.ps1
```

Schema and golden gates:

```powershell
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
```

## UI smoke

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

## RIP validation

```powershell
.\build\Debug\rip_reader_test.exe --package <package> --summary
```

## 09P OpenVDB Experimental Gates

Use these only for explicitly scoped 09P/OpenVDB experimental tasks:

```powershell
.\scripts\run_openvdb_smoke.ps1
.\scripts\run_09p_cli_experimental_tests.ps1
.\scripts\run_09p_experimental_pipeline_tests.ps1
```

The experimental OpenVDB path must not be treated as production-safe unless the task explicitly verifies and admits that behavior.

## 12B-R2 OpenVDB SDF Utility Report

Use the same report validator for both optional build lanes:

```powershell
.\scripts\run_12b_r2_openvdb_sdf_utility.ps1 -BuildDir build -Config Debug -Output output\benchmarks\12b_r2_openvdb_sdf_utility_off.json
.\scripts\run_12b_r2_openvdb_sdf_utility.ps1 -BuildDir build-openvdb-09p -Config Debug -Output output\benchmarks\12b_r2_openvdb_sdf_utility_on.json
```

The utility probe writes only `slicesoft.openvdb_sdf_utility.12b_r2.1` diagnostic JSON. It must not write a production package, RGBWSV TIFF, or preview output.

## 12C Qt UI Fresh Build

Qt 5.15.2 with MSVC 19.50+ uses the project-local compatibility shim in `apps/slicer_debug_ui/compat`.

PowerShell entry:

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui -Config Debug
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

VS Code entry:

```text
Task: SliceSoft: Build 12C Fresh Qt UI
Launch: SliceSoft: Debug 12C Fresh Qt UI
```

The script keeps `USE_OPENVDB=OFF`. It must not edit the local Qt installation.

## Baseline Gate

Each meaningful refactor step must pass:

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If preview/UI changed, also run `overlay-load-real`.

For documentation/config-only tasks, run targeted text/schema checks and `git diff --check`; do not claim build validation unless it was actually run.
