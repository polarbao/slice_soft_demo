# DOC_PREP_12E-R4 Production Admission 准备

> 文档状态：12E-08A COMPLETE / 12E-08B/08C TODO / 12E-08D BLOCKED
> 日期：2026-07-17
> 前置任务：12E-01 至 12E-07 COMPLETE
> 覆盖任务：12E-08 Production Admission
> 风险等级：生产路径变更，必须再次取得用户明确确认

## 1. 准备结论

12E-08 的需求、边界、准入证据、失败回滚和验证矩阵已经可以冻结，但当前不能直接进入
生产代码接入。12E-07 证明的是 `texture_model_fill_only` 的 diagnostic grid 精确闭环，不是
最终打印 raster 上包含支撑和光油的完整 production closure。

12E-08A 已关闭 classification-to-raster 缺口。当前还有三项生产证据缺口：

```text
1. 支撑、内部空洞支撑、表面光油和外侧光油尚未进入 12E 联动证据；
2. 默认 OpenVDB OFF 的 CPU candidate 尚无真实模型 Release 性能和内存准入阈值；
3. 新 Profile 的 production package、RIP strict、旧 Profile TIFF 不变性尚无实际证据。
```

因此：准备文档已完成，但执行 Gate 是 `BLOCKED`。不得因为 12E-07 的模型域 gap 为 0 就
直接写生产 TIFF。

## 2. Current State

```text
12E Config/DTO/Service：COMPLETE；
Legacy CPU global distance candidate：diagnostic，默认 OFF 构建可用；
OpenVDB conformance candidate：optional/OFF，不具备 production role；
Width Sweep/allTexture：diagnostic PASS；
Texture Transfer：OBJ/3MF diagnostic PASS；
Diagnostic Composer：12E grid 内存 RGBWSV PASS；
12D Closure Linkage：texture_model_fill_only exact PASS；
生产 TIFF/manifest/package：未接入；
Qt Profile/preview：未接入；
Production Acceptance：not_evaluated。
```

## 3. Production Target State

只有显式选择 `global_surface_shell` 的新 Profile 才允许进入候选生产链路：

```text
SceneModel + effective transform
-> strict topology admission
-> global 3D partition on classification grid
-> closest-reference texture transfer
-> deterministic classification-to-raster mapping
-> final raster semantic masks
-> model/support/varnish production composer
-> full 12D exact closure
-> production admission decision
-> RGBWSV TIFF + reports + manifest
-> RIP strict validation
```

旧 Profile、旧 apply mode 和默认 OpenVDB OFF 行为必须保持不变。

## 4. Classification Grid 到 Raster Grid 合同

12E-06 composer 目前按 `TextureFillPartitionGridSpec` 输出 voxel layer。它不能直接作为
生产 TIFF，因为 classification spacing 可能不同于打印 pixel pitch 和 layer thickness。

12E-08 前必须冻结：

```text
raster center world coordinate；
classification sample method；
inside/outside 边界规则；
texture/fill ownership 的确定性 tie；
classificationResolutionMm 与 pixelPitchX/Y/layerThickness 的关系；
空洞、薄壁和 allTexture 在重采样后的不变量；
重采样前后 coverage delta 和 quantization error 报告；
禁止用 preview resize 或 PNG 缩放代替几何映射。
```

建议首版采用 world-space raster-center query，直接消费已验证的全局距离和 closest reference
证据；不得对 12E voxel mask 做双线性图像缩放。

## 5. 完整材料域合同

最终生产层必须同时提供：

```text
TextureSurfaceMask；
ModelFillMask；
ModelMaterialMask；
ModelEnvelopeMask；
SupportRequiredMask；
SupportFillMask；
InternalVoidSupportMask；
SurfaceVarnishMask；
OuterVarnishShellMask；
ExpectedOccupiedDomainMask；
LayerEmptyMask。
```

生产优先级继续保持：

```text
Model > OuterVarnishShell > Support > Empty
```

12E-07 中支撑和光油的零 mask 只代表不可评价。12E-08 不允许复用这些零 mask 宣称完整
closure PASS。

## 6. Backend 准入策略

首个 production candidate 只能评估默认 OpenVDB OFF 的 Legacy CPU global-distance backend。

```text
Legacy CPU：候选生产 backend，仍需 Release 真实模型预算；
OpenVDB：conformance candidate，保持 optional/OFF；
OpenVDB 结果不得直接写生产 package；
若未来授予 OpenVDB production role，必须另建架构决策和独立准入任务。
```

不得通过启用 OpenVDB 解决默认 OFF lane 的性能或正确性缺口。

## 7. Production Admission Policy

建议新增独立 policy 输入，而不是在 report writer 内决定：

```text
TextureFillPartitionProductionAdmissionInput
  explicitProfileEnabled
  topologyAdmission
  partitionResult
  textureTransferResult
  rasterMappingResult
  fullClosureResult
  performanceEvidence
  regressionEvidence

TextureFillPartitionProductionAdmissionResult
  admitted
  status
  backend
  profile
  blockerCodes
  evidenceSnapshot
```

全部条件同时满足才允许 `admitted=true`：

```text
显式 global_surface_shell Profile；
strict topology PASS；
partition overlap/unassigned=0；
texture transfer outsideColored=0；
raster mapping invariants PASS；
full 12D closure PASS；
support/varnish 不再是 not_evaluated；
Release 性能和内存预算 PASS；
legacy Profile 回归 PASS；
p0.rgbwsv.2 / RIP strict PASS；
用户明确批准 production path change。
```

## 8. 稳定阻断码准备

建议冻结：

```text
E_12E_PRODUCTION_ADMISSION_NOT_CONFIRMED
E_12E_PRODUCTION_BACKEND_NOT_ADMITTED
E_12E_PRODUCTION_RASTER_MAPPING_UNAVAILABLE
E_12E_PRODUCTION_RASTER_INVARIANT_FAILED
E_12E_PRODUCTION_FULL_CLOSURE_UNAVAILABLE
E_12E_PRODUCTION_FULL_CLOSURE_FAILED
E_12E_PRODUCTION_PERFORMANCE_EVIDENCE_MISSING
E_12E_PRODUCTION_REGRESSION_EVIDENCE_MISSING
```

阻断必须发生在任何 production package publish 之前。

## 9. 分步执行建议

12E-08 不建议一次性修改生产链路，建议拆成四个守门子任务：

```text
12E-08A：COMPLETE，classification-to-raster DTO、算法和 generated fixture；
12E-08B：完整材料 semantic sidecar 与 12D full closure；
12E-08C：默认 OFF Release 真实模型性能/内存和旧 Profile 回归；
12E-08D：显式 Profile production package、RIP strict 和 admission decision。
```

每个子任务必须独立验证和提交。12E-08D 仍须用户在执行前再次确认。

## 10. 验收矩阵

### 10.1 Raster Mapping

```text
generated box/sloped/thin-wall/cavity；
minimum/intermediate/allTexture；
打印 raster 每个 model pixel 恰有 texture 或 fill；
overlap=0、unassigned=0；
真实 layerIndex/zMm；
repeat hash 稳定。
```

### 10.2 Material Closure

```text
ColorFillGap=0；
ModelSupportGap=0；
ColorSupportGap=0；
InternalVoidGap=0；
VarnishSupportGap=0；
support/varnish status 不得为 not_evaluated；
repair disabled 时 TIFF SHA-256 不变。
```

### 10.3 Production Package

```text
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
printValue=0；emptyValue=255；
manifest layer list 严格一致；
RIP Reader strict PASS；
旧 Profile TIFF hash/统计符合冻结基线。
```

### 10.4 Real Model

优先：

```text
model/obj/nai_you_new；
model/obj/aishen_fudiao；
model/obj/meigui_fudiao；
仓库内 Texture2D/ColorGroup 3MF fixture。
```

记录 Release core time、raster mapping time、texture transfer time、peak working set、输出统计和
失败原因。TIFF/PNG/JSON 写盘时间不得混入核心准入预算。

## 11. 回滚与兼容

```text
新能力只由显式 Profile 启用；
backend 或任一 Gate 失败时不写 package；
旧 Profile 继续走 legacy production path；
OpenVDB 保持 optional/OFF；
新 report 字段缺失不影响旧 package reader；
不得修改既有 package protocol、通道顺序、位深或极性。
```

## 12. 允许与禁止文件边界

12E-08A 至 08C 可在明确子任务内修改：

```text
src/slicer_core/materials/texture_application
src/slicer_core/pipeline 中独立 adapter/policy
src/slicer_core/diagnostics
src/slicer_core/reports
tests/unit 与 tests/golden
samples/configs 中显式 candidate fixture
```

12E-08D 之前禁止修改：

```text
生产 TIFF writer 行为；
manifest p0.rgbwsv.2 协议；
legacy Profile 默认 pipeline；
Qt 普通用户 Profile 列表；
OpenVDB 默认开关。
```

## 13. 准备 Gate

| 证据 | 当前状态 | 12E-08 执行要求 |
|---|---|---|
| 12E exact partition | PASS | 保持 |
| OBJ/3MF texture transfer | PASS | 保持 |
| 12E model-domain closure | PASS | 保持 |
| classification-to-raster mapping | PASS / DIAGNOSTIC | 12E-08A 已完成，继续保持不写 production |
| full support/varnish closure | NOT_EVALUATED | 必须先完成 12E-08B |
| Release real-model budget | MISSING | 必须先完成 12E-08C |
| legacy regression evidence | MISSING | 必须先完成 12E-08C |
| user production confirmation | NOT_GRANTED | 12E-08D 前再次确认 |

## 14. 最终判断

```text
12E-07：COMPLETE；
12E-08 文档准备：COMPLETE；
12E-08 代码执行：IN PROGRESS，12E-08A COMPLETE；
12E production：NOT ADMITTED。
```

本准备完成不授权任何生产写包修改。
