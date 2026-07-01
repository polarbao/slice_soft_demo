# DEV_10_TextureFidelityMetrics

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 10-3
> 生成日期：2026-07-01
> 任务：Task 10-3 Texture fidelity 指标

---

## 1. 目标

本文件定义 Stage 10 的纹理保真指标，回答 OBJ/MTL/PNG、3MF ColorGroup、3MF Texture2DGroup 和 experimental OpenVDB surface shell 路径中，哪些统计可以用于判断纹理数据链路是否可信。

本文件只定义 report / golden 可解释指标，不改变 `p0.rgbwsv.2`、不改变 `R G B W S V` 通道顺序、不实现 RIP 半色调、不代表真实打印色彩校准结果。

---

## 2. 当前实现依据

当前字段来自以下 A 级代码和样例：

```text
src/slicer_core/slicer.cpp
src/slicer_core/model.cpp
src/slicer_core/materials/texture_application/SurfaceTextureTransfer.h
src/slicer_core/materials/texture_application/SurfaceTextureTransfer.cpp
src/slicer_core/materials/texture_application/SurfaceShellRealModelReport.cpp
tests/packages/legacy/legacy_v1_tiled/reports/texture_report.json
tests/packages/legacy/legacy_v1_tiled/reports/model_report.json
tests/packages/bad/bad_tile_size/reports/three_mf_report.json
```

当前生产 package 已输出：

```text
reports/texture_report.json
reports/model_report.json
reports/three_mf_report.json
reports/slice_report.json.totals.texture
```

experimental OpenVDB surface shell 路径另有：

```text
schema = p0.surface_shell_texture_report.2
transferStats.sampledTextureVoxels
transferStats.fallbackVoxels
transferStats.missingUvVoxels
transferStats.missingTextureVoxels
transferStats.uvOutOfRangeVoxels
transferStats.repeatedSampledVoxels
transferStats.transferDistanceExceededVoxels
transferStats.queryFailedVoxels
transferStats.nearestQueryStats
```

---

## 3. 指标等级

| 等级 | 含义 | 使用方式 |
|---|---|---|
| Stable | 当前 report 已有字段支撑 | 可进入 10-6 schema / golden |
| Comparable | 可比较但不建议 exact 绑定 | 可用于真实模型验收摘要 |
| Diagnostic | 诊断字段 | 可用于 UI / 人工分析，不作为 hard gate |
| Candidate | 需要后续实现或 report 字段补齐 | 不能作为当前生产验收硬依赖 |
| NotApplicable | 当前输入类型不适用 | 需要在 summary 中明确原因 |

---

## 4. 核心指标定义

### 4.1 textureResolvedRate

| 项 | 定义 |
|---|---|
| 含义 | 纹理采样成功占参与纹理链路像素或体素的比例 |
| 生产路径 | `sampledPixels / max(1, sampledPixels + fallbackPixels + uvOutOfRangePixels)` |
| OpenVDB shell | `sampledTextureVoxels / max(1, consideredShellTextureVoxels)` |
| 等级 | Stable for production report, Comparable for OpenVDB shell |

`consideredShellTextureVoxels` 定义为：

```text
sampledTextureVoxels
+ materialDiffuseVoxels
+ fallbackVoxels
+ missingUvVoxels
+ missingTextureVoxels
+ uvOutOfRangeVoxels
+ transferDistanceExceededVoxels
+ queryFailedVoxels
```

解释规则：

```text
1.0 表示所有参与纹理链路的像素或体素均成功使用纹理采样；
0.0 表示没有有效纹理采样；
当 texture_report.enabled=false 时，指标应标记 NotApplicable，而不是写成失败。
```

### 4.2 uvCoverageRate

| 项 | 定义 |
|---|---|
| 含义 | 输入网格中可用于纹理采样的 UV 覆盖比例 |
| 字段来源 | `model_report.facesWithUv` / `model_report.facesWithoutUv`，或 `texture_report.stats.facesWithUv` / `facesWithoutUv` |
| 公式 | `facesWithUv / max(1, facesWithUv + facesWithoutUv)` |
| 等级 | Stable |

解释规则：

```text
OBJ/MTL/PNG：用于判断贴图是否能按 UV 采样；
3MF Texture2DGroup：用于判断 triangle property 是否能映射到 tex2coord；
3MF ColorGroup：如果不依赖 UV，应标记 NotApplicable。
```

### 4.3 fallbackPixelRate

| 项 | 定义 |
|---|---|
| 含义 | 因缺少纹理、缺少 UV、采样失败或策略降级而使用 fallback 的比例 |
| 生产路径 | `fallbackPixels / max(1, sampledPixels + fallbackPixels + uvOutOfRangePixels)` |
| OpenVDB shell | `fallbackVoxels / max(1, consideredShellTextureVoxels)` |
| 等级 | Stable for production report, Comparable for OpenVDB shell |

解释规则：

```text
fallbackPixelRate 越低，说明输入纹理越完整；
该指标不判断 fallback 颜色是否视觉可接受，只判断是否发生降级。
```

### 4.4 uvOutOfRangeRate

| 项 | 定义 |
|---|---|
| 含义 | UV 坐标超出纹理采样范围的比例 |
| 生产路径 | `uvOutOfRangePixels / max(1, sampledPixels + fallbackPixels + uvOutOfRangePixels)` |
| OpenVDB shell | `uvOutOfRangeVoxels / max(1, consideredShellTextureVoxels)` |
| 等级 | Stable for production report, Comparable for OpenVDB shell |

解释规则：

```text
若 UV address mode 支持 repeat，则重复采样可单独记录为 repeatedSampledVoxels；
uvOutOfRangeRate 不应与 repeatedSampledVoxels 混用。
```

### 4.5 missingTextureRate

| 项 | 定义 |
|---|---|
| 含义 | 引用到但未成功加载的纹理资源比例 |
| 生产路径 | `missingTextures / max(1, loadedTextures + missingTextures)` |
| 3MF report | `textureMissingCount / max(1, textureLoadedCount + textureMissingCount)` |
| OpenVDB shell | `missingTextureVoxels / max(1, consideredShellTextureVoxels)` |
| 等级 | Stable for resource count, Comparable for voxel count |

资源级 missing 适合做验收 hard gate；像素级 / 体素级 missing 更适合诊断影响范围。

### 4.6 materialBindingCoverage

| 项 | 定义 |
|---|---|
| 含义 | 几何面片成功绑定材质或纹理来源的覆盖比例 |
| 当前字段 | `obj_mtl_material_report` / `model_report` 可提供材质诊断，Stage 10 当前不强制 exact |
| 建议公式 | `facesWithMaterial / max(1, facesWithMaterial + facesWithoutMaterial)` |
| 等级 | Candidate |

该指标需要后续把 material binding 的面片级统计稳定写入 report 后，才能升级为 Stable。

### 4.7 colorGroupCoverage

| 项 | 定义 |
|---|---|
| 含义 | 3MF ColorGroup 成功解析到三角面的比例 |
| 字段来源 | `three_mf_report.colorGroups.resolvedTriangles` 或 `colorGroupResolvedTriangles` |
| 建议公式 | `colorGroupResolvedTriangles / max(1, triangleCount)` |
| 等级 | Comparable |

解释规则：

```text
colorGroupCount = 0 时标记 NotApplicable；
invalidReferenceCount > 0 时必须在 diagnostics 中记录，不应静默通过。
```

### 4.8 texture2DGroupCoverage

| 项 | 定义 |
|---|---|
| 含义 | 3MF Texture2DGroup 成功解析到三角面的比例 |
| 字段来源 | `three_mf_report.textures.resolvedTriangles` 或 `textureGroupResolvedTriangles` |
| 建议公式 | `textureGroupResolvedTriangles / max(1, triangleCount)` |
| 等级 | Comparable |

解释规则：

```text
texture2dGroupCount = 0 时标记 NotApplicable；
textureMissingCount > 0 时必须同时输出 missingTextureRate；
tex2CoordCount = 0 且 texture2dGroupCount > 0 时应视为高风险输入。
```

### 4.9 nearestTriangleHitRate

| 项 | 定义 |
|---|---|
| 含义 | surface shell 纹理转移时最近三角形查询成功比例 |
| 字段来源 | `transferStats.nearestQueryStats.queryCount` 与 `transferStats.queryFailedVoxels` |
| 公式 | `(queryCount - queryFailedVoxels) / max(1, queryCount)` |
| 等级 | Diagnostic |

该指标只适用于 experimental OpenVDB surface shell 路径，不进入 production package 的稳定契约。

---

## 5. 输入类型解释

### 5.1 OBJ / MTL / PNG

| 指标 | 来源 | 等级 |
|---|---|---|
| `textureResolvedRate` | `texture_report.sampledPixels` / `fallbackPixels` / `uvOutOfRangePixels` | Stable |
| `uvCoverageRate` | `texture_report.stats.facesWithUv` / `facesWithoutUv` | Stable |
| `fallbackPixelRate` | `texture_report.fallbackPixels` | Stable |
| `missingTextureRate` | `texture_report.loadedTextures` / `missingTextures` | Stable |
| `materialBindingCoverage` | material report 面片绑定统计 | Candidate |

### 5.2 3MF ColorGroup

| 指标 | 来源 | 等级 |
|---|---|---|
| `colorGroupCoverage` | `three_mf_report.colorGroupResolvedTriangles` | Comparable |
| `uvCoverageRate` | 不依赖 UV 时 `NotApplicable` | Stable status |
| `missingTextureRate` | 无 Texture2D 时 `NotApplicable` | Stable status |
| `invalidReferenceCount` | `three_mf_report.validation.invalidReferenceCount` | Stable diagnostic |

### 5.3 3MF Texture2DGroup

| 指标 | 来源 | 等级 |
|---|---|---|
| `texture2DGroupCoverage` | `three_mf_report.textureGroupResolvedTriangles` | Comparable |
| `uvCoverageRate` | `tex2CoordCount` 与面片 UV 统计 | Comparable |
| `missingTextureRate` | `textureLoadedCount` / `textureMissingCount` | Stable |
| `textureResolvedRate` | `textureSampledPixels` 与 production texture report | Comparable |

### 5.4 Experimental OpenVDB Surface Shell

| 指标 | 来源 | 等级 |
|---|---|---|
| `textureResolvedRate` | `transferStats.sampledTextureVoxels` | Comparable |
| `fallbackPixelRate` | `transferStats.fallbackVoxels` | Comparable |
| `missingTextureRate` | `transferStats.missingTextureVoxels` | Comparable |
| `uvOutOfRangeRate` | `transferStats.uvOutOfRangeVoxels` | Comparable |
| `nearestTriangleHitRate` | `transferStats.nearestQueryStats` | Diagnostic |

experimental OpenVDB surface shell 报告不能被下游当作 production package 使用；它只说明纹理壳层原型的采样质量。

---

## 6. Summary 输出建议

Stage 10-6 schema / golden 可新增 `textureFidelity` summary：

```json
{
  "available": true,
  "source": "texture_report",
  "textureResolvedRate": 1.0,
  "uvCoverageRate": 1.0,
  "fallbackPixelRate": 0.0,
  "uvOutOfRangeRate": 0.0,
  "missingTextureRate": 0.0,
  "colorGroupCoverage": {
    "available": false,
    "reason": "not_applicable"
  },
  "texture2DGroupCoverage": {
    "available": false,
    "reason": "not_applicable"
  },
  "diagnostics": []
}
```

字段规则：

```text
available=false 必须带 reason；
NotApplicable 不是失败；
生产 package 的 summary 不应引用 preview PNG 颜色；
路径字段不参与 exact golden；
ratio 默认容差为 1e-6。
```

---

## 7. Golden 比较规则

### 7.1 Exact

```text
enabled
loadedTextures
missingTextures
facesWithUv
facesWithoutUv
sampledPixels
fallbackPixels
uvOutOfRangePixels
colorGroupCount
texture2dCount
texture2dGroupCount
textureLoadedCount
textureMissingCount
invalidReferenceCount
```

### 7.2 Tolerance

```text
textureResolvedRate
uvCoverageRate
fallbackPixelRate
uvOutOfRangeRate
missingTextureRate
colorGroupCoverage
texture2DGroupCoverage
nearestTriangleHitRate
```

建议默认容差：

```text
absoluteTolerance = 1e-6
```

### 7.3 Diagnostic Only

```text
warnings 文案；
绝对纹理路径；
preview PNG 显示颜色；
OpenVDB nearest query performance counters；
uniqueColorCount；
textureCacheBytes。
```

---

## 8. 非目标范围

```text
不定义 ICC / 色彩管理 / 打印机校准；
不把 true-color preview 当成生产 RGBWSV 数据；
不把 fallback 视觉效果当成通过条件；
不实现 RIP 半色调或设备 bitstream；
不默认启用 OpenVDB；
不要求下游解析 experimental surface shell report。
```

---

## 9. 后续任务衔接

```text
10-4：用真实模型验收集标注每个模型的纹理指标期望值和风险原因。
10-6：将本指标文档转成 output contract schema / golden summary。
11：UI layer preview 可展示这些指标，但不能用 preview PNG 反推生产统计。
```
