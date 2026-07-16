# DEV_12D 材料闭环诊断与修复设计

> 文档状态：DEV
> 日期：2026-07-13
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
maxGapPx：允许自动修复的最大缝隙宽度，第一阶段只支持 1；
repair.enabled：是否修改生产 mask；
colorFillGap：颜色与填充之间默认补模型填充；
modelSupportGap：根据 gap 所在位置决定补模型填充或支撑；
internalVoidGap：默认补支撑；
varnishSupportGap：默认补支撑；
failOnGap：未修复 gap 是否使闭环状态 fail；
writeGapPreview：是否输出调试用 gap preview。
```

### 3.1 12D-02 配置实现边界

截至 2026-07-15，配置模型、默认值、解析、校验和 `slicer.config.1` 迁移已经实现。为避免尚未落地的修复功能被静默忽略，R3 之前对以下配置显式报错：

```text
mode = repair_then_report；
repair.enabled = true。
```

诊断模式允许配置 `maxGapPx`，但该字段在 R1/R2 仅参与诊断报告；只有 R3 修复能力完成并解除门禁后，才允许它控制生产 mask 修复。

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
SupportRequiredMask
ExpectedOccupiedDomainMask
LayerEmptyMask
```

其中：

```text
ModelEnvelopeMask：当前层模型应占据或包围的业务域，不等同于 final RGB/W/V 并集；
SupportRequiredMask：support generator 在材料冲突裁剪前产生的“应有支撑”意图 mask；
ExpectedOccupiedDomainMask：由 ModelEnvelopeMask、SupportRequiredMask 和外侧光油意图合成；
LayerEmptyMask：最终 RGBWSV 全 255 的像素。
```

`SupportRequiredMask` 不得从最终 S 通道反推，否则已经缺失的支撑区域不会进入 gap 候选。

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

### 5.1 Required Domain

先限定应当由材料占据的区域，避免把正常外部空气识别为 gap：

```text
ExpectedOccupiedDomain = ModelEnvelopeMask
                       | SupportRequiredMask
                       | OuterVarnishShellMask
ExternalBackground = flood_fill_from_canvas_border(LayerEmptyMask)
CandidateGap = LayerEmptyMask
             & ExpectedOccupiedDomain
             & !ExternalBackground
```

只有 `CandidateGap` 才进入后续分类。

### 5.2 基础邻接

对每个 layer，计算所有材料 mask 的 8 邻域膨胀：

```text
dilate(Color)
dilate(ModelFill)
dilate(Model)
dilate(Support)
dilate(OuterVarnishShell)
```

CandidateGap 像素若同时满足邻接关系，则归类为 gap：

```text
ColorFillGap = CandidateGap & dilate(Color) & dilate(ModelFill)
ModelSupportGap = CandidateGap & dilate(Model) & dilate(Support)
ColorSupportGap = CandidateGap & dilate(Color) & dilate(Support)
VarnishSupportGap = CandidateGap
                  & SupportRequiredMask
                  & dilate(OuterVarnishShell)
                  & dilate(Support)
```

### 5.3 内部镂空

InternalVoidGap 不应只靠邻接判断。它需要基于 `ModelEnvelopeMask`：

```text
InternalVoidGap = Empty & inside(ModelEnvelopeMask) & !externalBackground
```

第一阶段可沿用 internalVoidSupport 的 enclosed-area 判断。

### 5.4 外部背景保护

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
VarnishSupportGap -> SupportFill，仅限 SupportRequiredMask
```

修复限制：

```text
source 必须为 semantic_masks；
confidence 必须为 exact；
maxGapPx 第一批必须为 1；
2px 及以上 gap 只报告 REPAIR_GAP_TOO_WIDE；
任何 ExternalBackground 像素禁止写入修复 mask。
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

完整字段和不变量以 `docs/slice/DOC/DOC_SCHEMA_12D_MaterialClosureReport.md` 为准。

Package summary：

```json
{
  "schema": "p0.material_closure.1",
  "enabled": true,
  "mode": "diagnostic",
  "source": "semantic_masks",
  "confidence": "exact",
  "closureStatus": "fail",
  "productionAcceptance": "failed",
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

Candidate 轨道额外要求：

```text
source=rgbwsv_tiff_inferred；
confidence=candidate；
productionAcceptance=not_evaluated；
closureStatus 不得为 pass；
repair.attempted=false。
```

### 9.1 12D-03 报告骨架实现记录

截至 2026-07-15，`reports/material_closure_report.json` writer、`slice_report.totals.materialClosure` 摘要和 manifest 路径已经接入 legacy package 流程。12D-04 detector 尚未接入前，报告必须输出：

```text
source=unavailable；
confidence=unavailable；
closureStatus=not_available；
productionAcceptance=not_evaluated；
repair.attempted=false；
所有 gap/repaired 计数为 0；
MATERIAL_CLOSURE_SOURCE_UNAVAILABLE（仅 enabled=true 时）。
```

该骨架只表达“证据源尚不可用”，不得解释为闭环通过，也不修改 TIFF。

### 9.2 12D-04 TIFF 候选诊断实现记录

截至 2026-07-16，legacy package 流程会在 TIFF 成功写入后，使用同一份最终 interleaved RGBWSV uint8 buffer 执行只读候选诊断：

```text
ColorMask = any(R,G,B) < 255；
FillMask = W < 255 || V < 255；
SupportMask = S < 255；
ModelMask = ColorMask || FillMask；
LayerEmptyMask = RGBWSV 全 255。
```

候选 gap 只在空白像素两侧发现方向相对的材料时成立，支持 4/8 connectivity 和 `maxGapPx`。该方法可降低把普通外部空气当成 gap 的概率，但无法恢复 composer 的业务意图，尤其不能区分表面 V 与模型填充 V，也无法发现最终 S 通道已经丢失且没有邻接证据的支撑意图。因此报告必须固定：

```text
source=rgbwsv_tiff_inferred；
confidence=candidate；
closureStatus=warning；
productionAcceptance=not_evaluated；
repair.attempted=false。
```

候选检测不读取 preview PNG、不改写 TIFF，也不替代 12D-05 semantic mask exact detector。

## 10. Rollback

如果 12D 修复策略导致生产输出异常：

```text
1. 将 materialClosure.mode 改回 diagnostic；
2. 将 materialClosure.repair.enabled 改为 false；
3. 保留报告，不修改 TIFF；
4. 回归到 12A 既有材料组合逻辑。
```
