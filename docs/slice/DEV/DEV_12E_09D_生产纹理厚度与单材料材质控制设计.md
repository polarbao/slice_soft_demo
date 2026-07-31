# DEV_12E-09D 生产纹理厚度与单材料材质控制设计

> 文档状态：IMPLEMENTATION DESIGN READY
> 日期：2026-07-31

## 1. 当前根因

### 1.1 诊断宽度

`MainWindow` 当前收到 `SigDiagnosticTextureSurfaceWidthChanged` 后只更新诊断成员，并明确
提示“不修改生产 Profile”。这是 09A 的正确行为，不是生产功能。

### 1.2 Legacy 生产宽度

Legacy `ShouldApplyTextureToLayer` 使用 `texture.topSurfaceLayers`。当前白墨填充 Profile 是
`top_surface_band + topSurfaceLayers=1`，不存在把诊断 `surfaceShell.widthMm` 转成生产
层数的链路。

### 1.3 单材料浮雕

白墨/光油样例分别使用 `modelMaterial.materialChannel=W/V`。通用 UI 只改
`modelFill.material` 时，核心真实通道不会完整切换。

## 2. 数据模型

新增 UI/服务 DTO：

```text
ProductionTextureControlState：
  strategy = legacy_top_band | global_surface_shell；
  requestedTopLayers；
  effectiveTopLayers；
  requestedWidthMm；
  effectiveWidthMm；
  partitionMode；
  backend；
  editable；
  lockReason；
  stale；

SingleMaterialReliefState：
  requestedMaterial = white | varnish；
  effectiveChannel = W | V；
  valid；
  issues；
```

DTO 不包含 Qt 以外层内部指针，不暴露 OpenVDB 类型。

## 3. 服务边界

建议新增：

```text
apps/slicer_debug_ui/services/ProductionTextureSettingsModel；
apps/slicer_debug_ui/services/SingleMaterialReliefResolver；
```

`ProductionTextureSettingsModel` 负责：

```text
识别 Profile 能力；
读取 requested/effective；
对 Legacy 计算有效 Z 厚度；
对 Global 维护 width/mode；
生成 session config patch。
```

`SingleMaterialReliefResolver` 负责原子生成 W/V 字段组。配置校验器复核字段一致性，
不能信任 UI 自己。

## 4. UI 设计

位置：右侧“切片设置”。

### 4.1 生产纹理

Legacy：

```text
顶面纹理层数 [整数步进]
有效 Z 厚度 [只读]
语义说明：沿切片方向，不是三维法向壳层
```

Global：

```text
纹理模式 [有限宽度 | 全纹理]
纹理壳层宽度 [mm]
有效宽度/后端/准入 [只读]
```

### 4.2 单材料

```text
模型材料 [白墨 | 光油]
有效通道 [W/V，只读]
支撑材料 [S，只读]
```

### 4.3 诊断

右侧“预检与诊断”保留现有诊断控件，并增加固定标识：

```text
只做宽度上限与几何评估；
不会修改生产切片。
```

## 5. Effective Config

### Legacy

```json
{
  "texture": {
    "applyMode": "top_surface_band",
    "topSurfaceLayers": 3
  }
}
```

报告新增/复用：

```text
requestedTopSurfaceLayers；
effectiveTopSurfaceLayers；
effectiveTopSurfaceThicknessMm；
textureThicknessSemantics=z_layer_band。
```

### Global

```json
{
  "texture": {
    "surfaceShell": {
      "mode": "partial_shell",
      "widthMm": 0.5
    }
  }
}
```

`all_texture` 时不使用一个任意大 width 代替 mode。

### Single Material

由 resolver 同时写 `modelMaterial`、`materialProcessProfile`、validation、preview
和兼容 `modelFill` 摘要。核心校验增加 W/V 一致性错误码。

## 6. 测试设计

```text
production_texture_settings_model_unit_tests；
single_material_relief_resolver_unit_tests；
production_effective_config_unit_tests 扩展；
UI Smoke：legacy-texture-layers；
UI Smoke：global-texture-width；
UI Smoke：single-relief-white-varnish；
真实/fixture package + RIP strict。
```

差分断言：

```text
Legacy 1/3/全体兼容 Profile 的 RGB 层统计变化；
Global min/mid/allTexture 的 Texture/Fill coverage 单调；
W/V 单材料的几何轮廓和 S 完全一致；
诊断参数变化不改变生产 config hash。
```

## 7. 风险

```text
Legacy 层带与 Global 法向宽度不可比较；
真实模型 topology 会阻断 Global；
纯白 RGB 与背景冲突属于 12G，不在本任务修补；
旧 Profile 中材料字段可能不完整，需要 resolver 兼容映射；
修改后现有 package 必须标记 stale 并重新切片。
```

## 8. 回滚

```text
关闭新生产控件，恢复 Profile 原值；
诊断区不受影响；
旧配置继续按原字段解析；
不修改 TIFF schema 或已有 package。
```
