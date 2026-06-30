# SURFACE_SHELL_TEXTURE_REAL_MODEL_CHECKLIST

> 文档版本：v0.1  
> 用途：09B-R1 fixture 验证记录  
> 建议提交目录：`docs/slicer/`

| Case | Format | Closed | UV | Texture | Expected | Result |
|---|---|---:|---:|---:|---|---|
| OBJ real textured | OBJ | Yes | Yes | Yes | Texture sample > 0 | PASS: sampled=18188, colors=191 |
| 3MF real textured | 3MF | Yes | Yes | Yes | Texture sample > 0 | PASS: sampled=18188, colors=8 |
| OBJ missing texture | OBJ | Yes | Yes | No | Diffuse/fallback | PASS: diffuse=18188, missingTexture=18188 |
| OBJ no UV | OBJ | Yes | No | No | Diffuse/fallback | PASS: fallback=18188, missingUV=18188 |
| Open mesh | OBJ | No | No | No | strict_closed fail | PASS: boundaryEdges=4, rejected |
| Non-manifold | OBJ | No | No | No | strict_closed fail | PASS: nonManifoldEdges=1, rejected |

每个 case 至少记录：

```text
config path
model path
triangle count
boundary edges
non-manifold edges
active voxels
inside/shell/interior voxels
sampled texture voxels
fallback voxels
outside colored voxels
max transfer distance
runtime
report path
preview path
```

## 验证记录

共同参数：

```text
voxelSizeMm = 0.05
shellThicknessMm = 0.10
maxTransferDistanceMm = 0.186602540378444
meshPolicy = strict_closed
OpenVDB = 12.0.1
```

| Case | Config / Model | Topology | Voxel / Transfer | Runtime | Artifacts |
|---|---|---|---|---|---|
| OBJ real textured | `samples/configs/openvdb/surface_shell_obj_real.json` / `samples/models/openvdb/surface_shell_cube.obj` | triangles=12, boundary=0, nonManifold=0 | active=103574, inside=40931, shell=18188, interior=22743, sampled=18188, outsideColored=0, maxDistance=0.05 mm | import=2.16 ms, levelSet=839.82 ms, transfer=255.92 ms | `output/SurfaceShellObjReal/reports/surface_shell_texture_report.json`, `output/SurfaceShellObjReal/preview/` |
| 3MF real textured | `samples/configs/openvdb/surface_shell_3mf_real.json` / `samples/models/3mf/texture2d_checker_cube.3mf` | triangles=12, boundary=0, nonManifold=0 | active=103574, inside=40931, shell=18188, interior=22743, sampled=18188, outsideColored=0, maxDistance=0.05 mm | import=10.58 ms, levelSet=848.79 ms, transfer=285.93 ms | `output/SurfaceShell3MfReal/reports/surface_shell_texture_report.json`, `output/SurfaceShell3MfReal/preview/` |
| OBJ missing texture | `samples/configs/openvdb/surface_shell_obj_missing_texture.json` / `samples/models/openvdb/surface_shell_cube_missing_texture.obj` | triangles=12, closed | sampled=0, diffuse=18188, missingTexture=18188, outsideColored=0 | levelSet=829.59 ms, transfer=49.92 ms | `output/SurfaceShellObjMissingTexture/reports/surface_shell_texture_report.json` |
| OBJ no UV | `samples/configs/openvdb/surface_shell_obj_no_uv.json` / `samples/models/openvdb/surface_shell_cube_no_uv.obj` | triangles=12, closed | sampled=0, fallback=18188, missingUV=18188, outsideColored=0 | levelSet=792.78 ms, transfer=54.07 ms | `output/SurfaceShellObjNoUv/reports/surface_shell_texture_report.json` |
| Open mesh | `samples/configs/openvdb/surface_shell_open_mesh.json` / `samples/models/openvdb/surface_shell_open_mesh.obj` | triangles=10, boundary=4 | 在 level set 前拒绝 | N/A | `output/SurfaceShellOpenMesh/reports/surface_shell_texture_report.json` |
| Non-manifold | `samples/configs/openvdb/surface_shell_non_manifold.json` / `samples/models/openvdb/surface_shell_non_manifold.obj` | triangles=13, boundary=2, nonManifold=1 | 在 level set 前拒绝 | N/A | `output/SurfaceShellNonManifold/reports/surface_shell_texture_report.json` |

说明：运行时间来自 2026-06-12 的 Debug 单次执行，仅用于阶段诊断，不作为性能基线。
