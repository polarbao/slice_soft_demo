# Slice Soft Demo Project Profile

- Project: `slice_soft_demo`
- Repository: `polarbao/slice_soft_demo`
- Domain: Industrial UV / inkjet 3D printing slicing Host Software prototype
- Current phase: P0 Demo Feature Freeze -> R0/R1 architecture refactor track
- Main language: C++20
- UI: Qt 5.15 Widgets, UI layer only
- Build: CMake target-based, Windows x64 / MSVC
- Tools: `slicer_cli`, `rip_reader_test`, `slicer_debug_ui`
- Scripts: `scripts/run_regression.ps1`, `scripts/run_3mf_negative_tests.ps1`, `scripts/compare_material_profiles.ps1`

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
Relief heightfield
RGBWSV TIFF writer
RIP reader strict validation
Preview report / preview PNG
Qt Debug UI
UI self-test / overlay-load-real smoke test
quick/full/heavy regression
```

## Critical constraints

```text
Do not change p0.rgbwsv.2 without explicit decision.
Do not add large features during R1.
Do not implement surface_shell_texture or compensated_varnish during R1.
Do not introduce Qt into slicer_core.
Keep slicer_cli / rip_reader_test / slicer_debug_ui buildable.
Run quick regression after each meaningful refactor step.
```

## Current code concentration risk

```text
model.cpp: scene, OBJ/MTL, 3MF, texture/material resource parsing mixed together.
slicer.cpp: pipeline, raster, relief, support, materials, output, reports, preview mixed together.
config.cpp: schema/default/migration/validation concerns mixed together.
```
