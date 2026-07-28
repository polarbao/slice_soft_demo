# Slice Soft Demo Project Profile

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Domain: Industrial UV / inkjet 3D printing slicing Host Software prototype
- Current phase: 12E-09C complete; Stage 13 13C-03 and 13B-08-01/02/03 are complete; 13B-08 scene-workflow closure is in progress and 13D workbench-layout is prepared
- Latest completed milestone: `12E-09B-06 Qt dual-mode production entry closure`
- Latest completed task: `13B-08-03 Qt current-scene slice action`; Debug full CTest 81/81, UI self-test, current/stale/cancel/no-fallback UI Smokes, TIFF auto-load, and Quick CI pass.
- Current task: implement `13B-08-04 real-model workflow matrix and stage closure`. 13B-08-01 provides serial multi-file import, 22-instance capacity admission, partial failure continuation, cancellation/generation protection, and one final layout. 13B-08-02 provides strict effective-config readback, explicit Profile/output identity, Legacy scene Raster production, one RGBWSV Package, stable CLI failures, and no Global fallback. 13B-08-03 provides the Qt action state machine, frozen scene identity, explicit CLI launch, strict Package validation, stale/cancel isolation, and TIFF production-preview auto-load. 13D is PREPARED and waits for 13C-05. 13B production acceptance remains blocked by unresolved device buildVolume/origin/axes and a 22-instance performance budget.
- Prepared stage: R4-06 two-family intake, R4-07-R1 four-case Release/closure/legacy/RIP, R4-07-R2 candidate budget `2026-07-23.r1`, and Quick-CI-R1 all PASS. R4-08-R2 is `GO` after explicit user authorization at 2026-07-23 15:15:27 +08:00. 12E-08D-01..06 implement mode routing, writer-ready layers, shared TIFF/package/RIP, restricted RGB + W, lower/internal-void S support, surface/outer V varnish, and the 0.01 mm six-case Release matrix. 12E-09B-01..06 provide the production mode/Profile catalog, atomic Effective Config, Chinese selector, capability lock, shared preflight/process route, exact session/package identity, no-fallback, same-source preview/report, and actual timing/peak-memory presentation. The 2026-07-24 09B closure matrix passes six xiao_ma/yecan Legacy/Global cases; Global is 4.09x-5.92x slower and 8.19x-8.74x higher in peak memory, so Legacy remains default. 12E-09C-01..06 freeze X=635/Y=600, validate independent DPI and physical pixels, implement Legacy/Global non-square Raster plus outer-varnish discretization, propagate Qt settings to one-click Effective Config, correct Layer/Overlay Preview physical aspect, and pass the four-case Release/RIP matrix including explicit 600/600 compatibility. 12E-09A-02 now freezes single_model/scene Diagnostic Effective Config; 09A-03..06 remain scheduled after 13C-03. 13A-02 implementation, tests, report, and handoff are complete.
- Prepared Stage 13: the original 13A/13B/13C design remains valid. The approved 13B-08 specialty adds four tasks for serial multi-file import, explicit scene production service/CLI, the Qt "slice current scene" action, and a real-model closure matrix. The proposed 13D specialty adds four tasks for a top job bar, one context inspector, a collapsible project area, a unified diagnostics dock, and responsive UI closure. Preparation documents are not implementation evidence.
- Frozen specialty: `12G-TCWS` texture carrier, white separation, and RIP underbase remains document-only and must not be implemented until product/RIP decisions and G1-G8 are explicitly closed.
- Validated R4 model inputs: 22 OBJ/3MF assets under `model` were audited. All 5 OBJ files under `model/obj/xiao_ma_wu_yu_new` plus `model/obj/yecan/3.obj` and `model/obj/yecan/4.obj` are strict PASS; xiao_ma Damuzhi and yecan/3 are the two tracked restricted-candidate baseline families. The aishen/meigui/titian families remain a 0/3 complex-relief coverage gap because all 9 candidates have confirmed self-intersection. The `model` directory has no strict-PASS 3MF; use `samples/models/3mf/texture2d_checker_cube.3mf` only as the positive Texture2D control lane.
- Main language: C++20
- UI: Qt 5.15 Widgets, UI layer only
- Build: CMake target-based, Windows x64 / MSVC
- Tools: `slicer_cli`, `rip_reader_test`, `slicer_debug_ui`
- Formal docs: `docs/slice`
- Codex tasks: `docs/codex_task`
- Historical docs: `docs/archive/2026-06-30_slicer_legacy`
- Scripts: `scripts/run_regression.ps1`, `scripts/run_ci_quick.ps1`, `scripts/run_schema_tests.ps1`, `scripts/run_golden_tests.ps1`, `scripts/run_09p_cli_experimental_tests.ps1`, `scripts/run_09p_experimental_pipeline_tests.ps1`, `scripts/run_openvdb_smoke.ps1`, `scripts/run_12e_08d_04_global_production_matrix.ps1`, `scripts/run_12e_08d_05_global_material_parity.ps1`, `scripts/run_12e_08d_06_release_matrix.ps1`, `scripts/run_12e_09b_06_production_ui_gate.ps1`

## Core capabilities

```text
OBJ / MTL / Texture input
STL ASCII / Binary input
3MF stored / deflate input
3MF BaseMaterial / ColorGroup / Texture2DGroup
3MF bad package validation
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support / SupportType / island diagnostics
Geometry diagnostics / ProductionAdmissionPolicy
Relief heightfield
RGBWSV TIFF writer
RIP reader strict validation
Preview report / preview PNG
Preview/material process reports
Qt Debug UI
UI self-test / overlay-load-real smoke test
OpenVDB optional experimental surface-shell texture path
Target-only selectable legacy/global_surface_shell pipeline contract; shared writer and explicit restricted Global production Profile admitted
quick/full/heavy regression
```

## Critical constraints

```text
Do not change p0.rgbwsv.2 without explicit decision.
Do not introduce Qt into slicer_core.
Do not enable OpenVDB by default.
Do not make OpenVDB mandatory for all builds.
Do not replace the legacy slicer_cli production path.
Do not write production RGBWSV TIFF from the experimental OpenVDB path unless explicitly approved.
Do not treat warn_and_attempt output as production-safe.
Confirmed self-intersection must fail fast.
Non-manifold, duplicate/opposite duplicate, and local winding issues must block strict production admission.
Keep slicer_cli / rip_reader_test / slicer_debug_ui buildable.
Run targeted validation after each meaningful code or doc-governance step.
```

## Current code concentration risk

```text
model.cpp: scene, OBJ/MTL, 3MF, texture/material resource parsing mixed together.
slicer.cpp: pipeline, raster, relief, support, materials, output, reports, preview mixed together.
config.cpp: schema/default/migration/validation concerns mixed together.
```
