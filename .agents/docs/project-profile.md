# Slice Soft Demo Project Profile

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Domain: Industrial UV / inkjet 3D printing slicing Host Software prototype
- Current phase: Stage 13 inserted 13G support projection/continuity specialty is functionally complete (`13G-00..07`, `13G-R1`); `03D-LIBTIFF` is the highest-priority prepared engineering specialty and `12E-09D` is the next prepared material-control specialty.
- Latest completed milestone: `13G-R1 prepended physical support-base layers with compatibility overlay`
- Latest completed task: thin-shell front-up auto-orient flips face-down Reality nails around their long axis, then aligns the tip with scene +Y. Production UI writes `support.baseProjection=true/30` with `layerPlacement=prepend_below_model`, adds 30 physical S-only TIFF layers below the model, and lifts the model by `30 * layerThicknessMm`. Historical explicit configs without `layerPlacement` retain `overlay_existing`; configs without `baseProjection` remain compatibility-disabled.
- Latest orientation amendment: `13E-R1-01` owns planar heading for both X-major and Y-major nail footprints and normalizes the narrow tip to scene `+Y`. `13G-00B` adds Z front/back classification before heading; explicit auto-orient disable preserves source pose.
- Current task: execute `03D-01` only after explicit implementation authorization, beginning with the current writer contract and writer-only Release baseline. `12E-09D-01..06` are fully prepared and follow 03D; `12E-10A` and `13F-R1-01..05` remain separately prepared. 13B production acceptance remains blocked by unresolved device buildVolume/origin/axes and a 22-instance performance budget.
- Prepared stage: R4-06 two-family intake, R4-07-R1 four-case Release/closure/legacy/RIP, R4-07-R2 candidate budget `2026-07-23.r1`, and Quick-CI-R1 all PASS. R4-08-R2 is `GO` after explicit user authorization at 2026-07-23 15:15:27 +08:00. 12E-08D-01..06 implement mode routing, writer-ready layers, shared TIFF/package/RIP, restricted RGB + W, lower/internal-void S support, surface/outer V varnish, and the 0.01 mm six-case Release matrix. 12E-09B-01..06 provide the production mode/Profile catalog, atomic Effective Config, Chinese selector, capability lock, shared preflight/process route, exact session/package identity, no-fallback, same-source preview/report, and actual timing/peak-memory presentation. The 2026-07-24 09B closure matrix passes six xiao_ma/yecan Legacy/Global cases; Global is 4.09x-5.92x slower and 8.19x-8.74x higher in peak memory, so Legacy remains default. 12E-09C-01..06 freeze X=635/Y=600, validate independent DPI and physical pixels, implement Legacy/Global non-square Raster plus outer-varnish discretization, propagate Qt settings to one-click Effective Config, correct Layer/Overlay Preview physical aspect, and pass the four-case Release/RIP matrix including explicit 600/600 compatibility. 12E-09A-01..06 now freeze read-only diagnostics, single_model/scene Effective Config, Chinese controls, asynchronous lifecycle, and same-layer TIFF semantic preview. 12E-10A..D are fully prepared but unimplemented.
- Completed Stage 13: the original 13A/13B/13C design and the approved 13B-08/13D/13E specialties are implemented and verified. 13E freezes standard nail front-up `+Z`, deterministic equivalent-candidate ordering, `maxHeightMm=9`, and the right-side preflight/diagnostic workflow. 12E-09A is also complete. Preparation documents for 12E-10 are not implementation evidence.
- Frozen specialty: `12G-TCWS` texture carrier, white separation, and RIP underbase remains document-only and must not be implemented until product/RIP decisions and G1-G8 are explicitly closed.
- 12G RIP evidence amendment: the existing RIP can reuse one full-RGB six-channel package for transparent or opaque-white output and currently interprets white-region `WSV=000` as a private downstream signal. Under `black_is_print`, that byte pattern physically requests W, S, and V printing, so it is not a self-contained `p0.rgbwsv.2` material meaning. Keep 12G frozen until the private RIP contract, S-channel collision, fail-closed behavior, and migration strategy are formally decided. Texture underbase remains out of scope.
- TIFF writer preparation: `vcpkg.json` already declares `tiff`, but production CMake does not find or link `TIFF::TIFF`; the current writer in `src/slicer_core/tiff_io.cpp` is handwritten, uncompressed, uint8, contiguous RGBWSV. `03D-LIBTIFF` prepares a dual-backend migration with decoded-pixel/tag equivalence and performance gates. Do not switch the default writer or deploy new runtime DLLs without the stage gate and explicit authorization.
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
