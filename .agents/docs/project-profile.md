# Slice Soft Demo Project Profile

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Domain: Industrial UV / inkjet 3D printing slicing Host Software prototype
- Current phase: Stage 12E, the approved Stage 13 scope, Stage 15, and Stage 14 slicer-side delivery are complete. Stage 14 is `SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED`; printing-side, target RIP, clean-machine and physical-print validation remain NOT RUN.
- Latest completed milestone: Stage 14F local closure (Release package, M1 intake, S1 gate, S2 C1-C7 gate, frozen-contract hash evidence).
- Latest completed task: thin-shell front-up auto-orient flips face-down Reality nails around their long axis, then aligns the tip with scene +Y. Production UI writes `support.baseProjection=true/30` with `layerPlacement=prepend_below_model`, adds 30 physical S-only TIFF layers below the model, and lifts the model by `30 * layerThicknessMm`. Historical explicit configs without `layerPlacement` retain `overlay_existing`; configs without `baseProjection` remain compatibility-disabled.
- Latest orientation amendment: `13E-R1-01` owns planar heading for both X-major and Y-major nail footprints and normalizes the narrow tip to scene `+Y`. `13G-00B` adds Z front/back classification before heading; explicit auto-orient disable preserves source pose.
- Current task: no remaining Stage 14 slicer-side implementation card. Preserve SPI v1/11 exports/15 capabilities, Worker file contract, `p0.rgbwsv.2`, S2 path D and ViewData contracts until a controlled revision is authorized. Handwritten TIFF remains default, LibTIFF optional, PackBits on-demand.
- Prepared stage: 12E-08D/09A/09B/09C/09D and 10A..10D are complete. 10B passes the 17-row real OBJ/3MF matrix; 10C passes 36/36 Release measurements and records the current-reference-machine performance/memory baseline. Legacy remains default and Global remains explicit candidate. R4-08-R2 remains the Global production admission basis; aishen/meigui/titian remain strict blocked.
- Completed Stage 13: the original 13A/13B/13C design and the approved 13B-08/13D/13E specialties are implemented and verified. 13E freezes standard nail front-up `+Z`, deterministic equivalent-candidate ordering, `maxHeightMm=9`, and the right-side preflight/diagnostic workflow. 12E-09A is also complete. Preparation documents for 12E-10 are not implementation evidence.
- Frozen specialty: `12G-TCWS` texture carrier, white separation, and RIP underbase remains document-only and must not be implemented until product/RIP decisions and G1-G8 are explicitly closed.
- 12G RIP evidence amendment: the existing RIP can reuse one full-RGB six-channel package for transparent or opaque-white output and currently interprets white-region `WSV=000` as a private downstream signal. Under `black_is_print`, that byte pattern physically requests W, S, and V printing, so it is not a self-contained `p0.rgbwsv.2` material meaning. Keep 12G frozen until the private RIP contract, S-channel collision, fail-closed behavior, and migration strategy are formally decided. Texture underbase remains out of scope.
- TIFF writer state: handwritten remains the default uint8 contiguous RGBWSV writer. An explicit `slicesoft-libtiff` lane links LibTIFF 4.7.1, deploys `tiff.dll` and its license, and routes stripped plus standard 16-aligned tiled output through LibTIFF. Legacy nonstandard tile dimensions retain the handwritten compatibility route. `output.tiffCompression.algorithm=none|packbits` is propagated through both Writer lanes and production pipelines; new manifests declare the effective algorithm and the strict Reader rejects tag/declaration mismatches. PackBits remains explicit experimental, external RIP compatibility is pending, and default compression remains `none`.
- Validated R4 model inputs: 22 OBJ/3MF assets under `model` were audited. All 5 OBJ files under `model/obj/xiao_ma_wu_yu_new` plus `model/obj/yecan/3.obj` and `model/obj/yecan/4.obj` are strict PASS; xiao_ma Damuzhi and yecan/3 are the two tracked restricted-candidate baseline families. The aishen/meigui/titian families remain a 0/3 complex-relief coverage gap because all 9 candidates have confirmed self-intersection. The `model` directory has no strict-PASS 3MF; use `samples/models/3mf/texture2d_checker_cube.3mf` only as the positive Texture2D control lane.
- Main language: C++20
- UI: Qt 5.15 Widgets, UI layer only
- Build: CMake target-based, Windows x64 / MSVC
- Tools: `slicer_cli`, `rip_reader_test`, `slicer_debug_ui`
- Formal docs: `docs/slice`
- Codex tasks: `docs/codex_task`
- Historical docs: `docs/archive/2026-06-30_slicer_legacy`
- Scripts: `scripts/run_regression.ps1`, `scripts/run_ci_quick.ps1`, `scripts/run_schema_tests.ps1`, `scripts/run_golden_tests.ps1`, `scripts/run_09p_cli_experimental_tests.ps1`, `scripts/run_09p_experimental_pipeline_tests.ps1`, `scripts/run_openvdb_smoke.ps1`, `scripts/run_12e_08d_04_global_production_matrix.ps1`, `scripts/run_12e_08d_05_global_material_parity.ps1`, `scripts/run_12e_08d_06_release_matrix.ps1`, `scripts/run_12e_09b_06_production_ui_gate.ps1`, `scripts/run_12e_10b_final_closure_matrix.ps1`

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
