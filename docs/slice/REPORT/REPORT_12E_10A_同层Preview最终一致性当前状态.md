# REPORT 12E-10A 同层 Preview 最终一致性当前状态

> 状态：COMPLETE / 12E-10B READY
> 日期：2026-08-03
> 协议边界：`p0.rgbwsv.2` / `R G B W S V` / `uint8` / `black_is_print`

## 1. 阶段结论

12E-10A 已完成。统一预览现在把以下三类证据绑定到同一个真实生产层：

```text
生产 manifest/TIFF 的 layerIndex、zMm、DPI、像素尺寸和 RGBWSV；
09A Texture Surface / Model Fill 诊断分区；
生产 package 的 p0.material_closure.1 精确材料闭环报告。
```

缺少精确闭环报告、仅有 candidate 报告、闭环层号缺失、闭环 zMm 不一致或场景身份 stale 时，
诊断语义预览明确不可用，不允许跨层、相邻层或旧 scene 兜底。

## 2. 实现内容

### 2.1 Core 同层合同

`TextureFillPartitionSemanticPreview` 新增只读闭环证据合同，并完成：

```text
按生产 layerIndex 精确匹配闭环层；
按生产 layerThicknessMm 约束 zMm 容差；
候选闭环报告不得作为生产一致性证据；
Texture Surface / Model Fill 按生产像素中心和独立 X/Y pitch 映射；
W/S/V 直接统计同一张生产 TIFF；
闭环 PASS/FAIL 和剩余 gap 像素显式返回。
```

### 2.2 Qt 证据装配

`MaterialClosureReportInterpreter` 现在保留全部闭环层的 `layerIndex/zMm/status/gapPixels`。加载 package
时，`PreviewWorkspace` 把该报告与生产 TIFF 一起交给语义预览。状态栏明确显示：

```text
真实 layerIndex/zMm；
Texture/Fill 像素及覆盖率；
W/S/V 同层生产像素；
物理纹理宽度和 allTexture；
scene identity；
材料闭环状态和 gap 像素。
```

本阶段没有生成第二套生产 preview 文件，也没有从伪彩图反推生产材料。

## 3. 稳定错误码

```text
SEMANTIC_PREVIEW_EVIDENCE_MISSING
SEMANTIC_PREVIEW_CLOSURE_EVIDENCE_MISSING
SEMANTIC_PREVIEW_CLOSURE_NOT_EXACT
SEMANTIC_PREVIEW_CLOSURE_LAYER_MISSING
SEMANTIC_PREVIEW_CLOSURE_IDENTITY_MISMATCH
SEMANTIC_PREVIEW_PARTITION_INVALID
SEMANTIC_PREVIEW_PRODUCTION_GRID_INVALID
SEMANTIC_PREVIEW_MASK_INVALID
```

## 4. 实际验证

Debug：

```text
texture_fill_partition_semantic_preview_unit_tests：PASS
tiff_layer_source_unit_tests：PASS
material_preview_composer_unit_tests：PASS
diagnostic-semantic-preview UI smoke：PASS
material-closure-diagnostics UI smoke：PASS
slicer_debug_ui --self-test：PASS
```

Release：

```text
texture_fill_partition_semantic_preview_unit_tests：PASS
tiff_layer_source_unit_tests：PASS
material_preview_composer_unit_tests：PASS
diagnostic-semantic-preview UI smoke：PASS
material-closure-diagnostics UI smoke：PASS
```

负向覆盖包括缺证据、candidate、跨层、zMm stale、场景 revision stale 和无效生产物理网格；
正向覆盖 635/508 非等方测试夹具以及 W/S/V 同层统计。仓库现有 09D 真实 package 的
`manifest.layers` 与精确 `material_closure_report.layers` 也已核对为相同 layerIndex/zMm。

## 5. 未改变内容

```text
Legacy 仍为默认生产路线；
Global 仍为显式候选；
OpenVDB 仍为可选且默认关闭；
未修改 RGBWSV Writer、TIFF 格式、manifest schema 或 RIP；
未实施冻结的 12G-TCWS；
未吸收 Stage 13 多模型生产 Gate。
```

## 6. 下一任务

`12E-10B` 的文档、固定模型和矩阵合同已经准备完成，当前状态为 `READY`。下一阶段只执行真实
OBJ/3MF 的 Legacy/Global 双模式矩阵，不在 10B 扩展 Preview 算法或修改生产协议。
