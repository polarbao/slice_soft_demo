# DOC_SCHEMA_12D MaterialClosureReport

> 文档状态：Schema / Frozen for implementation
> 日期：2026-07-13
> Schema：`p0.material_closure.1`

## 1. 目的

定义 `reports/material_closure_report.json` 的稳定字段，使 CLI、回归脚本和 Qt UI 使用同一闭环诊断契约。

## 2. 顶层结构

```json
{
  "schema": "p0.material_closure.1",
  "packageProtocol": "p0.rgbwsv.2",
  "enabled": true,
  "mode": "diagnostic",
  "source": "semantic_masks",
  "confidence": "exact",
  "closureStatus": "fail",
  "productionAcceptance": "failed",
  "repair": {
    "enabled": true,
    "maxGapPx": 1,
    "attempted": true,
    "repairedPixels": 20
  },
  "totals": {},
  "worstLayers": [],
  "layers": [],
  "diagnostics": []
}
```

## 3. 枚举

| 字段 | 允许值 |
|---|---|
| `mode` | `diagnostic`, `repair_then_report` |
| `source` | `semantic_masks`, `rgbwsv_tiff_inferred`, `unavailable` |
| `confidence` | `exact`, `candidate`, `unavailable` |
| `closureStatus` | `pass`, `warning`, `fail`, `not_available` |
| `productionAcceptance` | `passed`, `failed`, `not_evaluated` |

约束：

```text
confidence=candidate -> productionAcceptance=not_evaluated；
confidence=candidate -> closureStatus 不得为 pass；
source=unavailable -> closureStatus=not_available；
repair.attempted=true -> source 必须为 semantic_masks；
repair.enabled=false -> repairedPixels=0；
repair.attempted=true -> mode 必须为 repair_then_report；
closureStatus / productionAcceptance 必须基于 remainingGapPixels。
```

## 4. Totals

```json
{
  "layerCount": 573,
  "evaluatedLayerCount": 573,
  "passLayerCount": 560,
  "warningLayerCount": 0,
  "failLayerCount": 13,
  "totalGapPixels": 77,
  "colorFillGapPixels": 20,
  "modelSupportGapPixels": 18,
  "colorSupportGapPixels": 0,
  "internalVoidGapPixels": 39,
  "varnishSupportGapPixels": 0,
  "repairedPixels": 20,
  "repairedColorFillPixels": 20,
  "repairedModelSupportPixels": 0,
  "repairedInternalVoidPixels": 0,
  "repairedVarnishSupportPixels": 0,
  "remainingGapPixels": 57,
  "repairRejectedTooWidePixels": 0,
  "externalBackgroundProtectedPixels": 923456
}
```

所有像素计数必须为非负整数，`totalGapPixels` 必须等于各类去重后 gap mask 的并集像素数，不能简单相加重叠分类。

## 5. Layer Item

```json
{
  "layerIndex": 169,
  "zMm": 1.69,
  "closureStatus": "fail",
  "gapPixels": 20,
  "gaps": {
    "colorFill": 20,
    "modelSupport": 0,
    "colorSupport": 0,
    "internalVoid": 0,
    "varnishSupport": 0
  },
  "repair": {
    "attempted": true,
    "repairedPixels": 20,
    "repairedColorFillPixels": 20,
    "repairedModelSupportPixels": 0,
    "repairedInternalVoidPixels": 0,
    "repairedVarnishSupportPixels": 0,
    "remainingGapPixels": 0,
    "rejectedTooWidePixels": 0
  },
  "externalBackgroundProtectedPixels": 16422,
  "gapPreviewPath": ""
}
```

`layerIndex` 与 manifest layer list 一致，按 Z 从低到高递增；`zMm` 使用实际切片坐标。

`gapPixels` 和 `gaps` 保留 repair 前原始证据；`repair.remainingGapPixels` 表示 repair 后剩余 gap。`closureStatus` 使用后者判定。

## 6. Worst Layers

```json
{
  "layerIndex": 169,
  "zMm": 1.69,
  "gapPixels": 20,
  "types": ["COLOR_FILL_GAP"]
}
```

按 `gapPixels` 降序、`layerIndex` 升序稳定排序，默认最多记录 20 层。

## 7. Stable Diagnostic Codes

```text
MATERIAL_CLOSURE_SOURCE_UNAVAILABLE
MATERIAL_CLOSURE_CANDIDATE_ONLY
COLOR_FILL_GAP
MODEL_SUPPORT_GAP
COLOR_SUPPORT_GAP
INTERNAL_VOID_GAP
VARNISH_SUPPORT_GAP
REPAIR_REQUIRES_EXACT_MASKS
REPAIR_GAP_TOO_WIDE
EXTERNAL_BACKGROUND_PROTECTED
```

诊断项结构：

```json
{
  "severity": "error",
  "code": "COLOR_FILL_GAP",
  "message": "颜色层与模型填充之间存在空白。",
  "layerIndex": 169,
  "pixelCount": 20
}
```

## 8. Slice Report 摘要

`slice_report.totals.materialClosure` 只保存摘要：

```json
{
  "schema": "p0.material_closure.1",
  "closureStatus": "fail",
  "confidence": "exact",
  "totalGapPixels": 77,
  "repairedPixels": 20,
  "remainingGapPixels": 57,
  "worstLayerIndex": 169,
  "reportPath": "reports/material_closure_report.json"
}
```

当尚无 worst layer 时，`worstLayerIndex` 必须为 JSON `null`，不得用第 0 层或其他层作为占位。报告骨架在诊断源尚未接入时使用 `source=unavailable`、`confidence=unavailable`、`closureStatus=not_available` 和 `productionAcceptance=not_evaluated`。

## 9. 不变性

```text
packageProtocol 必须为 p0.rgbwsv.2；
通道顺序仍为 R G B W S V；
位深仍为 uint8；
极性仍为 black_is_print；
report 不得改变 TIFF 解释方式。
```
