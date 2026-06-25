# SURFACE_SHELL_R2_ROBUSTNESS_PERFORMANCE_CHECKLIST

> 文档版本：v0.2
> 用途：09B-R2 验证记录
> 状态：已按当前实验实现更新
> 注意：09B-R2 仍为 OpenVDB 表面壳层纹理实验链路，不进入生产 RGBWSV TIFF 输出。

## 1. 业务 Golden

| Case | Format | Triangles | Materials | Textures | Result |
|---|---|---:|---:|---:|---|
| Nail OBJ golden | OBJ | 70262 | 1 | 1 | PASS，`warn_and_attempt`，`nonProduction=true`；存在 non-manifold / local winding 诊断 |
| Nail 3MF golden | 3MF | 75596 | 3 | 3 | PASS，`warn_and_attempt`，`nonProduction=true`；存在 non-manifold / duplicate face 诊断 |
| Relief complex | OBJ/3MF | 70262 / 75596 | 1 / 3 | 1 / 3 | PARTIAL，使用真实指甲 OBJ/3MF 作为复杂浮雕代理；尚未独立建立新业务授权 fixture |

## 2. 业务 Golden Hash

| Fixture | SHA256 | Source | License |
|---|---|---|---|
| `samples/models/textured/textured_relief.obj` | `5515693FD3181AF6B8438A1F5B2428DEB054DAAC172B25795258FF7720A925EE` | 项目既有真实纹理 OBJ 样例 | 项目样例，未单独声明 |
| `samples/models/3mf/03.3mf` | `F458690369D92132F003E7D4F05FC936D0EC033C918289DE49BAC078F69552F2` | 项目既有真实 3MF 样例 | 项目样例，未单独声明 |
| `samples/models/openvdb/surface_shell_multimaterial_seam.obj` | `6F619BA75EFC0AD968BD781C6BE577FB0AB86E27EDD70BD3CF25B80A3D06E7EC` | 09B-R2 生成 fixture | 项目内部测试 fixture |
| `samples/models/openvdb/surface_shell_thin_wall.obj` | `F27A81D662F4610A5BC7F39637F59677A5CAFE7F06164820D4EBDEB20D021C00` | 09B-R2 生成 fixture | 项目内部测试 fixture |
| `samples/models/openvdb/surface_shell_duplicate_face.obj` | `56E87D16B79B784A4DBA3ED355CF62368803F2DD25187532BB842B7F089E7A9F` | 09B-R2 生成 fixture | 项目内部测试 fixture |
| `samples/models/openvdb/surface_shell_local_reversed.obj` | `5433162CF6ADCACEF8AF60DD6CBB2E0231E9997EE87BF89A920E85173A176D96` | 09B-R2 生成 fixture | 项目内部测试 fixture |
| `samples/models/openvdb/surface_shell_self_intersect.obj` | `67C4F5220FCA99E694062E7126280A4BFDBD21B73BA15D239EC1D4EA18CE0F32` | 09B-R2 生成 fixture | 项目内部测试 fixture |

## 3. 鲁棒性

| Case | Expected | Result |
|---|---|---|
| Thin wall | warning/statistics | PASS，`thin_wall` fixture 通过；报告 shell/interior 统计可见薄壁全部落在 shell |
| Sharp tip | preserved or warned | PARTIAL，当前通过 min edge / area / aspect ratio 诊断覆盖，未建立独立 sharp-tip fixture |
| Small gap | matrix result | PASS，voxel 0.10 / 0.05 / 0.025 与 shell 0.05 / 0.10 / 0.20 matrix 脚本通过 |
| Duplicate face | detected | PASS，负向 fixture 被拒绝 |
| Opposite duplicate | detected | PASS，3MF golden 与 duplicate fixture 均输出 opposite duplicate 统计 |
| Local reversed face | detected | PASS，负向 fixture 被拒绝 |
| Self-intersection | detected/sampled | PARTIAL，当前为 AABB broad-phase candidate / sampled 统计，尚非完整 narrow-phase 三角相交判定 |
| Multiple components | counted | PASS，真实 OBJ=2 components，真实 3MF=3 components |
| Zero-volume component | detected | PASS，诊断字段已输出；当前验证样例未触发 zero-volume |

## 4. Seam/Material

| Case | Expected | Result |
|---|---|---|
| UV seam | triangle-side UV | PASS，最近三角命中后按该三角 UV 采样，不跨 seam 平均 |
| Material seam | no cross-material blend | PASS，`surface_shell_multimaterial_seam` 输出 red/blue 两材质分项统计 |
| Equal-distance tie | stable deterministic hit | PASS，新增 tie-break 单测 |
| Clamp | expected colors | PASS，当前 PNG 采样使用 clamp 到边界 |
| Repeat | expected colors | NOT COVERED，尚未建立 repeat/wrap 纹理 fixture |
| Multi-texture cache | per-texture stats | PASS，3MF golden 输出 3 个 texture cache 项，seam fixture 输出 2 个 texture cache 项 |

## 5. 性能

| Triangles | Config | LevelSet ms | BVH ms | Transfer ms | Peak MB | Result |
|---:|---|---:|---:|---:|---:|---|
| 1152 | Release | 64.59 | 0.64 | 1.48 | 2.84 | PASS |
| 10400 | Release | 59.96 | 7.22 | 2.82 | 4.69 | PASS |
| 51072 | Release | 89.84 | 48.06 | 4.13 | 12.27 | PASS |
| 100k | Release | N/A | N/A | N/A | N/A | NOT RUN，可选项未执行 |

## 6. 通过用例记录

```text
multimaterial seam:
  report: output/SurfaceShellR2MultiMaterialSeam/reports/surface_shell_texture_report.json
  preview: output/SurfaceShellR2MultiMaterialSeam/preview
  voxelSizeMm: 0.05
  shellThicknessMm: 0.10
  inside/shell/interior: 40931 / 18188 / 22743
  outsideColoredVoxels: 0
  unclassifiedVoxels: 0
  sampled/diffuse/fallback: 18188 / 0 / 0
  warnings/errors: 0 / 0

nail OBJ golden:
  report: output/SurfaceShellR2NailObjGolden/reports/surface_shell_texture_report.json
  preview: output/SurfaceShellR2NailObjGolden/preview
  voxelSizeMm: 0.10
  shellThicknessMm: 0.10
  inside/shell/interior: 338713 / 112436 / 226277
  outsideColoredVoxels: 0
  unclassifiedVoxels: 0
  sampled/diffuse/fallback: 112436 / 0 / 0
  warnings/errors: 3 / 0
  nonProduction: true

nail 3MF golden:
  report: output/SurfaceShellR2Nail3MfGolden/reports/surface_shell_texture_report.json
  preview: output/SurfaceShellR2Nail3MfGolden/preview
  voxelSizeMm: 0.10
  shellThicknessMm: 0.10
  inside/shell/interior: 373358 / 116234 / 257124
  outsideColoredVoxels: 0
  unclassifiedVoxels: 0
  sampled/diffuse/fallback: 116234 / 0 / 0
  warnings/errors: 4 / 0
  nonProduction: true
```
