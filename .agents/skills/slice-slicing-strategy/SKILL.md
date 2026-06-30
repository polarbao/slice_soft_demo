---
name: slice-slicing-strategy
description: Use for slice_soft_demo slicing strategy decisions including TextureApplicationPolicy, full-volume vs surface-shell texture, VarnishGeometryPolicy, additive vs compensated varnish, MaterialPolicy, MaterialProcessProfile, support interaction, and RGBWSV channel semantics.
---

# Slice Slicing Strategy

Read first:

- `.agents/docs/SLICE_AI_SKILL_MASTER.md`
- `docs/archive/2026-06-30_slicer_legacy/decisions/PRE_R0_DECISION_纹理壳层与光油几何策略约束.md` as historical evidence
- Relevant formal MaterialPolicy / MaterialProcessProfile docs in `docs/slice`
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

Historical R1 constraints:

- Define objects and insertion points only.
- Do not implement SurfaceShell behavior.
- Do not implement CompensatedShrink behavior.
- Preserve current full-volume and top-layer compatibility behavior.
- Preserve RGBWSV protocol.

Current 09P experimental constraints:

- OpenVDB / SurfaceShell work must stay in explicitly scoped experimental paths.
- OpenVDB must remain optional and disabled by default.
- Do not write production RGBWSV TIFF from the experimental path unless explicitly approved.
- Preserve `p0.rgbwsv.2`, RGBWSV channel order, uint8 depth, and `black_is_print`.

When analyzing strategy requests, compare pros/cons and identify whether the issue is configuration, pipeline insertion, geometry kernel, or material composition.
