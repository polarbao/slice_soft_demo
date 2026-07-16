# DOC_PREP_12D-R2 Semantic Mask 精确诊断接入准备

> 文档状态：READY / Stage 12D-R2
> 日期：2026-07-16
> 下一原子任务：12D-05 Semantic Mask 精确诊断
> 前置完成项：12D-02、12D-03、12D-04

## 1. 准备结论

12D-05 已具备进入开发的文档和代码条件。R2 只接入 exact semantic masks 并生成精确诊断报告，不实现 1px repair、不修改 TIFF、不增加 Qt gap preview。

12D-04 的 `rgbwsv_tiff_inferred` 继续保留为兼容候选轨道；12D-05 不删除、不伪装候选结果，而是在 semantic sidecar 完整时选择 `semantic_masks/exact`。

## 2. 当前代码证据与目标 Mask

| Exact mask | 当前代码证据 | 12D-05 接入方式 |
|---|---|---|
| `TextureSurfaceMask` | `compose_layer` 内的 `texture_surface_pixel` | composer sidecar 按最终实际写入语义记录 |
| `ModelFillMask` | `compose_layer` 内的 `model_fill_pixel` | composer sidecar 按最终实际写入语义记录 |
| `SupportFillMask` | 最终写入 S 通道的分支 | composer sidecar 记录实际 S 材料像素，不从 Preview 反推 |
| `InternalVoidSupportMask` | `support_type_map == InternalVoid` | 仅在最终 S 写入成功时记录 |
| `SurfaceVarnishMask` | `SurfaceVarnishMasks` outer/inner masks | 记录最终模型表面的 V 覆盖像素 |
| `OuterVarnishShellMask` | `outer_varnish_masks` | 使用策略生成的外侧壳层意图 mask，并保留最终写入 sidecar |
| `ModelEnvelopeMask` | `model_masks[layerIndex]` | 作为当前层模型业务域输入，不从 RGB/W/V 并集反推 |
| `SupportRequiredMask` | `support_generation.support_masks` | 在 support shape 完成后、`ApplyOuterVarnishSupportPriority` 前复制只读快照 |
| `ExpectedOccupiedDomainMask` | 尚未集中生成 | `ModelEnvelope | SupportRequired | OuterVarnishShellIntent` |
| `LayerEmptyMask` | 最终 interleaved RGBWSV layer | 六通道全 255 才是 Empty |

关键冻结点：

```text
SupportRequiredMask 必须在材料冲突裁剪前取样；
SupportFillMask 必须反映 composer 最终实际写入；
两者不能用同一份最终 S mask 代替；
否则“本应有支撑但最终缺失”的区域无法被 exact detector 发现。
```

## 3. 建议新增的核心 DTO

12D-05 建议新增 engine-neutral DTO，不把匿名 `slicer.cpp` 内部结构直接暴露到 report 层：

```text
MaterialClosureSemanticLayerInput
  layerIndex
  zMm
  widthPx
  heightPx
  textureSurfaceMask
  modelFillMask
  supportFillMask
  internalVoidSupportMask
  surfaceVarnishMask
  outerVarnishShellMask
  modelEnvelopeMask
  supportRequiredMask
  expectedOccupiedDomainMask
  layerEmptyMask
```

所有 mask 必须满足：

```text
size == widthPx * heightPx；
0 表示 false，非 0 表示 true；
layerIndex 与 manifest 从低 Z 到高 Z 一致；
DTO 生命周期只覆盖当前切片运行，不写入生产 TIFF。
```

## 4. Pipeline 插入顺序

```text
模型采样生成 ModelEnvelopeMask
-> 支撑生成与 support shape
-> 快照 SupportRequiredMask
-> 外侧光油/支撑优先级裁剪
-> compose_layer 写最终 RGBWSV + semantic sidecar
-> 构造 LayerEmptyMask
-> 构造 ExpectedOccupiedDomainMask
-> exact detector
-> BuildMaterialClosureExactReport
-> 原有 TIFF/report/package 发布
```

诊断服务只消费只读 mask。12D-05 不允许在 detector 内修改 `layer`、`model_masks`、`support_masks` 或 varnish masks。

## 5. Exact Gap 公式

```text
ExternalBackground = flood_fill_from_canvas_border(LayerEmptyMask)
CandidateGap = LayerEmptyMask
             & ExpectedOccupiedDomainMask
             & !ExternalBackground

ColorFillGap = CandidateGap
             & dilate(TextureSurfaceMask)
             & dilate(ModelFillMask)

ModelSupportGap = CandidateGap
                & dilate(ModelEnvelopeMask)
                & dilate(SupportFillMask)

ColorSupportGap = CandidateGap
                & dilate(TextureSurfaceMask)
                & dilate(SupportFillMask)

InternalVoidGap = LayerEmptyMask
                & inside(ModelEnvelopeMask)
                & !ExternalBackground

VarnishSupportGap = CandidateGap
                  & SupportRequiredMask
                  & dilate(OuterVarnishShellMask)
                  & dilate(SupportFillMask)
```

Gap 总数仍为所有分类 mask 的并集像素数，禁止直接相加重叠分类。

## 6. 报告状态矩阵

| 条件 | source | confidence | closureStatus | productionAcceptance |
|---|---|---|---|---|
| semantic sidecar 完整且 gap=0 | `semantic_masks` | `exact` | `pass` | `passed` |
| semantic sidecar 完整且 gap>0、`failOnGap=true` | `semantic_masks` | `exact` | `fail` | `failed` |
| semantic sidecar 完整且 gap>0、`failOnGap=false` | `semantic_masks` | `exact` | `warning` | `failed` |
| 仅有最终 RGBWSV | `rgbwsv_tiff_inferred` | `candidate` | `warning` | `not_evaluated` |
| 无诊断源 | `unavailable` | `unavailable` | `not_available` | `not_evaluated` |

R2 中 `repair.attempted=false`、`repairedPixels=0` 必须保持不变。

## 7. 12D-05 测试准备

第一批 exact detector 单元测试：

```text
closure_exact_pass；
color_fill_gap_1px；
model_support_gap_1px；
color_support_gap_1px；
internal_void_gap_1px；
varnish_support_gap_1px；
external_background_guard；
overlapping_gap_types_union_count。
```

集成测试至少断言：

```text
不依赖 preview PNG；
source=semantic_masks；
confidence=exact；
layerIndex/zMm 与 manifest 一致；
repair.attempted=false；
RGBWSV channelOrder/bitDepth/polarity 不变。
```

12D-06 再增加 materialClosure enabled/disabled 的 TIFF SHA-256 不变性验证；该哈希守门不提前混入 12D-05。

## 8. 文件边界建议

```text
src/slicer_core/diagnostics/MaterialClosureSemanticDetector.h/.cpp
src/slicer_core/reports/MaterialClosureReport.h/.cpp
src/slicer_core/slicer.cpp
tests/unit/material_closure_semantic_detector/main.cpp
CMakeLists.txt
```

若 composer sidecar 使 `slicer.cpp` 参数继续膨胀，应新增独立数据结构，不继续堆叠多个裸 `std::vector<uint8_t>&` 输出参数。

## 9. 验证命令

```powershell
cmake --build build --config Debug --target material_closure_semantic_detector_unit_tests material_closure_report_unit_tests slicer_cli rip_reader_test
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\slicer_cli.exe --config samples\configs\slice_config.json
.\build\Debug\rip_reader_test.exe --package output\SlicePackage --summary
git diff --check
```

## 10. 后续准备状态

```text
12D-05：READY TO IMPLEMENT；
12D-06：设计边界已明确，需等待 12D-05 exact report 后生成 TIFF hash baseline；
12D-07 至 12D-10：文档已规划，但 repair、UI 和真实模型执行均未准入。
```
