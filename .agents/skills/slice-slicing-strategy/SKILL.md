---
name: slice-slicing-strategy
description: Use for slice_soft_demo slicing strategy decisions including TextureApplicationPolicy, full-volume vs surface-shell texture, VarnishGeometryPolicy, additive vs compensated varnish, MaterialPolicy, MaterialProcessProfile, support interaction, and RGBWSV channel semantics.
---

# Slice Slicing Strategy

Read first:

- `.agents/docs/SLICE_AI_SKILL_MASTER.md`
- `docs/slicer/PRE_R0_DECISION_纹理壳层与光油几何策略约束.md`
- Relevant MaterialPolicy / MaterialProcessProfile docs
- Current implementation code

Current strategy boundaries:

```text
TextureApplicationPolicy:
  FullVolume
  SurfaceShell
  TopSurfaceOnly
  OuterSurfaceShell

VarnishGeometryPolicy:
  InPlaceTopLayers
  AdditiveGrow
  CompensatedShrink
```

During R1:

- Define objects and insertion points only.
- Do not implement SurfaceShell behavior.
- Do not implement CompensatedShrink behavior.
- Preserve current full-volume and top-layer compatibility behavior.
- Preserve RGBWSV protocol.

When analyzing strategy requests, compare pros/cons and identify whether the issue is configuration, pipeline insertion, geometry kernel, or material composition.
