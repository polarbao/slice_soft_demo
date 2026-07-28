# Slice Soft Demo Project Profile

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Domain: Industrial UV / inkjet 3D printing slicing Host Software prototype
- Current phase: 12E-09C complete; Stage 13 P0 design and all 17 near-term atomic task preparations complete; 13A-01..05 and 13B-01..07 implemented
- Latest completed milestone: `12E-09B-06 Qt dual-mode production entry closure`
- Latest completed task: `13B-07 real-model functional matrix and closure`; Debug/Release 1/11/12/22 plus OBJ/3MF packages and RIP strict pass.
- Current task: `13C-02 MaterialPreviewComposer`; 13C-01 manifest-authoritative TIFF source, bounded LRU, stable errors, cancellation/stale guards, and Qt async worker are complete. 13B production acceptance remains blocked by unresolved device buildVolume/origin/axes and a 22-instance performance budget.
- Prepared stage: R4-06 two-family intake, R4-07-R1 four-case Release/closure/legacy/RIP, R4-07-R2 candidate budget `2026-07-23.r1`, and Quick-CI-R1 all PASS. R4-08-R2 is `GO` after explicit user authorization at 2026-07-23 15:15:27 +08:00. 12E-08D-01..06 implement mode routing, writer-ready layers, shared TIFF/package/RIP, restricted RGB + W, lower/internal-void S support, surface/outer V varnish, and the 0.01 mm six-case Release matrix. 12E-09B-01..06 provide the production mode/Profile catalog, atomic Effective Config, Chinese selector, capability lock, shared preflight/process route, exact session/package identity, no-fallback, same-source preview/report, and actual timing/peak-memory presentation. The 2026-07-24 09B closure matrix passes six xiao_ma/yecan Legacy/Global cases; Global is 4.09x-5.92x slower and 8.19x-8.74x higher in peak memory, so Legacy remains default. 12E-09C-01..06 freeze X=635/Y=600, validate independent DPI and physical pixels, implement Legacy/Global non-square Raster plus outer-varnish discretization, propagate Qt settings to one-click Effective Config, correct Layer/Overlay Preview physical aspect, and pass the four-case Release/RIP matrix including explicit 600/600 compatibility. 12E-09A-02 now freezes single_model/scene Diagnostic Effective Config; 09A-03..06 remain scheduled after 13C-03. 13A-02 implementation, tests, report, and handoff are complete.
- Prepared Stage 13: 13A short-term +Z top view with XY/rotateZ/uniformScale/mirror transforms, 13B modelId/instanceId scene with up to 11 columns x 2 rows and 20/30 mm configurable edge gaps plus one joint RGBWSV package, and 13C TIFF-native R/G/B/W/S/V and RGB+S+W+V preview. All 17 near-term tasks have dependency, proposed ownership, planned test, output, and acceptance preparation. The complete 13A/13B functional chain now exists and the 13B-07 real-model matrix passes. TIFF-native preview, device build-volume/axis values, 22-instance production budgets, and production evidence do not yet exist.
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
