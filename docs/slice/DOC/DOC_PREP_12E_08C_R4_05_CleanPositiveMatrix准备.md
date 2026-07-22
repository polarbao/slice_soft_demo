# DOC_PREP_12E-08C-R4-05 Clean Positive Matrix 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-22
> 前置任务：R4-01..04 COMPLETE
> 生产边界：只生成诊断矩阵和报告，不写生产 TIFF/package，不解除 R4-06..08 blocker

## 1. 任务目标

R4-05 使用无需重建的真实 OBJ 和已有 Texture2D 3MF，证明现有 12E 诊断链在三个宽度端点上
满足 `Texture Surface Layer + Model Fill Layer = Model`，并证明 Model Fill 材料最终只解析到既有
`R G B W S V` 通道语义。

本任务解决“正常模型是否能继续推进 12E 功能”，不解决 `nai_you/aishen/meigui` 三个 required
模型的复杂自相交重建问题。

## 2. 固定边界

```text
legacy 仍为默认生产路径；
global_surface_shell 仍为 diagnostic-only；
使用 LegacyCpuGlobalDistanceBackend 建立默认 OFF 正向基线；
不修改 p0.rgbwsv.2、通道顺序、uint8 或 black_is_print；
不写 TIFF、manifest、preview 或 RIP 输出；
不将 C/M/Y/K 增加为 TIFF 通道；
不将正常模型 PASS 计入 required repair PASS；
不修改、覆盖或提交 model/obj 下的用户模型资产。
```

## 3. 验证模型基线

模型依据以
`docs/slice/REPORT/REPORT_12E_08C_R4_模型资产预检清单.md` 为真源。

### 3.1 必跑输入

| Case ID | 输入 | 用途 |
|---|---|---|
| `clean_obj_primary` | `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | 主彩色 OBJ，验证多材质/贴图和完整 width 矩阵 |
| `clean_obj_independent` | `model/obj/yecan/3.obj` | 独立 OBJ 复核，防止单系列偶然 PASS |
| `clean_3mf_texture2d` | `samples/models/3mf/texture2d_checker_cube.3mf` | Texture2D 3MF 正向 fixture |

### 3.2 扩展输入

`xiao_ma_wu_yu_new` 其余四个 OBJ 可作扩展覆盖。`model/obj/yecan/4.obj` 已通过完整自相交审计，
但当前是未跟踪用户资产：存在时可只读执行本地扩展验证，不得成为 CI 必需输入，不得
纳入任务提交。

`model/3mf` 当前没有 strict PASS 模型，不得将其中三个真实 3MF 改写为正向 fixture。

## 4. 现有能力与实施缺口

### 4.1 直接复用

```text
ModelPreflightService：必跑输入的完整 strict/自相交守门；
AdaptSceneModelToTriangleMesh：保留 triangle UV/material attributes；
BuildTextureFillPartitionBenchmarkGrid：建立可重复 classification grid；
LegacyCpuGlobalDistanceBackend + GlobalTextureFillPartitionService：三维互补分区和 width sweep；
TransferTextureFillPartition：OBJ/3MF 表面 RGB 属性转移；
ComposeTextureFillPartitionDiagnostic：内存 RGBWSV 诊断合成；
TextureFillPartitionReport：分区、扫描和 full-closure 报告基础字段。
```

### 4.2 R4-05 需要补齐

1. 现有 width sweep unit 主要使用 generated mesh，缺少三个必跑真实输入的统一矩阵验证。
2. `ComposeTextureFillPartitionDiagnostic` 直接接受 `white/varnish/rgb`，没有独立、可报告的
   `profile_default/material_role` 解析结果。
3. 尚无 R4-05 汇总 report，无法一次审计 input identity、width 三点、材料解析和互补结果。
4. C/M/Y/K 尚无已标定的 RGBWSV 值注册表。R4-05 不得硬编码墨量；未注册角色必须输出
   稳定“不可用”原因，不得伪造通道。

## 5. 冻结实施设计

### 5.1 新增诊断矩阵服务

推荐新增 backend-neutral 诊断服务，放在 `src/slicer_core/diagnostics`，不放进 legacy 生产 slicer：

```text
TextureFillPartitionPositiveMatrixRequest
TextureFillPartitionPositiveMatrixResult
RunTextureFillPartitionPositiveMatrix(...)
```

服务顺序固定为：

```text
load config/model
-> ModelPreflight full PASS
-> AdaptSceneModelToTriangleMesh
-> build classification grid
-> discover effectiveMinimum/allTextureThreshold
-> evaluate minimum/intermediate/allTexture
-> texture transfer
-> resolve Model Fill material
-> in-memory diagnostic composer
-> invariant/report projection
```

任何步骤 blocked 时 fail-closed，后续合成不得继续。

### 5.2 宽度矩阵

对每个必跑输入至少运行：

```text
minimumRequestedMm = 0.10
effectiveMinimumMm = max(0.10, 2 * classificationResolutionMm)
intermediateMm = 将 (effectiveMinimumMm + allTextureThresholdMm) / 2 按 0.01mm 步进量化
allTextureMm = allTextureThresholdMm
```

实现必须复用现有 `EvaluateWidthSweep`，设置
`representativeIntermediateCount=1`；中点采用现有 `RoundToStep` 的最近步进规则，minimum
继续采用 `CeilToStep`，避免 R4-05 另写一套量化算法。

若薄壁使 `effectiveMinimumMm == allTextureThresholdMm`，允许去重为一个 sample，但报告必须记录
`deduplicated=true` 和原始请求三点，不得伪造中间宽度。

每个有效 sample 必须满足：

```text
texture ∩ modelFill = empty
texture ∪ modelFill = model
outsideModel(texture/modelFill) = 0
unassignedModel = 0
textureVoxels 单调非递减
modelFillVoxels 单调非递增
allTexture: modelFillVoxels=0, textureVoxels=modelVoxels
```

### 5.3 Model Fill 材料解析

不将 UI 文案或原始 role 字符串直接传入 composer。先产生显式解析 DTO：

```text
requestedMaterial: white | varnish | rgb | profile_default | material_role
requestedRole: 空或稳定 role id
available: true | false
resolvedMaterial: white | varnish | rgb | unavailable
resolvedChannels: R/G/B/W/V 生产值摘要，S 恒不属于 Model Fill
profileId: 实际使用的 Profile
reasonCode: 未配置/不支持时的稳定 code
```

必跑材料行：

| 请求 | 预期 |
|---|---|
| `white` | 只在 W 写入 `modelFill.value` |
| `varnish` | 只在 V 写入 `modelFill.value` |
| `rgb/custom` | 在 R/G/B 写入显式自定义值 |
| `profile_default` | 解析到 Profile 已登记的 white/varnish/rgb；未登记则 blocked |
| `material_role` | 解析已登记 role；未登记 C/M/Y/K 显式 unavailable |

C/M/Y/K 仅是 Profile role id。只有当工艺 Profile 显式提供现有 RGBWSV 映射时才进入正向合成；
否则本任务验证“可解释地不可用”，不将其写成 PASS 通道。

### 5.4 报告合同

新增 R4-05 汇总 schema，冻结为：

```text
slicesoft.texture_fill_positive_matrix.12e_08c_r4.1
```

必需字段：

```text
diagnosticOnly=true
productionOutputWritten=false
requiredRepairPassCount=0
input.caseId/modelPath/sourceHash/resourceHash/preflightStatus
grid/classificationResolutionMm
widthSamples.requested/effective/allTexture/stats/invariantPass
materialCases.requestedMaterial/requestedRole/available/resolvedMaterial/resolvedChannels/profileId/reasonCode
summary.complementPass/monotonicPass/endpointPass/materialResolutionPass
issues[]
```

report 的稳定 projection 必须写 golden；时间和内存数值不进 golden。

## 6. 计划代码落点

```text
src/slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.{h,cpp}
src/slicer_core/materials/process_profile/ModelFillMaterialResolver.{h,cpp}
tests/unit/texture_fill_partition_positive_matrix/main.cpp
tests/unit/model_fill_material_resolver/main.cpp
tests/golden/expected/12e_r4_clean_positive_matrix_projection.json
CMakeLists.txt
```

若实施时发现现有 `MaterialProcessProfile` 无法表达已标定 role，只允许新增一个独立、可选、
无默认墨量的 role registry DTO。不得在 composer 内硬编码 C/M/Y/K 值，不得顺带改生产 writer。

## 7. 验证命令

实施 R4-05 时至少运行：

```powershell
cmake --build build --config Debug --target texture_fill_partition_positive_matrix_unit_tests model_fill_material_resolver_unit_tests

ctest --test-dir build -C Debug -R "(texture_fill_partition_positive_matrix|model_fill_material_resolver|texture_fill_partition_width_sweep|texture_fill_partition_diagnostic_composer)" --output-on-failure

cmake --build build --config Debug --target slicer_cli slicer_debug_ui
.\scripts\run_ci_quick.ps1
git diff --check
git status --short
```

真实模型矩阵必须使用上述三个必跑输入。本地扩展输入缺失时不得导致 CI 失败；必跑输入缺失
则 fail-closed。

## 8. 完成标准

```text
三个必跑模型 preflight PASS；
三点 width 互补/单调/终点全部 PASS；
white/varnish/rgb 合成只占用既有通道；
profile_default/material_role 成功解析或返回稳定不可用原因；
C/M/Y/K 没有新增协议通道或硬编码墨量；
report 显式记录 diagnosticOnly=true、productionOutputWritten=false、requiredRepairPassCount=0；
没有生成生产 package，没有修改用户模型资产；
定向测试和既有 width/composer 回归 PASS。
```

## 9. R4-05 之后的准备度

| 任务 | 准备度 | 当前结论 |
|---|---|---|
| R4-05 | **READY FOR DEVELOPMENT** | 本文已冻结输入、代码落点、矩阵、报告、验证和停止条件 |
| R4-06 | CONTRACT READY / EXTERNAL INPUT BLOCKED | 接收和属性审计合同已有，仍缺 `nai_you/aishen/meigui` 三个外部修复资产 |
| R4-07 | DEPENDENCY PREPARED / WAIT R4-06 | 四 case Release/global/legacy 验证已定义，不可在 R4-06 前执行 |
| R4-08 | DEPENDENCY PREPARED / WAIT R4-07 | 只刷新 08D GO/NO-GO，不实现 production adapter |

因此，R4-05 已具备开发条件。R4-06 之后的阻断不是普通模型或基础文档不足，而是三个 required
外部修复资产尚未到位。

## 10. 停止条件

```text
需要改 TIFF/manifest/RIP 合同：停止；
需要将 global 诊断结果写生产 package：停止；
需要放宽 strict 或自动 fallback legacy：停止；
需要硬编码未标定 C/M/Y/K 墨量：停止；
必跑模型不再满足完整预检 PASS：停止并更新资产清单；
任务需要提交或修改用户未跟踪模型：停止。
```
