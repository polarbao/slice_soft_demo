# DEV_12D 材料闭环诊断与修复设计

> 文档状态：DEV
> 日期：2026-07-08
> 对应 PRD：PRD_12D_横截面材料无缝闭环验收与修复.md

## 1. Goal

在 legacy production path 中新增材料闭环诊断与可选修复能力，让切片输出能证明 RGB 色彩层、模型填充层、支撑填充层和光油层之间不存在生产语义缝隙。

## 2. Current State

当前已经具备：

```text
1. RGBWSV TIFF 输出；
2. textureSurfacePixels / modelFillPixels / supportPixels / varnishPixels 统计；
3. internalVoidSupport；
4. outerVarnishShell / surfaceVarnish；
5. cross_section_material_stack_report。
```

当前缺口：

```text
1. 未拆分 outer/inner color mask；
2. 未输出 material closure report；
3. 无逐层 gap 分类；
4. 无修复策略；
5. UI 不能显示 closure worst layers。
```

## 3. Data Model

新增配置段：

```json
{
  "materialClosure": {
    "enabled": true,
    "mode": "diagnostic",
    "connectivity": 8,
    "maxGapPx": 1,
    "repair": {
      "enabled": false,
      "colorFillGap": "model_fill",
      "modelSupportGap": "contextual",
      "internalVoidGap": "support",
      "varnishSupportGap": "support"
    },
    "failOnGap": true,
    "writeGapPreview": false
  }
}
```

字段说明：

```text
enabled：是否启用闭环诊断；
mode：diagnostic | repair_then_report；
connectivity：4 或 8，默认 8；
maxGapPx：需要检测/修复的最大缝隙半径，第一阶段只支持 1；
repair.enabled：是否修改生产 mask；
colorFillGap：颜色与填充之间默认补模型填充；
modelSupportGap：根据 gap 所在位置决定补模型填充或支撑；
internalVoidGap：默认补支撑；
varnishSupportGap：默认补支撑；
failOnGap：未修复 gap 是否使闭环状态 fail；
writeGapPreview：是否输出调试用 gap preview。
```

## 4. Mask Inputs

闭环诊断应尽量使用 composer 阶段的语义 mask，而不是从 preview 反推。

需要输入：

```text
TextureSurfaceMask
ModelFillMask
SupportFillMask
InternalVoidSupportMask
SurfaceVarnishMask
OuterVarnishShellMask
ModelEnvelopeMask
LayerEmptyMask
```

如果第一阶段无法取得全部语义 mask，可先用 TIFF 六通道反推候选 gap，但 report 必须标记：

```text
source = "rgbwsv_tiff_inferred"
confidence = "candidate"
```

正式生产验收必须升级为：

```text
source = "semantic_masks"
confidence = "exact"
```

## 5. Gap Detection

### 5.1 基础邻接

对每个 layer，计算所有材料 mask 的 8 邻域膨胀：

```text
dilate(Color)
dilate(ModelFill)
dilate(Model)
dilate(Support)
dilate(OuterVarnishShell)
```

Empty 像素若同时满足邻接关系，则归类为 gap：

```text
ColorFillGap = Empty & dilate(Color) & dilate(ModelFill)
ModelSupportGap = Empty & dilate(Model) & dilate(Support)
ColorSupportGap = Empty & dilate(Color) & dilate(Support)
VarnishSupportGap = Empty & dilate(OuterVarnishShell) & dilate(Support)
```

### 5.2 内部镂空

InternalVoidGap 不应只靠邻接判断。它需要基于 `ModelEnvelopeMask`：

```text
InternalVoidGap = Empty & inside(ModelEnvelopeMask) & !externalBackground
```

第一阶段可沿用 internalVoidSupport 的 enclosed-area 判断。

### 5.3 外部背景保护

任何 gap 检测和修复都必须保护模型外部背景：

```text
ExternalBackground = flood_fill_from_canvas_border(Empty)
```

`ExternalBackground` 不允许被自动修复为支撑或模型填充。

## 6. Repair Rules

修复只在 `materialClosure.mode=repair_then_report` 且 `repair.enabled=true` 时执行。

推荐规则：

```text
ColorFillGap -> ModelFill
InternalVoidGap -> SupportFill
ModelSupportGap:
  if inside ModelEnvelope -> ModelFill
  else -> SupportFill
ColorSupportGap:
  if inside ModelEnvelope -> ModelFill
  else -> SupportFill
VarnishSupportGap -> SupportFill
```

组合优先级不变：

```text
Model > OuterVarnishShell > Support > Empty
```

## 7. Report Schema

新增：

```text
reports/material_closure_report.json
```

Package summary：

```json
{
  "schema": "p0.material_closure.1",
  "enabled": true,
  "mode": "diagnostic",
  "source": "semantic_masks",
  "closureStatus": "fail",
  "totalGapPixels": 7961,
  "repairedPixels": 0,
  "worstLayers": [
    {
      "layerIndex": 50,
      "gapPixels": 77,
      "types": ["modelSupportGap", "colorSupportGap"]
    }
  ]
}
```

Layer item：

```json
{
  "layerIndex": 169,
  "zMm": 1.69,
  "closureStatus": "fail",
  "colorFillGapPixels": 20,
  "modelSupportGapPixels": 0,
  "colorSupportGapPixels": 0,
  "internalVoidGapPixels": 0,
  "varnishSupportGapPixels": 0,
  "repairedPixels": 0,
  "externalBackgroundProtectedPixels": 123456
}
```

同步到 `slice_report.totals.materialClosure`：

```json
{
  "closureStatus": "fail",
  "totalGapPixels": 7961,
  "repairedPixels": 0,
  "worstLayerIndex": 50
}
```

## 8. UI Design

UI 增强建议：

```text
1. 报告页显示 materialClosure 总状态；
2. 诊断页显示 gap 类型和 worst layers；
3. 叠加预览支持 gap 伪彩层；
4. 像素探针显示 role = EmptyGap / ExternalEmpty / ModelFill / Support；
5. 点击 worst layer 可跳转到对应层。
```

## 9. Validation

验证模型优先使用：

```text
model/obj/aishen_fudiao
model/obj/meigui_fudiao
model/obj/nai_you_new
```

验证命令：

```powershell
cmake --build build --config Debug --target slicer_cli rip_reader_test
.\build\Debug\slicer_cli.exe --config <closure_config.json>
.\build\Debug\rip_reader_test.exe --package <packageDir> --summary
```

额外检查：

```text
1. material_closure_report.json 存在；
2. closureStatus 与 gapPixels 一致；
3. repair disabled 时 TIFF 不被修复；
4. repair enabled 时 repairedPixels > 0 且 totalGapPixels 降低；
5. 外部背景保持 Empty；
6. RGBWSV 协议不变。
```

## 10. Rollback

如果 12D 修复策略导致生产输出异常：

```text
1. 将 materialClosure.mode 改回 diagnostic；
2. 将 materialClosure.repair.enabled 改为 false；
3. 保留报告，不修改 TIFF；
4. 回归到 12A 既有材料组合逻辑。
```
