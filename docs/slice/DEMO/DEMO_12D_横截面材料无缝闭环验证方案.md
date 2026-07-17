# DEMO_12D 横截面材料无缝闭环验证方案

> 文档状态：DEMO / VERIFIED
> 日期：2026-07-17

## 1. 验证目标

证明材料闭环诊断和可选修复满足以下要求：

```text
精确诊断基于 semantic masks；
TIFF 反推只能输出 candidate；
repair disabled 不改变生产 TIFF；
repair enabled 只处理显式允许的 1px gap；
外部背景保持 Empty；
真实模型报告可定位 worst layer；
RGBWSV 协议不变。
```

## 2. 验证轨道

### 2.1 默认 OFF 回归

不提供 `materialClosure` 或设置 `enabled=false`，输出必须与 12A 基线兼容，不要求生成闭环报告。

### 2.2 Candidate 诊断

从 RGBWSV TIFF 反推候选占用关系：

```text
source=rgbwsv_tiff_inferred；
confidence=candidate；
productionAcceptance=not_evaluated；
closureStatus 不得为 pass；
repair.attempted=false。
```

### 2.3 Exact 诊断

从 composer 取得 semantic masks：

```text
source=semantic_masks；
confidence=exact；
允许 pass/fail；
逐层 gap 分类与 preview PNG 无关。
```

### 2.4 Repair Disabled

对同一输入分别运行基线和 `mode=diagnostic`：

```text
所有 TIFF SHA-256 相同；
repairedPixels=0；
报告仍保留原始 gap。
```

### 2.5 Repair Enabled

只对 exact semantic masks 和 `maxGapPx=1` 执行：

```text
1px fixture repairedPixels > 0；
remainingGapPixels 降低；
2px fixture 输出 REPAIR_GAP_TOO_WIDE，不自动修复；
外部背景全 255。
```

## 3. Fixture 集

计划目录：

```text
samples/models/material_closure/
samples/configs/material_closure/
tests/fixtures/material_closure/
```

必须覆盖：

```text
closure_exact_pass；
color_fill_gap_1px；
model_support_gap_1px；
color_support_gap_1px；
internal_void_gap_1px；
varnish_support_gap_1px；
external_background_guard；
gap_2px_no_auto_repair；
tiff_inferred_candidate。
```

完整期望见 `DOC_MATRIX_12D_Fixture与验收矩阵.md`。

## 4. 真实模型

优先使用：

```text
model/obj/aishen_fudiao
model/obj/meigui_fudiao
model/obj/nai_you_new
```

每个模型至少记录：

```text
配置快照；
模型/贴图 hash；
layerCount；
source/confidence；
closureStatus；
各 gap 总数；
worstLayers；
repair 前后 TIFF hash；
RIP reader 结果。
```

真实模型结果允许 fail，但必须可解释、可定位，不能用人工截图替代报告。

## 5. 验证命令

以下命令是当前可运行的 12D 验收入口：

```powershell
cmake --build build --config Debug --target slicer_cli rip_reader_test experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
.\build\Debug\slicer_cli.exe --config samples\configs\material_closure\closure_exact_pass.json
.\build\Debug\rip_reader_test.exe --package output\MaterialClosureExactPass --summary
.\scripts\run_material_closure_tests.ps1 -BuildDir build -Config Debug
.\scripts\run_12d_real_model_validation.ps1 -BuildDir build -Config Debug -RunId <run-id>
```

## 6. 自动检查

`run_material_closure_tests.ps1` 计划检查：

```text
schema=p0.material_closure.1；
packageProtocol=p0.rgbwsv.2；
candidate 不得 pass；
repair disabled TIFF hash 不变；
repair enabled 只修复 1px；
external background 保持 [255,255,255,255,255,255]；
manifest channelOrder/bitDepth/polarity 不变；
worstLayers 排序稳定；
diagnostic codes 稳定。
```

## 7. UI 验收

在 12C `DiagnosticsDock` 基础上验证：

```text
显示 closureStatus 与 confidence；
candidate 明确标记“仅候选诊断”；
显示 gap 类型和像素数；
点击 worst layer 跳转真实 layerIndex；
gap preview 标记“诊断预览”；
UI 不自行重算生产结论。
```

## 8. 退出标准

```text
synthetic fixture 全部符合矩阵；
三个真实模型均生成可解析报告；
repair disabled 不改变 TIFF；
外部背景保护通过；
RIP reader 通过；
Qt UI 能只读展示报告；
阶段报告记录通过、失败和残余风险。
```

## 9. 2026-07-17 真实模型结果

| 模型 | Grid | source/confidence | closure | gap | RIP |
|---|---|---|---|---:|---|
| `nai_you_new` | `286x569x223` | `semantic_masks/exact` | `pass` | 0 | PASS |
| `aishen_fudiao` | `283x531x256` | `semantic_masks/exact` | `pass` | 0 | PASS |
| `meigui_fudiao` | `284x718x247` | `semantic_masks/exact` | `pass` | 0 | PASS |

三个模型均在 `repair.enabled=false` 下通过，不需要为获得通过结果而改写 TIFF。逐层 TIFF SHA-256、输入资产 SHA-256 和 timing 见本次 `validation_summary.json` 与 `REPORT_12D_材料闭环准备状态.md`。
