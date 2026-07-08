# TASKS_12D 横截面材料无缝闭环任务清单

> 文档状态：Current Task Plan
> 日期：2026-07-08
> 对应文档：
> - docs/slice/DOC/DOC_DECISION_12D_横截面材料无缝闭环专项.md
> - docs/slice/PRD/PRD_12D_横截面材料无缝闭环验收与修复.md
> - docs/slice/DEV/DEV_12D_材料闭环诊断与修复设计.md

## 12D-01 文档与验收口径冻结

目标：

```text
确认视觉闭环不等于语义闭环；
确认闭环验收基于 RGBWSV 和 semantic masks；
确认默认只诊断、不修复生产 TIFF。
```

完成标准：

```text
PRD / DEV / DOC_DECISION 已生成；
任务清单已生成；
文档内容已完整落位，无未完成标记。
```

## 12D-02 MaterialClosureConfig

目标：

```text
新增 materialClosure 配置段；
默认 enabled=true, mode=diagnostic, repair.enabled=false。
```

验证：

```powershell
cmake --build build --config Debug --target experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
```

## 12D-03 MaterialClosureReport

目标：

```text
新增 reports/material_closure_report.json；
slice_report.totals 输出 materialClosure summary。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli
.\build\Debug\slicer_cli.exe --config <closure_fixture.json>
```

## 12D-04 TIFF 反推候选诊断

目标：

```text
在无法取得全部 semantic mask 前，先支持 rgbwsv_tiff_inferred 候选诊断；
输出 candidate confidence，避免误判为精确生产验收。
```

完成标准：

```text
报告包含 source=rgbwsv_tiff_inferred；
报告包含 confidence=candidate；
能输出 ColorFillGap / ModelSupportGap / ColorSupportGap。
```

## 12D-05 Semantic Mask 精确诊断

目标：

```text
从 composer 阶段接入 TextureSurfaceMask / ModelFillMask / SupportFillMask / OuterVarnishShellMask；
报告 source=semantic_masks, confidence=exact。
```

完成标准：

```text
同一 layer 的 gap 统计不依赖 preview PNG；
真实模型可输出 exact closure report。
```

## 12D-06 Repair Disabled 验证

目标：

```text
默认只诊断，不修改生产 TIFF。
```

完成标准：

```text
repair.enabled=false 时 repairedPixels=0；
原始 gap 仍在报告中可见；
RGBWSV TIFF 与未启用 materialClosure repair 的输出一致。
```

## 12D-07 Repair Enabled 一像素闭环修复

目标：

```text
支持 maxGapPx=1 的闭环修复。
```

完成标准：

```text
ColorFillGap 修复为 ModelFill；
InternalVoidGap 修复为 SupportFill；
ModelSupportGap 按上下文修复为 ModelFill 或 SupportFill；
报告 repairedPixels > 0。
```

## 12D-08 外部背景保护

目标：

```text
确保闭环修复不会把模型外部背景误填为支撑。
```

完成标准：

```text
flood fill border background 被保护；
externalBackgroundProtectedPixels > 0；
模型外空白仍为 RGBWSV 全 255。
```

## 12D-09 UI 闭环诊断显示

目标：

```text
UI 报告/诊断区显示 closureStatus、worstLayers、gap 类型。
```

完成标准：

```text
加载输出包后可看到 material closure 状态；
可定位 worst layer；
叠加预览可显示 gap 伪彩层。
```

## 12D-10 真实模型验证

目标：

```text
优先使用 model/obj 下真实模型验证。
```

验证模型：

```text
model/obj/aishen_fudiao
model/obj/meigui_fudiao
model/obj/nai_you_new
```

完成标准：

```text
每个模型输出 material_closure_report；
每个模型输出 closureStatus；
若存在 fail，报告 worst layers 和 gap 类型；
若开启 repair，可证明 gapPixels 降低且未破坏通道协议。
```
