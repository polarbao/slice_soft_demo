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

## OpenVDB vcpkg Root Selection

Normal non-OpenVDB projects may continue using the shared environment from `VCPKG_ROOT`.

For the OpenVDB feature lane, do not use a vcpkg root whose path contains spaces. A fresh manifest configure on 2026-07-12 with `D:\Program Files Tools\vcpkg` reached `hwloc:x64-windows` and failed because autotools split `D:\Program Files Tools\...` into separate arguments (`No rule to make target '/d/Program'`).

Current rule:

```text
default/non-OpenVDB lane: VCPKG_ROOT is allowed;
OpenVDB lane: use the dedicated no-space D:\vcpkg-openvdb root;
do not copy installed/packages/buildtrees between roots;
do not repoint an existing CMake build cache to another toolchain root;
future consolidation requires a no-space shared alias/root and a locked builtin-baseline, followed by a fresh build.
```

## 12C Qt UI Fresh Build

Qt 5.15.2 with MSVC 19.50+ uses the project-local compatibility shim in `apps/slicer_debug_ui/compat`.

PowerShell entry:

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui -Config Debug
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

Historical VS Code entry (removed from the daily launch list after 12F-R0 consolidation):

```text
Task: SliceSoft: Build 12C Fresh Qt UI
Launch: SliceSoft: Debug 12C Fresh Qt UI
```

The script keeps `USE_OPENVDB=OFF`. It must not edit the local Qt installation. The 12C entry is retained as a historical fresh regression lane; it is no longer the second daily VS Code Qt launch.

## Unified Debug/Release Runtime

The daily Qt runtime lane imports the Visual Studio x64 developer environment and uses NMake single-config build directories:

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release
```

Outputs:

```text
build-slicesoft/<Config>
runtime/slicesoft/<Config>
```

The runtime directory contains `slicer_debug_ui.exe`, `slicer_cli.exe`, `rip_reader_test.exe`, Qt DLLs, platform plugins, the MSVC runtime, `samples/`, `model/`, Profile-referenced documents, and `runtime_manifest.json`. The deployment script validates all scenario config/model/document paths before publishing. OpenVDB remains OFF. Runtime UI resolves its application directory as the packaged resource root and resolves the sibling CLI and RIP reader before any build-directory fallback.

To republish already-built artifacts without rebuilding:

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly
```

## Baseline Gate

Each meaningful refactor step must pass:

```powershell
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

If preview/UI changed, also run `overlay-load-real`.

For documentation/config-only tasks, run targeted text/schema checks and `git diff --check`; do not claim build validation unless it was actually run.

## 12D Material Closure

Repair-disabled TIFF invariance gate：

```powershell
.\scripts\run_material_closure_tests.ps1 -BuildDir build -Config Debug -Mode RepairDisabled
```

该脚本生成 baseline/diagnostic 两份 package，按 manifest layerIndex 比较全部生产 TIFF 的 SHA-256，并运行 RIP Reader。它不启用 repair，也不比较预期不同的 report/manifest 整体目录 hash。
