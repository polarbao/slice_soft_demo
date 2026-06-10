# Slice Architecture Boundary

## Target layers

```text
apps/
  slicer_cli
  rip_reader_test
  slicer_debug_ui

src/slicer_core/
  scene/
  importers/
  pipeline/
  texture/
  materials/
  support/
  raster/
  output/
  reports/
  diagnostics/
```

## Allowed dependencies

```text
importers -> scene
pipeline -> scene + texture + materials + support + raster + output + reports
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
```

## Public API rules

- Public APIs must use STL types or project domain DTOs, not Qt types.
- Public API changes require impact analysis and regression plan.
- During R1, prefer wrapper APIs before moving code.
- Legacy files may remain temporarily; document remaining responsibilities in stage reports.

## Strategy boundaries

```text
TextureApplicationPolicy is a formal strategy boundary.
VarnishGeometryPolicy is a formal strategy boundary.
R1 defines objects and insertion points only.
R2 or later implements new behavior.
```
