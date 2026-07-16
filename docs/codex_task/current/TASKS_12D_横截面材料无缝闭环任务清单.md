# TASKS_12D 横截面材料无缝闭环任务清单

> 文档状态：Current Task Plan
> 日期：2026-07-16
> 对应文档：
> - docs/slice/DOC/DOC_DECISION_12D_横截面材料无缝闭环专项.md
> - docs/slice/DOC/DOC_DECISION_12D_R0_R1_R2_R3_材料闭环阶段拆分.md
> - docs/slice/ROADMAP/ROADMAP_12D_材料闭环分阶段执行路线.md
> - docs/slice/PRD/PRD_12D_横截面材料无缝闭环验收与修复.md
> - docs/slice/DEV/DEV_12D_材料闭环诊断与修复设计.md
> - docs/slice/DOC/DOC_SCHEMA_12D_MaterialClosureReport.md
> - docs/slice/DEMO/DEMO_12D_横截面材料无缝闭环验证方案.md
> - docs/slice/DOC/DOC_MATRIX_12D_Fixture与验收矩阵.md
> - docs/slice/DOC/DOC_PREP_12D_R2_SemanticMask精确诊断接入准备.md
> - docs/slice/DOC/DOC_PREP_12D_R2_RepairDisabled不变性验证准备.md
> - docs/slice/DOC/DOC_PREP_12D_R3_一像素修复背景保护UI真实模型准备.md

执行准入：12C、12D-R1 与 12D-R2 已完成。12D-R3 的准备文档已补齐；12D-07 已满足技术前置条件，但 repair-enabled 开发仍须用户明确指定后方可开始。

## 12D-01 文档与验收口径冻结

状态：DONE / 12D-R0

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

完成记录：

```text
开放项已关闭；
R0/R1/R2/R3 阶段已拆分；
MaterialClosureReport schema 已冻结；
DEMO、fixture matrix、CODEX_PROMPT 和准备状态报告已生成；
12C-R2-05 已完成，12D-R1 可从 12D-02 开始。
```

## 12D-02 MaterialClosureConfig

状态：DONE / 12D-R1

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

完成记录：

```text
SliceConfig 已新增 MaterialClosureConfig 与 MaterialClosureRepairConfig；
默认值已冻结为 enabled=true、mode=diagnostic、connectivity=8、maxGapPx=1、repair.enabled=false；
legacy 配置与 slicer.config.1 迁移路径均可读取 materialClosure；
非法 mode、connectivity、maxGapPx 和 repair rule 会被拒绝；
R3 实现前，repair_then_report 或 repair.enabled=true 会显式拒绝，避免配置被静默忽略；
experimental_config_unit_tests 已通过。
```

## 12D-03 MaterialClosureReport

状态：DONE / 12D-R1

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

完成记录：

```text
新增 p0.material_closure.1 报告构建模块；
在 detector 尚未实现时稳定输出 source/confidence=unavailable、closureStatus=not_available；
报告包含完整 repair、totals、worstLayers、layers 和 diagnostics 骨架；
报告构建拒绝负 layerCount，保证汇总计数非负；
slice_report.totals.materialClosure 输出稳定摘要；
manifest.reports.materialClosure 指向 reports/material_closure_report.json；
报告单元测试、sample.stl CLI 包生成和 RIP Reader 已通过。
```

## 12D-04 TIFF 反推候选诊断

状态：DONE / 12D-R1

目标：

```text
在无法取得全部 semantic mask 前，先支持 rgbwsv_tiff_inferred 候选诊断；
输出 candidate confidence，避免误判为精确生产验收。
```

完成标准：

```text
报告包含 source=rgbwsv_tiff_inferred；
报告包含 confidence=candidate；
productionAcceptance=not_evaluated 且 closureStatus 不得为 pass；
repair.attempted=false；
能输出 ColorFillGap / ModelSupportGap / ColorSupportGap。
```

完成记录：

```text
新增 MaterialClosureCandidateDetector，从最终 uint8 RGBWSV layer buffer 反推候选材料邻接；
候选检测支持 connectivity=4/8 与 maxGapPx，输出 ColorFillGap、ModelSupportGap、ColorSupportGap 和去重 gap 并集；
候选报告固定 source=rgbwsv_tiff_inferred、confidence=candidate、closureStatus=warning；
productionAcceptance 固定 not_evaluated，repair.attempted=false，不修改 TIFF；
外部连通空白只统计为 protected evidence，不作为修复输入；
synthetic detector/report 单测、sample.stl CLI、RIP Reader 和完整 CTest 已通过。
```

## 12D-05 Semantic Mask 精确诊断

状态：DONE / 12D-R2

准备记录：

```text
已冻结 semantic sidecar DTO、mask ownership 和 pipeline 插入顺序；
SupportRequiredMask 取样点固定为 support shape 后、材料优先级裁剪前；
SupportFillMask 固定为 composer 最终实际写入 S 的像素；
exact 状态矩阵、fixture、文件边界和验证命令已写入 DOC_PREP_12D_R2；
12D-05 只诊断、不修复，12D-06 再执行 TIFF SHA-256 不变性守门。
```

目标：

```text
从 composer 阶段接入 TextureSurfaceMask / ModelFillMask / SupportFillMask / OuterVarnishShellMask；
接入 ModelEnvelopeMask / SupportRequiredMask / ExpectedOccupiedDomainMask；
报告 source=semantic_masks, confidence=exact。
```

完成标准：

```text
同一 layer 的 gap 统计不依赖 preview PNG；
真实模型可输出 exact closure report。
```

完成记录：

```text
新增 MaterialClosureSemanticDetector 与逐层只读 semantic sidecar；
接入 TextureSurface / ModelFill / ModelMaterial / SupportFill / InternalVoidSupport / SurfaceVarnish / OuterVarnishShell；
SupportRequiredMask 在材料优先级裁剪前恢复支撑意图，LayerEmptyMask 由最终 RGBWSV 六通道判定；
exact report 固定 source=semantic_masks、confidence=exact，并按 gap 输出 pass/warning/fail 与生产验收状态；
sample.stl 在 preview.enabled=false 时仍输出 exact/pass，证明诊断不依赖 preview PNG；
model/obj/nai_you_new 真实 OBJ 输出 exact/pass，126 层 totalGapPixels=0；
repair.attempted=false、repairedPixels=0，RGBWSV TIFF 未在本任务中修改。
```

## 12D-06 Repair Disabled 验证

状态：DONE / 12D-R2

准备记录：

```text
已冻结 baseline/diagnostic 双配置差异；
TIFF hash 必须按 manifest layerIndex 一一比较；
已区分 package TIFF 字节不变性与 synthetic gap 保留断言；
脚本入口、失败条件、文件边界和验证命令已明确；
本任务仍只验证 repair disabled，不实现 repair。
```

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

完成记录：

```text
新增 repair_disabled_baseline/diagnostic 成对配置，除 packageDir 和 materialClosure.enabled 外保持一致；
新增 run_material_closure_tests.ps1，按 manifest layerIndex 校验 30 层 TIFF SHA-256；
baseline 与 exact diagnostic 两份 package 均通过 RIP Reader；
semantic detector 断言输入 evidence 不变且原始 gap 仍可见；
exact report 断言 repair.attempted=false、repairedPixels=0、remainingGapPixels=gapPixels；
保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print，不实现 repair。
```

## 12D-07 Repair Enabled 一像素闭环修复

状态：DONE / 12D-R3

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
2px 及以上 gap 输出 REPAIR_GAP_TOO_WIDE 且不自动修复。
```

完成记录：

```text
新增 exact analysis、保守一像素 repair plan 与 RGBWSV/semantic mask 同步应用模块；
ColorFill、ModelSupport contextual、InternalVoid 与 VarnishSupport 可按规则修复；
ColorSupport-only 保持只报告；
2x2 厚区或无法逐像素确认的组件输出 REPAIR_GAP_TOO_WIDE；
报告保留 repair 前 totalGapPixels，并以 remainingGapPixels 判定状态；
repair-enabled sample package 与 RIP Reader 通过；
Repair Disabled 30 层 TIFF SHA-256 守门继续通过；
完整 CTest 9/9 通过。
```

## 12D-08 外部背景保护

状态：PREPARED / READY FOR USER ADMISSION / 12D-R3

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

状态：PREPARED / BLOCKED BY 12D-08 / 12D-R3

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

状态：PREPARED / BLOCKED BY 12D-09 / 12D-R3

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
