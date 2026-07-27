# DEV_13B MultiModelScene 规则排版与联合切片设计

> 文档版本：v0.1
> 文档状态：Formal DEV / PREPARED
> 生成日期：2026-07-24

## 1. 技术目标

在不破坏现有单模型 Legacy/Global 双模式和 `p0.rgbwsv.2` 的条件下，建立场景配置、实例资源隔离、
规则排版、联合 raster 和单 package 输出。

## 2. 目录和模块

建议边界：

```text
src/slicer_core/scene/
  MultiModelScene.*
  ModelSource.*
  ModelInstance.*
  ModelTransform.*
  ResourceScope.*

src/slicer_core/layout/
  GridLayoutPolicy.*
  BuildVolume.*
  SceneCollisionService.*

src/slicer_core/pipeline/
  MultiModelSliceOrchestrator.*
  SceneLayerComposer.*

src/slicer_core/reports/
  MultiModelSceneReport.*

apps/slicer_debug_ui/
  models/SceneDocument.*
  widgets/ModelListPanel.*
  widgets/SceneLayoutPanel.*
  workers/SceneSliceWorker.*
```

## 3. 场景配置

建议新增 session 配置合同：

```json
{
  "schema": "slicesoft.multimodel_scene.13b.1",
  "sceneId": "scene_001",
  "buildVolume": {
    "source": "unresolved",
    "widthMm": null,
    "heightMm": null,
    "origin": "unresolved",
    "xDirection": "unresolved",
    "yDirection": "unresolved",
    "isFixture": false
  },
  "layout": {
    "policy": "grid",
    "maxColumns": 11,
    "maxRows": 2,
    "columnGapMm": 20.0,
    "rowGapMm": 30.0,
    "spacingMode": "edge_clearance",
    "order": "row_major"
  },
  "materialBindingMode": "scene_profile_only",
  "models": [],
  "instances": []
}
```

模型源：

```json
{
  "modelId": "model_001",
  "sourcePath": "model/a.obj",
  "format": "obj",
  "resourceScopeId": "scope_001",
  "sourceHash": "",
  "resourceHash": ""
}
```

实例：

```json
{
  "instanceId": "inst_001",
  "modelId": "model_001",
  "transform": {
    "translateXmm": 0.0,
    "translateYmm": 0.0,
    "rotateZdeg": 0.0,
    "uniformScale": 1.0,
    "mirrorX": false,
    "mirrorY": false
  },
  "visible": true,
  "locked": false
}
```

## 4. Effective Config

场景 Effective Config 使用：

```text
subjectType=scene；
sceneId；
sceneRevision；
model hashes；
instance transform revisions；
source Profile；
requested/derived/effective layout；
dpiX/dpiY/layerHeight；
slicePipeline.mode；
per-model material binding。
```

P0 在产品规则确认前使用 `scene_profile_only`。每个实例记录 resolved Profile，但不同 Profile 请求
必须 fail closed。`buildVolume` 未知时允许 draft/save，不允许 production ready；`0.0` 不作为未知值。

它必须复用 09B 已建立的原子写入和回滚能力，并为 12E-09A-02 提供 scene-aware identity。不得覆盖
`samples/configs` 或模型源目录。

详细 schema、资源作用域、事务和稳定错误见
`DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备.md`。

## 5. 规则排版算法

输入：

```text
已完成默认姿态和实例变换的 XY 包围盒；
maxColumns/maxRows；
columnGapMm/rowGapMm；
buildVolume；
排列原点和方向。
```

确定性算法：

```text
按 instance list 稳定顺序 row_major；
每行最多 maxColumns；
当前实例 minX = 上一实例 maxX + columnGapMm；
下一行 minY = 上一行所有实例 maxY + rowGapMm；
行高取该行最大包围盒高度；
排版后整体按策略居中或对齐 buildVolume；
每次结果写入 derivedTransform，不覆盖用户源 transform；
用户手工移动后标记 layoutOverride。
```

排版结果必须对同一输入稳定，不依赖文件系统枚举顺序。

## 6. 碰撞和幅面

P0 使用两阶段检查：

```text
第一阶段：XY AABB 快速越界/重叠；
第二阶段：投影轮廓或层占用 mask 精确检查；
任一模型越界 -> BUILD_VOLUME_OUT_OF_RANGE；
实例重叠 -> INSTANCE_OVERLAP_BLOCKED；
缺少幅面 -> BUILD_VOLUME_UNDEFINED；
超过 22 个实例 -> INSTANCE_LIMIT_EXCEEDED。
```

接触边界是否允许由 epsilon 固定，建议使用不大于一个物理像素的容差，并在配置/报告中记录。

## 7. 联合切片管线

推荐不把源模型永久布尔合并：

```text
1. 逐 modelId 导入并缓存资源；
2. 逐 instance 应用 transform；
3. 逐 instance transformed preflight/admission；
4. 计算全场景 XY bbox 和共享 Z layer range；
5. 建立一个全局 RasterGrid；
6. 逐实例调用现有 Legacy 或 Global layer producer；
7. 把实例局部 layer buffer 映射到全局 layer buffer；
8. 逐层执行材料/支撑/光油合成和 closure；
9. 使用共享 RGBWSV writer 写一个 TIFF；
10. 写 manifest、scene report、slice/material/support report；
11. 原子发布 package；
12. RIP strict 验证。
```

场景内所有实例 P0 必须使用同一 `slicePipeline.mode`。混合 Legacy/Global 属于后续研究，避免同层精度和
语义不可比。

## 8. 支撑和材料合成

P0 采用实例独立、场景合成：

```text
每个实例按自身模型投影生成 lower/internal-void/upper support；
支撑只映射到全局画布；
不跨实例寻找共同支撑；
外侧光油按实例生成；
实例不重叠，因此沿用 Model > OuterVarnishShell > Support > Empty；
场景级 closure 必须检查全部实例映射后的最终画布。
```

## 9. 输出合同

生产协议保持：

```text
schema=p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
每个 layerIndex 一个 TIFF。
```

新增旁路报告：

```text
reports/multimodel_scene_report.json
schema=slicesoft.multimodel_scene_report.13b.1
```

报告至少包含：

```text
sceneId/revision；
buildVolume/layout；
modelId/instanceId/source hashes；
requested/effective transform；
per-instance admission；
per-instance bbox/layer range；
per-instance material/support stats；
global grid/layer count；
碰撞/越界；
package identity。
```

manifest 只增加可选 `reports.scene` 和 `scene` 摘要，不修改 TIFF 协议字段。严格 Reader 必须继续验证
原有必填字段，并对未知可选元数据保持兼容。

## 10. 错误与原子性

```text
任一 required instance 导入失败 -> 整场景失败；
任一 required instance strict blocked -> 整场景失败；
碰撞/越界 -> 不写最终 package；
写包中途失败 -> 清理 staging，不覆盖旧成功 package；
不得静默拆分成多个单模型 package；
不得在报告中把部分成功写成 scene PASS。
```

## 11. 性能

```text
同一 modelId 多实例共享只读几何和纹理资源；
仅缓存变换后的轻量索引或矩阵，不重复复制全部资源；
逐层实例结果可流式合成，避免保留 scene x all layers；
峰值内存预算单独记录 geometry/cache/layer/writer；
联合切片 benchmark 排除预览 PNG 写盘；
实例数测试点：1/11/12/22。
```

## 12. 测试

```text
scene schema/negative config；
稳定 grid layout；
edge clearance；
23 实例阻断；
resourceScope 同名纹理隔离；
mirror/post-transform admission；
碰撞和越界；
global layer mapping；
per-instance stats；
atomic package；
RIP strict；
Legacy/Global 单模型回归。
```
