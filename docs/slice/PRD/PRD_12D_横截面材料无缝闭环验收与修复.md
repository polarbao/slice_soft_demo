# PRD_12D 横截面材料无缝闭环验收与修复

> 文档状态：PRD
> 日期：2026-07-13
> 上游文档：PRD_12A_彩色纹理材料填充支撑光油策略.md

## 1. Goal

建立横截面材料无缝闭环能力，确保彩色纹理模型和单材料模型在每个切片层中，颜色层、模型内部填充层、支撑填充层和可选光油层之间不存在会影响打印承托的空白缝隙。

## 2. Scope

12D 面向生产 TIFF 语义，不面向 preview 图片表象。验收基于 RGBWSV 六通道和核心语义 mask。

包含：

```text
1. 逐层材料闭环诊断；
2. 缝隙类型分类；
3. 闭环报告；
4. 可选自动修复策略；
5. UI 中的闭环状态显示；
6. 真实模型验证。
```

## 3. Non-goals

```text
1. 不改变 p0.rgbwsv.2；
2. 不改变 uint8 / black_is_print；
3. 不实现 RIP；
4. 不改变默认材料优先级；
5. 不以 UI preview 截图作为验收依据；
6. 不把模型外部背景误填为支撑。
```

## 4. Definitions

### 4.1 Material Masks

```text
TextureSurfaceMask：颜色层，写 RGB；
ModelFillMask：模型内部填充层，默认写 W，也可配置为 V/RGB/其他材料；
SupportFillMask：支撑填充层，写 S；
SurfaceVarnishMask：模型表面像素上的 V；
OuterVarnishShellMask：模型外轮廓之外的 V 壳层；
EmptyMask：所有通道均不打印的区域。
```

### 4.2 Closure Gap

闭环缝隙指一个 Empty 像素或连续 Empty 区域同时邻接两个应当接触的材料区域。

典型类型：

```text
ColorFillGap：颜色层和模型填充层之间的空白；
ModelSupportGap：模型材料和支撑材料之间的空白；
ColorSupportGap：颜色层和支撑材料之间的空白；
InternalVoidGap：模型轮廓内部未被模型填充或支撑填充的空白；
VarnishSupportGap：外侧光油壳层和支撑之间的空白。
```

## 5. User Stories

### US-12D-01 逐层闭环诊断

作为调试人员，我希望每个 layer 输出闭环诊断，这样我能知道当前层是否存在材料缝隙。

验收：

```text
1. 每层输出 closureStatus = pass | warning | fail；
2. 每层输出各类 gapPixels；
3. 汇总报告输出 worstLayers；
4. UI 能定位到问题层。
```

### US-12D-02 颜色层与模型填充层无缝

作为工艺人员，我希望颜色层和模型内部填充层之间没有空白像素，避免模型横截面中间断料。

验收：

```text
1. ColorFillGap 默认必须为 0；
2. 若开启 repair，ColorFillGap 修复为 ModelFill；
3. 修复材料必须遵守 modelFill.material；
4. report 记录 repairedColorFillPixels。
```

### US-12D-03 模型材料与支撑材料无缝

作为工艺人员，我希望模型外部需要支撑的位置与支撑材料之间没有空白像素。

验收：

```text
1. ModelSupportGap 默认必须为 0；
2. 若 gap 位于模型实体内部，修复为 ModelFill；
3. 若 gap 位于模型外部承托区域，修复为 SupportFill；
4. report 记录 repairedModelSupportPixels。
```

### US-12D-04 内部镂空区域闭环

作为工艺人员，我希望模型内部镂空区域默认写 S 支撑，而不是留下不可解释空白。

验收：

```text
1. internalVoidSupport 默认开启；
2. InternalVoidGap 默认修复为 SupportFill；
3. 外部背景不得被误判为 internal void；
4. report 区分 internal_void 与 bottom_projection。
```

### US-12D-05 UI 闭环提示

作为 UI 使用者，我希望在叠加预览或诊断区看到闭环状态，而不是自己凭颜色猜测。

验收：

```text
1. UI 显示闭环总状态；
2. UI 显示问题层列表；
3. UI 显示 gap 类型和像素数；
4. UI 支持跳转到 worst layer；
5. UI 像素探针能显示该点属于 RGB/W/S/V/Empty/Gap。
```

## 6. Default Policy

默认策略：

```text
materialClosure.enabled = true
materialClosure.mode = diagnostic
materialClosure.repair.enabled = false
materialClosure.maxGapPx = 1
materialClosure.connectivity = 8
```

说明：

1. 第一阶段先诊断，不默认修复生产数据；
2. 修复需要显式开启；
3. maxGapPx 默认为 1，优先解决真实输出中最常见的一像素缝隙；
4. 后续可扩展到 2px 及以上，但必须有单独验收。

## 7. Acceptance Criteria

12D 阶段完成标准：

```text
1. 新增 material_closure_report.json；
2. slice_report 汇总 closureStatus；
3. 至少覆盖 model/obj/aishen_fudiao、model/obj/meigui_fudiao、model/obj/nai_you_new；
4. 输出每个模型的 pass/warning/fail 和 worst layers；
5. repair disabled 时只报告不改 TIFF；
6. repair enabled 时输出 repairedPixels 并保持 RGBWSV 协议不变；
7. UI 可显示闭环状态。
```

## 8. Closed Decisions

12D-R0 已冻结以下口径：

```text
1. semantic_masks exact 诊断存在 critical gap 时，生产状态为 fail；
2. rgbwsv_tiff_inferred 只能输出 candidate/warning，不能 production pass；
3. 第一批自动修复只允许 maxGapPx=1，2px 及以上只诊断；
4. VarnishSupportGap 只在 SupportRequiredMask 要求接触的位置成立；
5. gap preview 默认关闭，可用于 fixture/人工诊断，但不是生产真源；
6. repair.enabled 默认 false，repair disabled 不得改变 TIFF；
7. 外部背景必须保持 RGBWSV 全 255。
```

详细阶段和报告约束见：

```text
docs/slice/DOC/DOC_DECISION_12D_R0_R1_R2_R3_材料闭环阶段拆分.md
docs/slice/DOC/DOC_SCHEMA_12D_MaterialClosureReport.md
```
