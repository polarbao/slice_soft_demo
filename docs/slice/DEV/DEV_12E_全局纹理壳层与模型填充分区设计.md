# DEV_12E 全局纹理壳层与模型填充分区设计

> 文档版本：v0.1
> 文档状态：DEV / Stage 12E Planning
> 生成日期：2026-07-16
> 对应 PRD：PRD_12E_全局纹理表面层与模型填充连续调节.md
> 实现状态：PARTIAL；Config/Service/CPU/OpenVDB Conformance/Width Sweep/Texture Transfer/Diagnostic Composer/12D Model-Domain Closure/Raster Mapping/Report 已实现，Full Closure/UI/Production 待后续任务

## 1. 技术目标

建立 engine-neutral 的全模型三维纹理/填充互补分区服务，使 `Texture Surface Layer` 宽度成为 `Model Fill Layer` 几何范围的唯一控制量。

核心不变量：

```text
TextureSurfaceMask3D AND ModelFillMask3D = 0
TextureSurfaceMask3D OR ModelFillMask3D = ModelOccupancyMask3D
TextureSurfaceCount + ModelFillCount = ModelOccupancyCount
```

切片层只能消费 3D 分区结果，不得在 composer 中重新按单层轮廓推导表面宽度。

## 2. Current Code Reality

当前 A 级代码基线：

```text
src/slicer_core/config.h
  TextureConfig: apply_mode/top_surface_layers/non_surface_rgb_policy；
  ModelFillConfig: enabled/material/scope/value/empty_allowed_in_production；

src/slicer_core/slicer.cpp
  ShouldApplyTextureToLayer 根据 column range 和 layer index 处理 full/top/band；
  ShouldApplyModelFill 只接收 textureSurfacePixel 布尔值；
  compose_layer 在单层像素循环中统计 texture_surface_pixels/model_fill_pixels；

src/slicer_core/geometry/OpenVdbSurfaceShell.*
  已有完整 3D inside/shell/interior mask；
  shell 判定使用 SDF phi 和 shell_thickness_mm；

src/slicer_core/materials/texture_application/SurfaceShellTextureService.*
  已有 shell voxel 到表面纹理属性的转移边界；

apps/slicer_debug_ui/widgets/QuickConfigPanel.*
  已有纹理策略和模型填充材料控件；
  没有全局宽度、动态最大值或覆盖率控件。
```

当前缺口：

```text
1. config 没有 global_surface_shell 正式策略和 widthMm；
2. legacy production 没有完整 3D texture/fill partition DTO；
3. OpenVDB shell 仍是 optional experimental/utility 证据，不能直接进入生产 writer；
4. report 没有 overlap/unassigned/monotonic/allTexture contract；
5. UI 没有模型分析 preflight 和动态范围。
```

## 3. Layer(s) Involved

```text
config：策略、宽度和兼容默认值；
geometry：完整三维 occupancy、distance 和 topology admission；
texture：最近表面属性/UV 传递；
materials：TextureSurface/ModelFill 互补语义；
pipeline：在 composer 前生成并缓存 3D partition；
raster/composer：按 layer 读取 partition，不重新分类；
reports：partition schema、误差、覆盖率和 backend 证据；
apps/slicer_debug_ui：宽度、动态阈值、覆盖率和预览；
tests/scripts：单调性、全纹理、OFF/ON 和真实模型回归。
```

## 4. Architecture Boundary

推荐新增服务边界：

```text
GlobalTextureFillPartitionService
  input: Scene/Mesh DTO + transformed grid + strategy config
  output: backend-neutral GlobalTextureFillPartitionResult

GlobalSurfaceDistanceBackend
  implementation candidate A: LegacyCpuGlobalDistanceBackend
  implementation candidate B: OpenVdbGlobalDistanceBackend

SurfaceTextureTransfer
  input: TextureSurfaceMask3D + closest surface reference
  output: TextureSurfaceRgb3D / transfer diagnostics

LayerSemanticComposer
  input: one Z slice of partition + material policies
  output: RGBWSV layer + exact semantic masks

TextureFillPartitionRasterMapper
  input: validated partition + texture transfer + final raster geometry
  output: true-Z raster model/texture/fill masks + texture RGB + quantization evidence
```

依赖方向：

```text
pipeline -> geometry + texture + materials + raster + reports
Qt UI -> config/report DTO or JSON
reports -> partition result DTO
output writer -> composed RGBWSV only
```

禁止：

```text
core public DTO 暴露 openvdb::Grid 类型；
UI include OpenVDB header；
distance backend 直接写 TIFF；
逐层二维 morphology 作为 global_3d_distance production backend；
USE_OPENVDB=OFF 时 global config 静默切换到不同输出语义；
未通过 strict topology admission 时写 production package。
```

## 5. Backend 候选比较

| 项目 | Legacy CPU whole-model backend | OpenVDB SDF backend |
|---|---|---|
| 算法基础 | mesh/BVH 最近三角形 + 3D inside/distance grid | OpenVDB level set + signed distance |
| 新依赖 | 无 | 无新增，但依赖现有 optional OpenVDB/vcpkg lane |
| CMake | 默认构建可用 | `USE_OPENVDB=ON` 独立构建 |
| 许可证 | 项目自有代码 | MPL-2.0，保持现有依赖治理 |
| 优点 | 默认 OFF lane 可生产候选、部署稳定 | whole-model SDF 能力已有，shell 分类清晰 |
| 风险 | 性能、内存、符号距离鲁棒性需实现验证 | 构建和内存成本、拓扑要求、不得自动 production |
| 首轮定位 | production candidate | conformance/diagnostic candidate |

决策：

```text
1. 本阶段不引入 CGAL 或其他新第三方几何库；
2. 先冻结 backend-neutral contract；
3. 默认 OFF lane 必须至少有一个可工作的 production candidate；
4. OpenVDB 用于结果交叉验证和误差评估；
5. 若 CPU candidate 未通过性能/鲁棒性 gate，不得为赶进度把 OpenVDB 静默设为默认。
```

## 6. 配置契约

建议在现有 `texture` 下新增：

```json
{
  "texture": {
    "enabled": true,
    "applyMode": "global_surface_shell",
    "surfaceShell": {
      "geometryMode": "global_3d_distance",
      "widthMm": 0.10,
      "widthStepMm": 0.01,
      "minimumWidthPolicy": "two_cells_floor_0_10_mm",
      "surfaceScope": "all_closed_surfaces",
      "fullTextureAtModelLimit": true
    }
  },
  "modelFill": {
    "enabled": true,
    "material": "white",
    "scope": "complement_of_global_texture_shell",
    "value": 0,
    "emptyAllowedInProduction": false
  }
}
```

字段说明：

```text
applyMode：新增 global_surface_shell；
geometryMode：首版只接受 global_3d_distance；
widthMm：请求宽度，单位 mm；
widthStepMm：首版固定 0.01；
minimumWidthPolicy：计算有效下限；
surfaceScope：首版 all_closed_surfaces；
fullTextureAtModelLimit：允许在模型阈值处 fill complement 为空；
modelFill.scope：显式声明 fill 是 global shell 的补集。
```

兼容规则：

```text
1. 老配置没有 surfaceShell 时不改变输出；
2. top_surface_band/solid_volume_from_top_surface 保持原行为；
3. surface_shell_from_sdf 不自动迁移；
4. global_surface_shell 在实现前由 config validator 明确拒绝，不得静默 fallback；
5. modelFill.enabled=false 不能替代 allTexture 条件。
```

校验分层：

```text
config 静态校验：有限数值、widthMm>0、widthStepMm=0.01、枚举和 texture/fill 成对关系；
model preflight 动态校验：effectiveMinimumWidthMm、拓扑、最终变换、classification resolution 和 backend capability；
结构正确但 backend 不可用时，保留 DTO，并在切片/写包前输出 E_12E_PARTITION_BACKEND_UNAVAILABLE；
不得静默 fallback 到 top_surface_band、逐层 morphology 或 surface_shell_from_sdf。
```

详细契约见 `DOC_PREP_12E_R0_ConfigDTO契约准备.md`。

## 7. 核心 DTO

建议概念：

```cpp
struct GlobalTextureFillPartitionOptions {
    double requested_width_mm{0.10};
    double width_step_mm{0.01};
    double base_minimum_width_mm{0.10};
    std::string surface_scope{"all_closed_surfaces"};
};

struct GlobalTextureFillPartitionResult {
    bool available{false};
    bool partition_pass{false};
    std::string backend;
    GridSpec3D grid;
    double requested_width_mm{0.0};
    double effective_minimum_width_mm{0.0};
    double effective_width_mm{0.0};
    double max_interior_distance_mm{0.0};
    double all_texture_threshold_mm{0.0};
    bool all_texture{false};
    Mask3D model;
    Mask3D texture_surface;
    Mask3D model_fill;
    ClosestSurfaceReference3D closest_surface;
    PartitionStats stats;
    std::vector<ValidationIssue> issues;
};
```

Public DTO 必须使用 STL/project types，不能暴露 Qt 或 OpenVDB 类型。

## 8. 全局分类算法

### 8.1 前置条件

```text
1. 输入模型完成单位、缩放、方向和最终变换；
2. 输出 XY pitch 和 layer thickness 已确定；
3. mesh diagnostics 已完成；
4. strict production 模式拒绝 self-intersection、non-manifold、duplicate/opposite duplicate、local winding blocker；
5. 构建与最终输出对齐的完整 3D classification grid。
```

### 8.2 距离和分区

对模型内部采样点 `p`：

```text
d(p) = 到 all_closed_surfaces 的三维最短欧氏距离
TextureSurface(p, w) = Model(p) AND d(p) <= w + epsilon
ModelFill(p, w) = Model(p) AND NOT TextureSurface(p, w)
```

计算：

```text
resolutionMm = max(voxelSizeMm, pixelPitchXmm, pixelPitchYmm, layerThicknessMm)
effectiveMinimumWidthMm = max(0.10, 2 * resolutionMm)
maxInteriorDistanceMm = max(d(p)), p ∈ Model
allTextureThresholdMm = max(
    effectiveMinimumWidthMm,
    ceil(maxInteriorDistanceMm / 0.01) * 0.01)
if requestedWidthMm < effectiveMinimumWidthMm: fail
effectiveWidthMm = min(requestedWidthMm, allTextureThresholdMm)
```

`epsilon` 必须与 backend/grid resolution 绑定并写入 report。不同 backend 的 epsilon 不得导致未报告的输出漂移。

### 8.3 薄壁和 medial axis

当纹理层从相对表面相遇时：

```text
1. texture mask 做 union；
2. fill complement 可局部消失；
3. 不生成 overlap 或 negative fill；
4. 最近表面距离相同的点采用稳定 tie-break；
5. 建议按 distance、surface component id、triangle id 的稳定顺序决定纹理来源；
6. tie 数量和受影响像素/体素写入 diagnostics。
```

### 8.4 纹理属性传递

只对 `TextureSurfaceMask3D` 执行：

```text
1. 根据 closest surface reference 找到最近三角形和重心坐标；
2. 复用 OBJ/MTL/PNG 或 3MF texture/color 属性；
3. 遵守 sampler、uvAddressMode、flipV、missingTexturePolicy；
4. 缺 UV/贴图按现有 policy 报告，不得把 fill 误当 fallback；
5. 全纹理模式中的内部点仍使用最近表面属性，不使用逐列顶面颜色投影。
```

### 8.5 Classification-to-Raster 映射

12E-08A 采用 world-space raster center query：

```text
rasterCenter = origin + (index + 0.5) * rasterSpacing；
sourceCell = floor((rasterCenter - classificationOrigin) / classificationSpacing)；
source cell 使用半开区间；
source 范围外保持 Empty；
model/texture/fill ownership 原样复制；
texture RGB 只随 TextureSurface ownership 复制；
禁止使用 PNG resize 或二维插值替代几何映射。
```

映射结果必须输出 coverage delta、最大中心量化误差、source cell 复用量、mappingMs 和真实
layerIndex/zMm。该步骤当前为 diagnostic-only，不直接写 TIFF。

## 9. Pipeline 插入点

建议链路：

```text
1. Load config/model/texture；
2. Transform/autoOrient；
3. Mesh diagnostics + production admission；
4. Build full 3D model occupancy；
5. Build full 3D surface distance and closest-surface reference；
6. Build TextureSurfaceMask3D；
7. Build ModelFillMask3D as exact complement；
8. Transfer texture attributes for texture mask；
9. Generate support/varnish masks under existing boundaries；
10. For each layer, read Z slice of texture/fill masks；
11. Compose RGBWSV and exact semantic masks；
12. Run 12D material closure diagnosis；
13. Write package/reports/preview。
```

关键规则：步骤 6/7 只执行一次全模型分类。步骤 10 不允许重新计算二维 texture shell。

## 10. Composer 规则

推荐伪代码：

```text
if modelPixel:
    assert textureSurfacePixel XOR modelFillPixel
    if textureSurfacePixel:
        write RGB from closest 3D surface texture
        optionally apply W underbase / V surface varnish
        semantic = TextureSurface
    else:
        write modelFill.material
        semantic = ModelFill
else if outerVarnishPixel:
    write V
else if supportPixel:
    write S
else:
    write Empty
```

材料叠加不改变主分类：纹理像素即使同时写 W/V，仍只计入 `TextureSurface`，不能同时计入 `ModelFill`。

## 11. Report Contract

建议新增独立报告：

```text
schema = slicesoft.texture_fill_partition.12e.1
path = reports/texture_fill_partition_report.json
```

完整字段、状态枚举、null 语义和与其他报告的关系见 `DOC_SCHEMA_12E_TextureFillPartitionReport.md`。12E-01 只允许输出 unavailable/blocked/not_evaluated 骨架，不得在没有实际分区结果时输出 pass。

根字段示例（数值仅用于说明结构，不代表已运行结果）：

```json
{
  "schema": "slicesoft.texture_fill_partition.12e.1",
  "status": "pass",
  "mode": "global_surface_shell",
  "backend": "legacy_cpu_global",
  "requestedWidthMm": 0.10,
  "effectiveMinimumWidthMm": 0.10,
  "effectiveWidthMm": 0.10,
  "maxInteriorDistanceMm": 0.82,
  "allTextureThresholdMm": 0.82,
  "allTexture": false,
  "resolution": {},
  "totals": {},
  "layers": [],
  "monotonicity": {},
  "issues": []
}
```

`totals` 至少包含：

```text
modelVoxels/modelPixels；
textureSurfaceVoxels/textureSurfacePixels；
modelFillVoxels/modelFillPixels；
overlapTextureFillVoxels/overlapTextureFillPixels；
unassignedModelVoxels/unassignedModelPixels；
textureCoverageRatio/modelFillCoverageRatio；
thinRegionMergedVoxels；
medialAxisTieCount；
partitionPass。
```

`layers[]` 必须证明：

```text
textureSurfacePixels + modelFillPixels == modelPixels
overlapTextureFillPixels == 0
unassignedModelPixels == 0
```

## 12. 与 12D 的关系

12E 不替代 12D closure diagnosis。

接口要求：

```text
12E 输出 exact TextureSurfaceMask 和 ModelFillMask；
12D 使用 exact semantic masks 检测 ColorFillGap/ModelSupportGap；
12E 的 partitionPass 只证明模型内部 texture/fill 分区完整；
12D 继续证明模型、支撑、光油和背景之间的生产闭环；
allTexture=true 时 ColorFillGap 应为 not_applicable 或 0，并给出原因。
```

12E production composer 接入前，12D semantic_masks exact 数据契约必须可用；12D repair 是否完成不是 R0/R1 算法原型的硬前置，但 production admission 需单独冻结。

## 13. UI 设计

### 13.1 控件

在材料设置中：

```text
texturePolicyCombo：新增“全局三维表面纹理”；
textureSurfaceWidthSlider：稳定范围映射到 0.01 mm steps；
textureSurfaceWidthSpin：QDoubleSpinBox，2 decimals，suffix mm；
effectiveMinimumLabel：只读；
allTextureThresholdLabel：只读；
textureCoverageLabel/modelFillCoverageLabel：只读；
allTextureState：只读状态；
modelFillMaterialCombo：保留材料选择，不因 fill coverage=0 删除配置。
```

slider 与 spinbox 必须双向同步，模型阈值变化时：

```text
1. block signals；
2. 更新 range；
3. clamp 当前值；
4. 写 session override；
5. 更新 effective config summary；
6. 不覆盖原始 Profile template。
```

### 13.2 Preflight

新增 backend-neutral 模型分析摘要：

```text
available/pending/blocked；
effectiveMinimumWidthMm；
allTextureThresholdMm；
estimatedTextureCoverageRatio；
estimatedModelFillCoverageRatio；
topologyBlockers；
classificationBackendRole。
```

UI 只读 report/DTO，不访问 distance grid。

### 13.3 Profile

现有 12A/12C Profile 不改默认语义。新 Profile 建议在 production gate 后新增：

```text
textured_nail_global_shell_white_fill
textured_nail_global_all_texture
```

第二个 Profile 仍保留 `modelFill.enabled=true/material=white`，只是 width 选择模型阈值，运行结果的 fill complement 为 0。

## 14. Failure Policy

必须 fail fast：

```text
非有限或小于 effective minimum 的 width；
无法计算模型动态阈值；
strict topology blocker；
partition overlap > 0；
unassigned model > 0；
global_surface_shell 请求在实现/构建中不可用；
backend 输出尺寸与 production grid 不一致；
全纹理状态下 modelFillPixels > 0，或 modelPixels != textureSurfacePixels。
```

诊断模式可以输出 report，但不得将失败结果标记为 production-safe。

## 15. 性能与内存

全 3D grid 可能显著增加内存和计算时间。R1/R2 必须记录：

```text
grid dimensions/voxel count；
occupancy bytes；
distance bytes；
closest-surface bytes；
texture/fill mask bytes；
buildDistanceMs/classifyMs/transferMs/composeMs；
peakWorkingSetBytes；
cache hit/miss；
```

优化原则：

```text
先用窄带、tile/chunk 和缓存控制内存；
不能为了性能退回逐层二维近似而不改变 mode 名称；
任何降级必须显式标记 backend/mode 和 productionAllowed=false。
```

具体预算由 12E-R1 benchmark 冻结，本规划不虚构目标数值。

## 16. Verification Plan

文档/配置阶段：

```powershell
git diff --check
```

代码阶段计划验证：

```text
L1：config/partition/monotonicity/closest-surface unit tests；
L2：generated box/thin wall/cavity golden partition report；
L3：legacy CPU 与 OpenVDB ON conformance matrix；
L4：12D semantic mask exact closure；
L5：slicer_cli package + rip_reader strict；
L6：Qt self-test/UI smoke；
L7：真实 OBJ/3MF Release 性能和内存报告。
```

脚本和 target 名称只有在对应任务实现后才能作为已运行证据。

## 17. Risks

| 风险 | 影响 | 缓解 |
|---|---|---|
| 3D grid 内存过高 | 大模型不可用 | 窄带/tile/cache，先 benchmark 再准入 |
| CPU 符号距离不稳定 | 内外分类错误 | strict topology gate + OpenVDB conformance |
| 最近表面颜色在中轴跳变 | 全纹理内部颜色不稳定 | 稳定 tie-break + tie diagnostics + fixture |
| 把 fill=0 误解为空材料 | 生产缺料 | 只允许 partition 100% texture 的合法例外 |
| UI 固定最大值 | 不同模型越界或无法全纹理 | 模型 preflight 动态阈值 |
| OpenVDB 被误升为默认 | 构建和生产边界破坏 | `USE_OPENVDB=OFF` gate + backend-neutral contract |
| 逐层近似伪装为全局 | 厚度漂移 | 3D sphere/slope/cavity validation |

## 18. Rollback

```text
1. 新 mode 必须显式启用；删除/禁用该 mode 后旧配置保持原输出；
2. 新 report 缺失不影响旧 Profile；
3. UI 新控件可通过 feature availability 隐藏，保留现有纹理策略控件；
4. OpenVDB OFF 构建始终保留；
5. production gate 失败时只保留 diagnostic partition report，不写生产 TIFF；
6. 不修改 p0.rgbwsv.2，因此不需要协议回滚。
```
