# REPORT_12D 材料闭环准备状态

> 文档状态：12D-R2 COMPLETE / 12D-R3 IN PROGRESS
> 日期：2026-07-16

## 1. 当前结论

12D 文档准备阶段已完成；12D-R1/R2 已完成。12D-R3 已完成 12D-07 一像素闭环修复和 12D-08 外部背景硬保护；12D-09 Qt 闭环诊断已解除前置阻塞并进入可执行状态，12D-10 继续等待 UI 任务。

## 2. 已准备文档

```text
DOC_DECISION_12D_横截面材料无缝闭环专项.md；
DOC_DECISION_12D_R0_R1_R2_R3_材料闭环阶段拆分.md；
ROADMAP_12D_材料闭环分阶段执行路线.md；
PRD_12D_横截面材料无缝闭环验收与修复.md；
DEV_12D_材料闭环诊断与修复设计.md；
DOC_SCHEMA_12D_MaterialClosureReport.md；
DEMO_12D_横截面材料无缝闭环验证方案.md；
DOC_MATRIX_12D_Fixture与验收矩阵.md；
DOC_PREP_12D_R2_SemanticMask精确诊断接入准备.md；
DOC_PREP_12D_R2_RepairDisabled不变性验证准备.md；
DOC_PREP_12D_R3_一像素修复背景保护UI真实模型准备.md；
TASKS_12D_横截面材料无缝闭环任务清单.md；
CODEX_PROMPT_12D_横截面材料闭环执行指令.md。
```

## 3. 已关闭问题

```text
candidate TIFF 反推不得 production pass；
exact semantic masks 才能作为生产闭环证据；
repair 默认关闭；
第一批只允许显式 1px 修复；
2px 及以上只报告；
VarnishSupportGap 受 SupportRequiredMask 限定；
gap preview 默认关闭且只作诊断；
外部背景必须保持 RGBWSV 全 255。
```

## 4. 实施阶段

| 阶段 | 内容 | 状态 |
|---|---|---|
| 12D-R0 | 文档/schema/fixture/执行边界 | COMPLETE |
| 12D-R1 | 配置、报告骨架、TIFF candidate | COMPLETE（12D-02/03/04） |
| 12D-R2 | semantic mask exact、repair-disabled 不变性 | COMPLETE（12D-05/06） |
| 12D-R3 | 1px repair、背景保护、UI、真实模型 | IN PROGRESS（12D-07/08 COMPLETE） |

## 5. 12D-02/03/04/05/06 已实现

```text
MaterialClosureConfig / MaterialClosureRepairConfig 数据模型；
materialClosure 默认值、legacy 解析与 slicer.config.1 迁移；
mode、connectivity、maxGapPx 和 repair rule 校验；
repair 尚未实现时的显式门禁，避免 repair 配置静默生效失败；
配置单元测试覆盖默认值、显式配置、迁移与负向用例。
MaterialClosureReport 独立报告构建模块；
detector 不可用阶段的 unavailable/not_available 安全报告骨架；
slice_report.totals.materialClosure 摘要与 manifest 报告路径；
报告单元测试和 CLI/RIP Reader 集成验证。
TIFF inferred candidate detector 从最终 RGBWSV buffer 只读反推材料邻接；
候选输出 ColorFillGap / ModelSupportGap / ColorSupportGap 和去重 gap 并集；
source=rgbwsv_tiff_inferred、confidence=candidate、closureStatus=warning；
productionAcceptance=not_evaluated、repair.attempted=false；
synthetic detector/report 单测、sample.stl CLI、RIP Reader 和完整 8 项 CTest 通过。
MaterialClosureSemanticDetector 与逐层只读 composer semantic sidecar；
SupportRequiredMask 与最终 SupportFillMask 的意图/实际输出分离；
五类 exact gap、外部背景保护与重叠分类并集去重；
source=semantic_masks、confidence=exact 的生产可判定报告；
preview disabled sample fixture 与 nai_you_new 真实 OBJ exact/pass 验证；
repair.attempted=false、repairedPixels=0，未实现或启用修复。
repair-disabled baseline/diagnostic 成对配置与自动验证脚本；
按 manifest layerIndex 比较全部 TIFF SHA-256，不比较预期不同的报告文件；
30 层 TIFF 字节完全一致，两份 package 均通过 RIP Reader；
detector evidence 不变与 report 原始 gap 保留单元测试。
```

## 6. 尚未实现

```text
gap preview；
Qt UI closure 展示；
真实模型自动验收脚本。
```

## 7. 准入复核

```text
12C-R1-03/R1-04：COMPLETE；
12C-R2-01 至 R2-05：COMPLETE；
REPORT_12C_Qt工作台当前状态.md：已生成；
12D PRD/DEV/DEMO/schema/fixture matrix/TASKS/CODEX_PROMPT：完整；
未发现待确认开放项；
12D-02 MaterialClosureConfig：COMPLETE；
12D-03 MaterialClosureReport：COMPLETE；
12D-04 TIFF 反推候选诊断：COMPLETE；
12D-05 semantic mask 精确诊断：COMPLETE；
12D-06 repair-disabled TIFF 不变性验证：COMPLETE；
12D-07 Repair Enabled 一像素闭环修复：COMPLETE；
12D-08 外部背景保护：COMPLETE；
下一代码任务：12D-09 Qt 闭环诊断显示，准备完成但尚未启动。
```

## 8. 后续任务准备判断

12D-05 已按 `DOC_PREP_12D_R2_SemanticMask精确诊断接入准备.md` 完成。12D-06 的双配置、TIFF 哈希范围、manifest 层顺序、gap 保留断言与失败判定已通过 `DOC_PREP_12D_R2_RepairDisabled不变性验证准备.md` 固化。R3 的 repair plan、1px 宽度、背景保护、UI 和真实模型边界已通过独立准备文档补齐。

```text
12D-05：COMPLETE；
12D-06：COMPLETE，12D-R2 已封口；
12D-07：COMPLETE；
12D-08：COMPLETE；
12D-09：PREPARED / READY FOR USER ADMISSION；
12D-10 继续受 12D-09 门禁阻塞。
```

## 9. 12D-08 完成证据

```text
repair plan 固化 ExternalBackgroundMask 与 ExpectedOccupiedDomainMask；
Apply 阶段增加 !ExternalBackground、ExpectedOccupiedDomain、!RejectedTooWide 二次守门；
border_connected_empty、closed_internal_void、narrow_neck_to_border 合成夹具通过；
外部背景修复前后 RGBWSV byte snapshot 一致，六通道保持 255；
repair-enabled package 与 RIP Reader 通过；
repair-disabled 30 层 TIFF SHA-256 不变性通过；
完整 CTest 9/9 通过。
```
