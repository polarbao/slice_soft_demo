# REPORT_12D 材料闭环当前状态

> 文档状态：12D COMPLETE
> 日期：2026-07-17

## 1. 当前结论

12D-R0/R1/R2/R3 已全部完成。当前实现具备 candidate/exact 闭环诊断、默认关闭的一像素修复、外部背景硬保护、Qt 只读诊断和三个真实 OBJ 自动验收。三个真实模型均在 repair disabled 下得到 `semantic_masks/exact/pass`，无需修改生产 TIFF。

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
| 12D-R3 | 1px repair、背景保护、UI、真实模型 | COMPLETE（12D-07/08/09/10） |

## 5. 已实现能力

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
repair-enabled exact 一像素修复、2px 及以上拒绝和 remaining gap 重评估；
ExternalBackgroundMask、ExpectedOccupiedDomainMask 与 RejectedTooWide 三重守门；
Qt 材料闭环页、worst layer 定位和可选 gap 伪彩预览；
真实模型 effective config、输入资产 hash、逐层 TIFF hash、RIP 与 timing 自动验收。
```

## 6. 12D-R3 完成能力

```text
一像素 repair plan 和 RGBWSV/semantic mask 同步应用；
外部背景 byte snapshot 保护；
已有 gapPreviewPath 时的 Qt 诊断显示（本阶段不新增 gap preview writer）；
run_12d_real_model_validation.ps1 真实模型验收脚本；
real_model_diagnostic_template.json 可移植诊断模板。
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
12D-09 Qt 闭环诊断显示：COMPLETE；
12D-10 真实模型验证：COMPLETE；
12D：COMPLETE；
下一候选任务：12E-01 Config 与 DTO 契约，已准备但未自动启动。
```

## 8. 后续任务准备判断

12D-05 已按 `DOC_PREP_12D_R2_SemanticMask精确诊断接入准备.md` 完成。12D-06 的双配置、TIFF 哈希范围、manifest 层顺序、gap 保留断言与失败判定已通过 `DOC_PREP_12D_R2_RepairDisabled不变性验证准备.md` 固化。R3 的 repair plan、1px 宽度、背景保护、UI 和真实模型边界已通过独立准备文档补齐。

```text
12D-05：COMPLETE；
12D-06：COMPLETE，12D-R2 已封口；
12D-07：COMPLETE；
12D-08：COMPLETE；
12D-09：COMPLETE；
12D-10：COMPLETE；
12D-R3：COMPLETE；
12E-01：PREPARED / READY FOR USER ADMISSION。
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

## 10. 12D-09 完成证据

```text
Qt 诊断区域新增“材料闭环”页，只读取 reports/material_closure_report.json；
显示 closureStatus、confidence、productionAcceptance、repair、五类 gap 和 worstLayers；
candidate 明确显示不能作为生产通过依据，并阻断非法候选 pass；
点击 worst layer 使用真实 layerIndex 跳转统一预览；
仅在 gapPreviewPath 存在且文件有效时显示“RGB + 闭环 Gap”；
exact pass/fail、repaired-with-remaining、candidate-only、report-missing Smoke 通过；
12C fresh Qt lane、诊断折叠、工作区尺寸和完整 CTest 9/9 通过。
```

## 11. 12D-10 真实模型配置

统一模板：

```text
samples/configs/material_closure/real_model_diagnostic_template.json
```

冻结条件：

```text
modelTransform.scale=[1,1,1]；
texture.applyMode=top_surface_band；
modelFill.material=white；
support.placement=lower；
support.internalVoid.enabled=true；
surfaceVarnish.enabled=false；
outerVarnish.enabled=false；
materialClosure.mode=diagnostic；
materialClosure.repair.enabled=false；
preview.enabled=false；
OpenVDB disabled。
```

## 12. 真实模型实际结果

命令：

```powershell
cmake --build build --config Debug --target slicer_cli rip_reader_test
.\scripts\run_12d_real_model_validation.ps1 -BuildDir build -Config Debug -RunId 20260717_12D10_FINAL
```

结果：

| 模型 | Grid | closure | total/remaining gap | 背景保护像素 | RIP |
|---|---|---|---:|---:|---|
| `nai_you_new` | `286x569x223` | exact/pass | `0/0` | 12,493,184 | PASS |
| `aishen_fudiao` | `283x531x256` | exact/pass | `0/0` | 19,686,953 | PASS |
| `meigui_fudiao` | `284x718x247` | exact/pass | `0/0` | 21,432,795 | PASS |

五类 gap 均为 0，`productionAcceptance=passed`，`repairedPixels=0`，`worstLayers=[]`。真实模型均已通过，因此本轮未启用 repair；这不是 repair 缺测，repair enabled 的 synthetic package 已由 12D-07 覆盖。

## 13. 输入与 TIFF Hash 证据

| 模型 | OBJ SHA-256 | MTL SHA-256 | Texture SHA-256 | TIFF hash 清单 SHA-256 |
|---|---|---|---|---|
| `nai_you_new` | `675c99fe25958f0140f228e6b55d11333925d89f178da7cb950bdf433bcabd13` | `687aedad99f9570232aec51a5495068a6ea92395d928f26fda97c608f7c3f681` | `b1a4b6dbd7daf5ccb4e5ce8c1a01ccbd0991e7827384a5f597ea4a9512bf69cb` | `15b0fe787984644b9f66701449ff9dbb9db60d2f62c7a9046012b87ceac05d00` |
| `aishen_fudiao` | `5c3f2741297e687bc3e9ce34a2bf3234ba751dededf09faac0a36e81c8f83088` | `098573098f4d0b2997a7b7e3379157153852d01d5677a0e08eacdff6afad0150` | `c43943f3d11d9d3cd386d7500e6884acb58e243dcdc5260e192660b0ceed5569` | `544fe2f680fa98794d4f9ab5292771c8b360bc9649a83768c429f273316f6c19` |
| `meigui_fudiao` | `5d8affd74c54a234084cf12ed20049b75d8032e996a306c5e9cb9460cf54d70c` | `ef90304a1f4ae7b1eee389b5c508fa9fcddfae6dcc3bcfc8aa3161b161ed707e` | `2ba235d91847e102f1a8734689838f141fcf75847d63a617c06bcfae4ccd67b6` | `303350c3fee13bf49d4b6598819bcbc7fccaae0189fbbe7f1432e50b3fa041c4` |

逐层 hash 清单和 effective config 位于：

```text
output/MaterialClosureRealModelValidation/20260717_12D10_FINAL/
```

## 14. 诊断耗时

单位为毫秒；Debug 构建、preview disabled，仅作为本次可复现诊断数据，不作为 Release 性能 KPI。

| 模型 | modelLoad | sliceProcessing | layerCompute | TIFF write | report write | outputWrite | total |
|---|---:|---:|---:|---:|---:|---:|---:|
| `nai_you_new` | 3,326.350 | 27,879.763 | 12,923.205 | 1,348.980 | 1,026.382 | 2,375.363 | 33,926.376 |
| `aishen_fudiao` | 3,476.723 | 29,800.941 | 16,181.077 | 1,454.563 | 1,150.438 | 2,605.001 | 36,259.032 |
| `meigui_fudiao` | 10,626.960 | 36,730.824 | 18,359.190 | 1,626.237 | 988.422 | 2,614.659 | 50,355.868 |

## 15. 协议与后续阶段

```text
p0.rgbwsv.2：不变；
R G B W S V：不变；
uint8 / black_is_print：不变；
repair 默认 false：不变；
OpenVDB 默认 false：不变。
```

12D 已封口。12E-R0 准备材料已完整，12E-01 可以在用户明确指定后进入 Config/DTO 契约实现；不得把 12E 三维互补分区提前塞回 12D repair。

本次最终验证：

```text
cmake build slicer_cli/rip_reader_test：PASS；
12D-10 三真实模型脚本：PASS；
RepairDisabled TIFF SHA-256 不变性：PASS（30 层）；
RepairEnabled exact synthetic package：PASS；
CTest Debug：9/9 PASS。
```
