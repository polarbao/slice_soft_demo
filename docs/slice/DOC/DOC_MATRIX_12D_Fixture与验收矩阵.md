# DOC_MATRIX_12D Fixture 与验收矩阵

> 文档状态：Matrix / Ready
> 日期：2026-07-13

## 1. Synthetic Fixture

| Fixture | Source | Repair | 期望状态 | 核心断言 |
|---|---|---:|---|---|
| `closure_exact_pass` | semantic masks | off | pass | 所有 gap=0 |
| `color_fill_gap_1px` | semantic masks | off | fail | `COLOR_FILL_GAP` > 0 |
| `color_fill_gap_1px` | semantic masks | on | pass | 修复为 ModelFill |
| `model_support_gap_1px` | semantic masks | off | fail | `MODEL_SUPPORT_GAP` > 0 |
| `model_support_gap_1px` | semantic masks | on | pass | 按 envelope 上下文补 ModelFill/S |
| `color_support_gap_1px` | semantic masks | on | pass | 按 envelope 上下文修复 |
| `internal_void_gap_1px` | semantic masks | on | pass | 修复为 S |
| `varnish_support_gap_1px` | semantic masks | on | pass | 仅 SupportRequiredMask 内补 S |
| `external_background_guard` | semantic masks | on | pass | 画布边界背景保持全 255 |
| `gap_2px_no_auto_repair` | semantic masks | on | fail | `REPAIR_GAP_TOO_WIDE`，不修复 |
| `tiff_inferred_candidate` | TIFF inferred | off | warning | `not_evaluated`，不得 pass |

## 2. 配置矩阵

| 维度 | 最小覆盖 |
|---|---|
| `enabled` | false / true |
| `mode` | diagnostic / repair_then_report |
| `repair.enabled` | false / true |
| `connectivity` | 8 为生产默认；4 仅配置校验 fixture |
| `maxGapPx` | 1 可修复；2 只诊断 |
| `writeGapPreview` | false 默认；true 诊断 fixture |
| 模型填充 | white / varnish |
| 支撑 placement | lower / both |
| outer varnish | disabled / enabled |

## 3. 生产不变性矩阵

| 条件 | TIFF 是否允许改变 | productionAcceptance |
|---|---:|---|
| `materialClosure.enabled=false` | 否 | 沿用既有生产流程 |
| `mode=diagnostic` | 否 | exact 可评估 |
| `repair.enabled=false` | 否 | exact 可评估 |
| `source=rgbwsv_tiff_inferred` | 否 | not_evaluated |
| `repair_then_report + exact + 1px` | 是，仅修复像素 | 修复后重新评估 |
| gap 宽度大于 1px | 否 | failed |

## 4. 通道断言

```text
ModelFill=white：修复像素 W=0，其余未参与通道保持 255；
ModelFill=varnish：修复像素 V=0；
SupportFill：S=0；
外部背景：RGBWSV 全 255；
任何修复不得改变 channelOrder、bitDepth、polarity。
```

## 5. 真实模型矩阵

| 模型 | 重点 | 必须输出 |
|---|---|---|
| `aishen_fudiao` | 高 Z 浮雕、模型填充与外支撑接触 | exact report、worst layers、hash |
| `meigui_fudiao` | 复杂浮雕、颜色/填充边界 | exact report、gap 分类、hash |
| `nai_you_new` | 标准甲片、下表面和内部镂空支撑 | exact report、background guard、hash |

真实模型不预设必须为 pass；若 fail，必须保留具体层和稳定 reason code。
