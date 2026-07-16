# DOC_PREP_12D-R3 一像素修复、背景保护、UI 与真实模型准备

> 文档状态：PREPARED / 12D-07 READY FOR USER ADMISSION
> 日期：2026-07-16
> 覆盖任务：12D-07、12D-08、12D-09、12D-10

## 1. 准备结论

12D-R3 的任务边界、依赖顺序和验收证据已补齐，可在 12D-06 完成后按 07 -> 08 -> 09 -> 10 顺序执行。当前只完成准备，不解除 repair 门禁，不修改 C++/Qt/TIFF。

## 2. 12D-07 Repair Enabled 准备

### 2.1 数据契约

现有 `MaterialClosureSemanticLayerResult` 只有计数，不能直接用于安全修复。12D-07 应新增仅存在于内存中的 analysis/plan DTO：

```text
MaterialClosureSemanticLayerAnalysis
  summary
  candidateGapMask
  colorFillGapMask
  modelSupportGapMask
  colorSupportGapMask
  internalVoidGapMask
  varnishSupportGapMask
  externalBackgroundMask

MaterialClosureRepairPlan
  modelFillRepairMask
  supportRepairMask
  rejectedTooWideMask
  attemptedPixels
  repairedPixels
```

报告层只消费统计结果，不拥有修复决策；Qt 不访问这些 pipeline 临时 mask。

### 2.2 Pipeline 顺序

```text
compose original RGBWSV layer + semantic sidecar
-> exact analyze before repair
-> build repair plan
-> apply allowed 1px repair to mutable layer
-> update affected semantic masks
-> exact analyze after repair
-> write TIFF
-> write before/after report evidence
```

repair 必须发生在 TIFF writer 前；repair disabled 必须绕过 plan/application，继续由 12D-06 hash gate 保护。

### 2.3 修复规则

```text
ColorFillGap -> 当前 modelFill.material 对应通道；
InternalVoidGap -> S=0；
ModelSupportGap -> inside ModelEnvelope 时 ModelFill，否则仅在 SupportRequiredMask 内写 S=0；
VarnishSupportGap -> 只在 SupportRequiredMask 内、OuterVarnishShell 外侧写 S=0；
ColorSupportGap -> 第一批只报告，不自动修复；
外部背景 -> 永不修复。
```

材料冲突优先级继续保持：

```text
Model > OuterVarnishShell > Support > Empty
```

### 2.4 1px 判定

不能仅用邻域命中代替宽度判定。12D-07 必须对 expected-domain empty connected component 计算宽度：

```text
可修复：组件内每个像素均位于允许的两侧材料 1px 邻域，且最大内距/跨距满足 1px；
不可修复：存在第 2 个连续空白像素厚度、无法确认两侧材料、或组件接触 external background；
不可修复项输出 REPAIR_GAP_TOO_WIDE 或稳定的拒绝 reason code。
```

### 2.5 报告口径

保持 `p0.material_closure.1`，补充并冻结以下语义：

```text
layer.gapPixels = repair 前原始 gap 并集；
layer.repair.remainingGapPixels = repair 后剩余 gap；
totals.totalGapPixels = repair 前原始 gap 并集；
totals.repairedPixels = 实际由 Empty 改为材料的去重像素；
totals.remainingGapPixels = repair 后剩余 gap（向后兼容新增字段）；
closureStatus / productionAcceptance 基于 remainingGapPixels；
repair.attempted 仅在 exact + enabled + repair_then_report 时为 true。
```

12D-07 开发前应先更新 schema 文档和报告单元测试，再实现修复。

## 3. 12D-08 外部背景保护准备

12D-08 不重新定义背景，而是把 12D-05 的 border flood-fill evidence 升级为 repair hard guard：

```text
RepairableMask &= !ExternalBackgroundMask；
RepairableMask &= ExpectedOccupiedDomainMask；
RepairableMask &= !RejectedTooWideMask。
```

必须提供三类 synthetic fixture：

```text
border_connected_empty：画布边缘连通空白保持六通道 255；
closed_internal_void：封闭内部镂空可按规则写 S；
narrow_neck_to_border：通过 1px 窄通道连接边界的空白仍视为外部背景，不得填充。
```

验证同时比较 external background 像素的 before/after byte snapshot 和计数，不只看 preview。

## 4. 12D-09 Qt 诊断显示准备

UI 只读取 `reports/material_closure_report.json`：

```text
DiagnosticsDock 显示 closureStatus / confidence / productionAcceptance；
显示 repair enabled/attempted/repaired/remaining；
显示五类 gap 和 worstLayers；
点击 worst layer 使用真实 layerIndex 跳转统一预览；
gap preview 仅在 writeGapPreview=true 且路径存在时显示；
candidate 必须显示“候选诊断，不能作为生产通过依据”；
UI 不重新计算 gap，不直接读取 semantic sidecar。
```

准备 fixture 至少包含 exact pass、exact fail、repaired-with-remaining、candidate-only、report-missing 五种报告状态。Qt 文本使用中文，JSON 枚举保持英文协议值。

## 5. 12D-10 真实模型验收准备

执行顺序与重点：

| 模型 | 重点 | repair-disabled | repair-enabled |
|---|---|---|---|
| `model/obj/nai_you_new` | 标准甲片、内部镂空与背景保护 | exact report + TIFF baseline hash | 只允许已确认 1px gap |
| `model/obj/aishen_fudiao` | 高 Z 浮雕、模型/支撑边界 | worst layers + reason code | 2px 及以上必须拒绝 |
| `model/obj/meigui_fudiao` | 复杂纹理/填充边界 | 颜色/填充 gap 分类 | 修复后 RIP 与协议校验 |

每个模型记录：

```text
配置、模型、MTL、贴图 SHA-256；
grid/layerCount；
source/confidence/status；
五类 gap、repaired、remaining、protected 计数；
worstLayers；
TIFF hash 清单；
rip_reader_test 结果；
切片与输出阶段耗时。
```

真实模型不预设全部 pass。稳定 fail + 可定位 reason code 也是有效结果；禁止人工涂改 TIFF 以换取通过。

## 6. 任务准入矩阵

| 任务 | 准备状态 | 开发准入条件 |
|---|---|---|
| 12D-07 | PREPARED / READY | 用户明确开始 |
| 12D-08 | PREPARED / BLOCKED | 12D-07 COMPLETE |
| 12D-09 | PREPARED / BLOCKED | 12D-08 COMPLETE + stable report fixture |
| 12D-10 | PREPARED / BLOCKED | 12D-09 COMPLETE + 三个模型配置冻结 |

## 7. 共同安全边界

```text
p0.rgbwsv.2 不变；
R G B W S V 不变；
uint8 / black_is_print 不变；
repair 默认 false；
只允许 exact semantic masks 驱动 repair；
candidate 永不 repair；
OpenVDB 默认关闭且不参与本阶段；
外部背景保持 RGBWSV 全 255；
2px 及以上 gap 不自动修复。
```
