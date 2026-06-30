# Slice Architecture Boundary

## Target layers

```text
apps/
  slicer_cli
  rip_reader_test
  slicer_debug_ui

src/slicer_core/
  config/
  scene/
  importers/
  geometry/
  pipeline/
  texture/
  materials/
  support/
  raster/
  output/
  output/rgbwsv/
  reports/
  diagnostics/
  system/
```

## Allowed dependencies

```text
importers -> scene
geometry -> scene/mesh DTOs + diagnostics
pipeline -> scene + texture + materials + support + raster + output + reports
materials -> material DTOs + process profiles + channel composition inputs
output/rgbwsv -> output/tiff + output/manifest
reports -> diagnostics/config snapshot/stats
apps -> public tool/core APIs
slicer_debug_ui -> Qt + process/report/package services
```

## Forbidden dependencies

```text
core -> Qt UI
importers -> TIFF writer
materials -> filesystem texture loading
support -> report file writing
reports -> business decisions
UI -> slicer.cpp internal temporary structs
slicer_core -> QString/QList/QObject/QWidget
geometry -> material/output/report writing
experimental OpenVDB path -> implicit production RGBWSV TIFF writing
UI -> OpenVDB internal types or voxel-grid implementation details
```

## Public API rules

- Public APIs must use STL types or project domain DTOs, not Qt types.
- Public API changes require impact analysis and regression plan.
- During refactors, prefer wrapper APIs before moving code.
- Legacy files may remain temporarily; document remaining responsibilities in stage reports.

## Strategy boundaries

```text
TextureApplicationPolicy is a formal strategy boundary.
VarnishGeometryPolicy is a formal strategy boundary.
MaterialPolicy and MaterialProcessProfile are formal material/process boundaries.
ProductionAdmissionPolicy is a formal geometry safety boundary.
OpenVDB surface-shell texture is an experimental strategy path.
```

OpenVDB-related code must stay optional, explicitly gated, and separated from the legacy production `slicer_cli` output path unless a later approved task changes that boundary.
