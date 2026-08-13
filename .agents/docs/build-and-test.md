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

The default Stage 14+ lane requires the manifest dependency `meshoptimizer` and uses
`x64-windows-static-md` under the repository-local installed root. Configure through the preset or runtime script:

```powershell
cmake --preset slicesoft-main
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
```

`meshoptimizer` is statically linked into `slicer_base`; no meshoptimizer DLL is deployed. The optional LibTIFF
lane remains `x64-windows` and resolves its own meshoptimizer package in the same repository-local installed root.

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

Golden fixtures must be deterministic, repository-tracked test inputs. Do not bind a fixed-size Golden expectation to
a real user Profile that may legitimately change model identity, scale, support, or preview policy. The
`material_process_top2` Golden case uses:

```text
samples/configs/golden/material_process_top2_fixture.json
samples/models/textured/fixtures/policy_textured_small.obj
```

The real UI Profile remains under `samples/configs/material_process` and must not be changed merely to satisfy Quick CI.

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

The daily Qt runtime lane follows the same structure as `ry_print_demo`: CMake presets select the
Visual Studio x64 multi-config generator, while MSBuild performs an 8-job incremental build:

```powershell
cmake --preset slicesoft-main
cmake --build --preset slicesoft-debug
cmake --build --preset slicesoft-release

.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release
```

VS Code daily entries:

```text
Configure Main Build
Fast Build (Debug/Release)
Build Runtime (Debug)  -> Run UI (Debug) / Debug UI (Debug)
Build Runtime (Release) -> Run UI (Release)
Quick Run UI (Debug/Release, No Build)
Deploy Runtime (Release, No Build)
Build All Runtimes      -> Debug followed by Release
```

Legacy `build` / CTest / sample matrix tasks are retained under the `Advanced` prefix and are not the default UI compilation path.

Outputs:

```text
build-slicesoft/main/<Config>
runtime/slicesoft/<Config>
```

The runtime directory contains `slicer_debug_ui.exe`, `slicer_ui_host_sim.exe`,
`slicer_module.dll`, `slicer_worker.exe`, `module.json`, `slicer_cli.exe`,
`rip_reader_test.exe`, required third-party runtime DLLs, Qt DLLs, platform plugins,
the MSVC runtime, `samples/`, `model/`, Profile-referenced documents, and
`runtime_manifest.json`. The deployment script validates all scenario
config/model/document paths and runs the packaged host module self-test before
publishing. OpenVDB remains OFF. Runtime UI resolves its application directory
as the packaged resource root and resolves sibling executables before any
build-directory fallback.

The Visual Studio/MSBuild lane relies on normal compiler dependency tracking and does not turn an
ordinary `.cpp` edit into a clean rebuild. The script still computes the input fingerprint before and
after compilation and refuses deployment if sources changed during the build. Use `-ForceClean` for
an explicit clean build:

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release -ForceClean
```

NMake is retained only as a manual fallback. Its separate build root and stricter clean fingerprint
guard prevent stale `.obj.d` reuse:

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 `
  -BuildDir build-slicesoft-nmake `
  -Config Release `
  -BuildSystem NMake
```

To republish already-built artifacts without rebuilding:

```powershell
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release -DeployOnly
```

## 03D TIFF Writer

The following 03D commands freeze the historical handwritten baseline before the later T-A default switch:

```powershell
.\scripts\Run03DTiffWriterBaseline.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -Iterations 5
```

The benchmark measures only `write_rgbwsv_tiff` with a pre-generated buffer. Its historical evidence keeps the handwritten
backend, exact RGBWSV decode, stripped/tiled tags, 255 tile padding, current errors, and RIP strict PASS.
Do not compare these numbers with full-package elapsed time.

`03D-04` completed the then-optional LibTIFF stripped/tiled lane while handwritten was still default. Use the same
targeted contract suite in both build directories:

```powershell
cmake --build build-slicesoft/main --config Release --target `
  tiff_writer_contract_unit_tests `
  tiff_backend_build_info_unit_tests `
  tiff_writer_backend_unit_tests
ctest --test-dir build-slicesoft/main -C Release `
  -R "^(tiff_writer_contract_unit_tests|tiff_backend_build_info_unit_tests|tiff_writer_backend_unit_tests)$" `
  --output-on-failure

cmake --build build-slicesoft/03d-libtiff --config Release --target `
  tiff_writer_contract_unit_tests `
  tiff_backend_build_info_unit_tests `
  tiff_writer_backend_unit_tests `
  slicer_cli `
  rip_reader_test
ctest --test-dir build-slicesoft/03d-libtiff -C Release `
  -R "^(tiff_writer_contract_unit_tests|tiff_backend_build_info_unit_tests|tiff_writer_backend_unit_tests)$" `
  --output-on-failure
```

The LibTIFF lane must report `stripped=true` and `tiled=true`. Standard tile dimensions divisible by 16 use
LibTIFF; historical nonstandard tile fixtures retain the handwritten compatibility route. The strict Reader accepts legal TIFF `SHORT` or `LONG` encodings for unsigned dimension/count tags
and still requires exact numeric values and RGBWSV pixels.

`03D-05` closes the functional compatibility gate for both configured build lanes:

```powershell
.\scripts\Run03DTiffCompatibilityGate.ps1 -Config Release
```

The gate builds and runs the Writer equivalence tests, shared Legacy/Global/scene package tests, actual
stripped/tiled packages, RIP strict checks, and the fixed bad-package error-code matrix. Its output under
`output/benchmarks/03d_05` is local evidence and remains ignored. A PASS authorizes only `03D-06`
performance measurement; it does not authorize changing the default Writer.

`03D-06` runs the Release Writer-only matrix with one process per backend/storage/case/cache condition:

```powershell
.\scripts\run_03d_libtiff_writer_matrix.ps1 `
  -HandwrittenBuildDir build-slicesoft/main `
  -LibTiffBuildDir build-slicesoft/03d-libtiff `
  -Config Release
```

The script re-runs the 03D-05 compatibility gate, then records actual configured/effective backend,
LibTIFF version, wall/CPU p50/p95, exact decode, output bytes and independent-process memory. `warm`
uses one untimed warmup; `cold_output_directory` uses a fresh directory without warmup but does not flush
the OS disk cache. The 2026-08-03 reference run concluded `GO_OPTIONAL`; that default decision is historical
and was superseded by T-A-03.

`03D-07` closed the optional lane without changing the then-current default:

```powershell
.\scripts\Run03DTiffOptionalClosure.ps1 -Config Release
```

This command verifies the default/optional Preset policy, re-runs compatibility and Writer performance,
deploys an isolated LibTIFF Runtime with `tiff.dll` and its license, runs Package/RIP strict smoke, and
runs the default handwritten full regression. The generated `output/benchmarks/03d_07` evidence is local
and ignored. There is no LibTIFF Reader backend; read compatibility uses the same project strict Reader.

## TIFF T-A Current Default

As of 2026-08-11, T-A-03 makes LibTIFF 4.7.1 the default Writer on the dynamic `x64-windows`
triplet. Use the main preset for production validation and the explicit legacy preset only for the
handwritten known-failure lane:

```powershell
cmake --preset slicesoft-main
cmake --build build-slicesoft/main --config Release --target `
  tiff_writer_contract_unit_tests `
  tiff_backend_build_info_unit_tests `
  tiff_writer_backend_unit_tests `
  tiff_writer_equivalence_unit_tests `
  slicer_cli rip_reader_test slicer_worker slicer_module
ctest --test-dir build-slicesoft/main -C Release `
  -R "^(tiff_writer_contract_unit_tests|tiff_writer_alignment_conformance_unit_tests|tiff_backend_build_info_unit_tests|tiff_writer_backend_unit_tests|tiff_writer_equivalence_unit_tests)$" `
  --output-on-failure

cmake --preset slicesoft-handwritten-legacy
cmake --build --preset slicesoft-handwritten-legacy-release
ctest --test-dir build-slicesoft/03d-handwritten -C Release `
  -R "tiff_writer_(contract|handwritten_alignment)|tiff_backend_build_info" `
  --output-on-failure
```

The main Runtime and Stage 14 package must contain `tiff.dll`, `licenses/libtiff.txt`, and matching
hash/dependency inventory. Default compression remains `none`; do not enable PackBits by default until
T-A-04 receives external target RIP/control-software evidence.

## 03E TIFF Compression

03E-01 isolates Writer/Reader compression performance:

```powershell
.\scripts\Run03ETiffCompressionMatrix.ps1 -Config Release
```

03E-02 validates the optional production configuration, manifest declaration, strict Reader, Legacy/Global/Scene
propagation, bad packages, and deterministic plus real OBJ packages across both Writer lanes:

```powershell
.\scripts\Run03EProductionCompressionGate.ps1 -Config Release
```

The 03E-02 gate records local evidence under `output/benchmarks/03e_02`. It does not run the external target RIP,
Photoshop, or printer control software. `none` remains the default until external interoperability and a new explicit
default-change authorization are both complete. PackBits has no compression-level setting.

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

## 12E-08C Release Evidence

默认 OpenVDB OFF 的真实模型诊断与 legacy 回归入口：

```powershell
cmake --build build --config Release
.\scripts\run_12e_08c_release_evidence.ps1 -BuildDir build -Config Release
ctest --test-dir build -C Release --output-on-failure
```

该脚本只写 `output/benchmarks/12e_08c` 诊断 JSON，不写 12E production package。核心时间排除
TIFF/PNG/JSON 写盘。真实 OBJ 被 strict topology 阻断时，脚本仍应输出可审计报告并保持
`productionAdmitted=false`；这类结果是证据完成，不是性能预算或生产准入通过。

## 12E-08C-R4 Restricted Production Candidate

两独立真实模型族的候选验证入口：

```powershell
cmake --build build --config Release --target repaired_asset_intake repaired_asset_intake_unit_tests texture_fill_partition_positive_matrix texture_fill_partition_positive_matrix_unit_tests texture_fill_partition_release_benchmark texture_fill_partition_release_benchmark_unit_tests
.\scripts\run_12e_08c_r4_06_repaired_asset_intake.ps1 -BuildDir build -Config Release -SkipBuild
.\scripts\run_12e_08c_r4_07_development_gate.ps1 -BuildDir build -Config Release -SkipBuild -ReuseIntakeEvidence
.\scripts\run_12e_08c_r4_07_restricted_candidate_gate.ps1
```

该入口要求 xiao_ma/yecan 两个独立 strict/admitted 模型族、四用例和 legacy TIFF/RIP 全部通过。输出仍为
diagnostic candidate evidence，`productionOutputWritten=false`、`productionAdmission=not_evaluated`。

候选预算冻结入口：

```powershell
cmake --build build --config Release --target repaired_asset_intake texture_fill_partition_positive_matrix texture_fill_partition_release_benchmark
.\scripts\run_12e_08c_r4_07_r2_candidate_budget.ps1 -BuildDir build -Config Release -SkipBuild
```

该入口固定参考机器、MSVC/Release、OpenVDB OFF、模型 hash、`voxelMm=0.20` 和每 case 五次正式测量，
校验版本化时间/峰值内存预算。它不代表任意机器产品 SLA，不写 global production package，也不授予 08D。

## 12E-08C-R1 Pre-Repair Baseline

默认 OpenVDB OFF 的修复前真实模型证据入口：

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_preflight_unit_tests
ctest --test-dir build -C Debug -R mesh_repair_preflight --output-on-failure
.\scripts\run_12e_08c_r1_pre_repair_baseline.ps1 -BuildDir build -Config Debug
```

脚本对 `nai_you_new`、`aishen_fudiao`、`meigui_fudiao` 和闭合 Texture2D 3MF 各执行两次，比较排除
计时后的稳定 report projection。输出仅位于 `output/benchmarks/12e_08c_r1_pre_repair`，不执行 repair、
不写 production package/TIFF。

## 12E-08C-R2 Conservative Cleanup Evidence

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(cleanup|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_cleanup_evidence.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r2_cleanup`。脚本强制默认 OpenVDB OFF，只执行显式 degenerate/exact
duplicate cleanup 诊断，不写生产包。

## 12E-08C-R2-02 Guarded Topology Evidence

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_02|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_02_topology_evidence.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r2_02_topology`。脚本对四个 required case 各运行两次，冻结
vertex mapping、operation hash 和组件数；只生成诊断 JSON，不写生产 package/TIFF。

## 12E-08C-R2-03 Boundary Loop Evidence

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_03|r2_02|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_03_boundary_evidence.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r2_03_boundary`。脚本冻结边界预算、generated mapping、属性状态和
双运行稳定投影；不写生产 package/TIFF。

## 12E-08C-R2-04 Post-Strict Evidence Guard

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_evidence_validator_unit_tests mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(r2_04|r2_03|r2_02|evidence_validator|cleanup|contract|preflight)" --output-on-failure
.\scripts\run_12e_08c_r2_04_post_strict_evidence.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r2_04_post_strict`。独立 validator 按固定顺序复核 operation、source/
vertex/generated mapping、material/UV、完整 post-strict 和 canonical hash；失败候选被丢弃。脚本对四个
required case 各运行两次，只写诊断 JSON，始终保持 `productionOutputWritten=false`。

## 12E-08C-R3-01 Non-Manifold Pattern Evidence

```powershell
cmake --build build --config Debug --target mesh_non_manifold_pattern_classifier_unit_tests mesh_repair_contract_unit_tests mesh_repair_preflight_unit_tests mesh_repair_preflight
ctest --test-dir build -C Debug -R "mesh_(non_manifold_pattern|repair_(contract|preflight|r3_01))" --output-on-failure
.\scripts\run_12e_08c_r3_01_non_manifold_patterns.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r3_01_non_manifold`。脚本对四个 required case 各执行两次，冻结每条
non-manifold edge 的 pattern、incident/source triangle、residual component 和 fan-split feasibility；只做
诊断分类，不修改网格、不写 production package/TIFF。

## 12E-08C-R3-01A Complete Self-Intersection Evidence

```powershell
cmake --build build --config Debug --target mesh_complete_self_intersection_analyzer_unit_tests mesh_repair_contract_unit_tests mesh_repair_preflight_unit_tests mesh_repair_preflight
ctest --test-dir build -C Debug -R "mesh_(complete_self_intersection|repair_(contract|preflight|r3_01a))" --output-on-failure
.\scripts\run_12e_08c_r3_01a_complete_self_intersection.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r3_01a_self_intersection`。脚本对四个 required case 各执行两次，冻结
完整候选计数、narrow-phase 分类、pair SHA-256 和稳定投影。它不执行 repair，不写 production package/TIFF；
budget/resource blocked 不能作为完整 strict PASS。

## 12E-08C-R3-02 Real Model Repair Matrix

```powershell
cmake --build build --config Debug --target mesh_repair_preflight mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests
ctest --test-dir build -C Debug -R "mesh_repair_(cleanup|preflight|r3_02)" --output-on-failure
.\scripts\run_12e_08c_r3_02_repair_matrix.ps1 -BuildDir build -Config Debug
```

输出位于 `output/benchmarks/12e_08c_r3_02_repair_matrix`。脚本对四个 required case 分别执行
`strict_no_repair` 与 `conservative_repair`，每条 lane 两次；完整自相交证据、non-manifold 分类、operation、
attribute/evidence validator 和 stable projection 必须一致。confirmed/coplanar case 在 mutation 前 fail-fast；
本入口只写诊断 JSON，不写 production package/TIFF。

## 12E-09D Production Texture And Single Material

```powershell
.\scripts\run_12e_09d_production_texture_material_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release
```

该入口构建并验证生产纹理合同、设置模型、单材料 Resolver、Qt 控件、Effective Config、Global 显式
`all_texture` 和共享 RGBWSV Writer。矩阵覆盖 Legacy 1/3/10、Global min/mid/all_texture、单材料 W/V，
并对全部 package 执行 RIP strict。证据写入 `output/benchmarks/12e_09d`，默认不提交。

## 12E-10A Same-Layer Preview Final Consistency

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  texture_fill_partition_semantic_preview_unit_tests `
  tiff_layer_source_unit_tests `
  material_preview_composer_unit_tests `
  slicer_debug_ui
ctest --test-dir build-slicesoft/main -C Debug `
  -R "^(texture_fill_partition_semantic_preview_unit_tests|material_preview_composer_unit_tests|tiff_layer_source_unit_tests)$" `
  --output-on-failure
.\build-slicesoft\main\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe `
  --ui-smoke-test --case diagnostic-semantic-preview --repo-root .
.\build-slicesoft\main\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe `
  --ui-smoke-test --case material-closure-diagnostics --repo-root .
```

Release 使用相同 target、CTest 过滤器和 smoke case，并把配置及可执行目录切换为 `Release`。该 Gate
验证生产 TIFF、09A Texture/Fill、W/S/V 和精确 `p0.material_closure.1` 按真实 layerIndex/zMm 绑定；
缺报告、candidate、跨层和 stale 必须 fail-closed。

## 12E-10B Real OBJ/3MF Dual-Mode Matrix

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10b_final_closure_matrix.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/12e_10
```

该 Gate 核对冻结模型 hash，执行 xiao_ma/yecan 的 Legacy/Global minimum/intermediate/all_texture、
Texture2D checker 3MF 的双模式格式控制，以及 aishen/meigui/titian strict blocked 披露。成功 case
必须生成 `p0.rgbwsv.2` TIFF package 并通过 RIP strict；blocked case 禁止写包和 fallback。固定汇总为
`output/benchmarks/12e_10/final_closure_matrix.json`，默认不提交。

## 12E-10C Release Performance And Memory

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10c_release_performance.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/12e_10c `
  -Iterations 3 `
  -WarmupIterations 1
```

该 Gate 固定 xiao_ma/yecan、minimum/intermediate/all_texture、600 x 600 DPI、0.20 mm 层厚、
stripped/uncompressed 和 Preview 关闭。每个 case 先预热再执行 3 次计量，并交替引擎顺序；所有生产包
必须通过 RIP strict。固定矩阵为
`output/benchmarks/12e_10c/release_performance_matrix.json`，默认不提交。

## Stage 15 Texture White Carrier

Stage 15 代码落地后使用默认 OpenVDB OFF 的 Release 轨道：

```powershell
cmake --build build-slicesoft/main --config Release --target `
  texture_white_carrier_policy_unit_tests `
  texture_white_preflight_service_unit_tests `
  experimental_config_unit_tests `
  production_effective_config_unit_tests `
  slicer_cli `
  rip_reader_test
ctest --test-dir build-slicesoft/main -C Release `
  -R "^(texture_white_carrier_policy_unit_tests|texture_white_preflight_service_unit_tests|experimental_config_unit_tests|production_effective_config_unit_tests)$" `
  --output-on-failure
.\scripts\run_stage15_white_carrier_gate.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/stage15
```

`15A-01` 必须在第一处代码编辑前生成 `baseline_identity.json`。Stage 15 不新增第三方依赖，UI 扫描复用 Qt 5.15；OpenVDB 保持 OFF。脚本与前两个测试 target 由 Stage 15 实现任务创建，在此之前不得声称命令已运行。
