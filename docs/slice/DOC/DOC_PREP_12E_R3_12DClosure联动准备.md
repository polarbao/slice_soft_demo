# DOC_PREP_12E-R3 12D Closure 联动准备

> 文档状态：IMPLEMENTED / 12E-07 COMPLETE
> 日期：2026-07-17
> 前置任务：12E-01 至 12E-06 COMPLETE
> 覆盖任务：12E-07 12D Closure 联动

## 1. 准备结论

12E-06 已生成真实 Z 顺序的 `TextureSurfaceMask`、`ModelFillMask` 和内存 RGBWSV
诊断层。12D 已提供 `MaterialClosureSemanticLayerInput` 与 exact detector。12E-07 可以在
不写生产 TIFF、不修改 repair 规则的前提下，用一个独立 adapter 把两组证据连接起来。

当前准备只准入 `texture_model_fill_only` 诊断范围。12E diagnostic composer 没有生成支撑、
内部空洞支撑、表面光油和外侧光油的完整业务意图，因此不得把这些类别的零 mask 伪装成
完整 12D production closure PASS。

## 2. Current State

```text
12E partition：available/partitionPass，Texture XOR ModelFill = Model；
12E texture transfer：复用 closest reference，nearestQueryCount=0；
12E diagnostic composer：按真实 layerIndex/zMm 输出 RGBWSV 内存层；
12E S 通道：全部 255；
12D exact detector：已支持 texture/fill/support/varnish/empty semantic masks；
12D repair：本任务必须保持 disabled，不得修改生产层。
```

## 3. Target State

新增 engine-neutral 的 12E/12D adapter：

```text
TextureFillPartitionDiagnosticComposerResult
  -> TextureFillPartitionClosureAdapter
  -> MaterialClosureSemanticLayerInput[]
  -> AnalyzeMaterialClosureSemanticLayer
  -> diagnostic closure linkage summary
```

联动结果必须明确：

```text
scope = texture_model_fill_only；
source = semantic_masks；
confidence = exact；
productionAcceptance = not_evaluated；
repairAttempted = false；
productionOutputWritten = false。
```

## 4. Mask 来源与映射

| 12D 输入 | 12E-07 来源 | 可评价性 |
|---|---|---|
| `TextureSurfaceMask` | 12E composer 同层 exact mask | exact |
| `ModelFillMask` | 12E composer 同层 exact mask | exact |
| `ModelMaterialMask` | `TextureSurfaceMask OR ModelFillMask` | exact |
| `ModelEnvelopeMask` | 同一 exact union | exact，限当前 12E grid |
| `LayerEmptyMask` | 同层 RGBWSV 六通道均为 255 | exact |
| `ExpectedOccupiedDomainMask` | 当前 scope 使用 model union | exact，限模型域 |
| `SupportFillMask` | 当前 12E composer 不提供 | unavailable，不评价 |
| `InternalVoidSupportMask` | 当前 12E composer 不提供 | unavailable，不评价 |
| `SupportRequiredMask` | 当前 12E composer 不提供 | unavailable，不评价 |
| `SurfaceVarnishMask` | 当前 12E composer 不提供 | unavailable，不评价 |
| `OuterVarnishShellMask` | 当前 12E composer 不提供 | unavailable，不评价 |

adapter 可以为 12D DTO 填入同尺寸零 mask 以满足结构完整性，但联动 summary 必须把支撑和
光油类别标为 `not_evaluated`。零 mask 只是“本诊断范围没有证据”，不是“生产结果不存在缺口”。

## 5. 建议 DTO 与 API

建议新增：

```text
TextureFillPartitionClosureAdapterRequest
  composer
  connectivity
  maxGapPx

TextureFillPartitionClosureLayerResult
  layerIndex
  zMm
  colorFillGapVoxels
  modelDomainGapVoxels
  allTexture
  colorFillApplicability

TextureFillPartitionClosureAdapterResult
  available
  status
  scope
  source
  confidence
  productionAcceptance
  supportClosureStatus
  varnishClosureStatus
  layers
  totalColorFillGapVoxels
  repairAttempted
  issues
```

公开接口继续使用 STL 和项目 DTO，不依赖 Qt、OpenVDB 或 TIFF writer。

## 6. 普通模式与 allTexture

普通分区：

```text
textureSurfaceVoxels > 0；
modelFillVoxels > 0；
TextureSurface XOR ModelFill = Model；
ColorFillGap = 0；
colorFillApplicability = applicable。
```

allTexture：

```text
textureSurfaceVoxels = modelVoxels；
modelFillVoxels = 0；
unassignedModelVoxels = 0；
ColorFillGap = 0；
colorFillApplicability = not_applicable；
reason = all_texture_partition。
```

allTexture 不能因没有 ModelFill 邻居而被判为失败，也不能通过禁用 model fill 制造。

## 7. 不变量与阻断

联动前必须验证：

```text
composer.available=true 且 status=diagnostic；
layerCount == depth；
layerIndex 从 0 到 depth-1 严格升序；
zMm 与 grid cell center 一致；
每层 mask size == width * height；
mask 二值；
texture/fill 不重叠；
同层 composed channels size == width * height * 6；
channelOrder == R G B W S V；
texture/fill union 中不能存在六通道全 255 的空像素；
repair 不执行；
不得写 package、manifest、TIFF 或 preview。
```

任一条件失败必须返回 blocked 和稳定错误码，不允许降级成 candidate PASS。

## 8. 稳定错误码准备

建议冻结：

```text
E_12E_CLOSURE_ADAPTER_INPUT_INVALID
E_12E_CLOSURE_LAYER_ORDER_INVALID
E_12E_CLOSURE_MASK_INVALID
E_12E_CLOSURE_MODEL_DOMAIN_GAP
E_12E_CLOSURE_COLOR_FILL_GAP
E_12E_CLOSURE_CHANNEL_ORDER_INVALID
```

错误码进入 12E report；message 可演进，code 用于测试和兼容判断。

## 9. Report 合同

`texture_fill_partition_report` 可新增可选 `closureLinkage`：

```json
{
  "availability": "available",
  "status": "diagnostic",
  "scope": "texture_model_fill_only",
  "source": "semantic_masks",
  "confidence": "exact",
  "productionAcceptance": "not_evaluated",
  "colorFillApplicability": "applicable",
  "allTextureReason": null,
  "colorFillGapVoxels": 0,
  "modelDomainGapVoxels": 0,
  "supportClosureStatus": "not_evaluated",
  "varnishClosureStatus": "not_evaluated",
  "repairAttempted": false,
  "productionOutputWritten": false,
  "issues": []
}
```

该对象不能替代 production `material_closure_report`，不能输出 `productionAcceptance=passed`。

## 10. 测试矩阵

第一批单元测试：

```text
ordinary_partition_exact_pass；
all_texture_not_applicable_pass；
texture_fill_overlap_blocks；
unassigned_model_domain_blocks；
empty_pixel_inside_model_domain_detected；
layer_order_mismatch_blocks；
layer_z_mismatch_blocks；
channel_order_mismatch_blocks；
repair_remains_disabled；
repeat_result_is_deterministic。
```

报告 golden 至少断言 scope、source、confidence、allTexture reason、gap counts、支撑/光油
`not_evaluated` 和 `productionAcceptance=not_evaluated`。

## 11. 文件边界

允许修改：

```text
src/slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h/.cpp
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.h/.cpp
src/slicer_core/reports/TextureFillPartitionReport.h/.cpp
tests/unit/texture_fill_partition_closure_adapter/main.cpp
tests/unit/texture_fill_partition_report/main.cpp
tests/golden/expected/12e_texture_fill_partition_closure_linkage.json
CMakeLists.txt
docs/slice 与 docs/codex_task 的 12E 状态文档
```

禁止修改：

```text
生产 TIFF writer / manifest schema；
12D repair planner 和 repair 规则；
Qt UI；
legacy slicer_cli production pipeline；
OpenVDB 默认开关和依赖策略。
```

## 12. 验证命令

```powershell
cmake --build build --config Debug --target texture_fill_partition_closure_adapter_unit_tests texture_fill_partition_report_unit_tests
ctest --test-dir build -C Debug -R "texture_fill_partition_(closure_adapter|report)|material_closure_semantic_detector" --output-on-failure
cmake --build build-openvdb-09p --config Debug --target texture_fill_partition_closure_adapter_unit_tests texture_fill_partition_report_unit_tests
ctest --test-dir build-openvdb-09p -C Debug -R "texture_fill_partition_(closure_adapter|report)|material_closure_semantic_detector" --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

## 13. Gate 结论

```text
12E-06：COMPLETE；
12E-07：COMPLETE；
12E-08：PREPARED / BLOCKED BY PRODUCTION EVIDENCE；
12E production：NOT ADMITTED。
```

12E-07 完成不自动执行 12E-08。

## 14. 实际实现与验证

实现：

```text
TextureFillPartitionClosureAdapter 与六个稳定错误码；
partition/composer grid、二值 mask、真实 layerIndex/zMm 和 RGBWSV 顺序守门；
12E exact masks 到 MaterialClosureSemanticLayerInput 的只读映射；
model-domain empty 与 12D ColorFillGap 统计；
allTexture not_applicable(reason=all_texture_partition)；
closureLinkage report、per-layer evidence 和 golden；
support/varnish 保持 not_evaluated；repair 和 production output 保持 false。
```

实际验证：

```text
adapter 单测：10/10 PASS；
report 单测：6/6 PASS；
默认 OFF 定向 CTest：3/3 PASS；
OpenVDB ON 定向 CTest：3/3 PASS；
12D Repair Disabled：RIP strict PASS、30 层 TIFF SHA-256 invariant PASS；
默认 OFF 全量 build：PASS；全量 CTest：17/17 PASS。
```

12E-08 准备入口为 `DOC_PREP_12E_R4_ProductionAdmission准备.md`。
