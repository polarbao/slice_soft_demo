# DOC_PREP_12E-08C-R4-06 Repaired Asset Intake 准备

> 文档状态：IMPLEMENTED / DEVELOPMENT 2/2 / REAL FAMILY INPUT 0/3 BLOCKED
> 日期：2026-07-22
> 前置任务：R4-01..05 COMPLETE
> 生产边界：只接收和审计修复资产，不直接写生产 TIFF/package

## 1. 任务目标

R4-06 接收 `aishen/meigui/titian` 三个 required 真实模型族的候选，证明候选身份、几何、姿态、UV、
材质和纹理来源可追溯，并执行完整自相交与 post-strict 审计。候选可以是 strict PASS 原始资产、外部修复
资产或独立审计重建资产。

R4-06 不负责通用复杂自相交重建。没有真实 family PASS 输入时最终 Gate 保持 blocked，不使用跨族正常
模型伪造 required family PASS。跨族 clean 模型可以按独立 `development_model_pool` 进入开发 Gate。

## 2. Required family 身份

| Family ID | 候选目录 | 当前结论 |
|---|---|---|
| `required_aishen_family` | `model/obj/aishen_fudiao/*.obj` | 5/5 confirmed self-intersection，0 PASS |
| `required_meigui_family` | `model/obj/meigui_fudiao/*.obj` | 3/3 confirmed self-intersection，0 PASS |
| `required_titian_family` | `model/obj/titian_fudiao/*.obj` | 1/1 confirmed self-intersection，0 PASS |

候选必须保留对应 Family ID 和来源资产 hash，不允许改用 `xiao_ma/yecan` 等跨族 clean 模型替代。
`nai_you_new/MF_nai_you.obj` 保留为历史负向回归，但不再计入 required family。替代规则见
`DOC_DECISION_12E_08C_R4_06_真实模型族准入替代规则.md`。

## 3. Clean 模型的边界

`REPORT_12E_08C_R4_模型资产预检清单.md` 中“可直接进入”的 7 个 OBJ 继续作为：

```text
intake 服务的正向/负向控制；
属性 hash 和 repeatability 算法回归；
R4-05 已完成的 width/material 正向证据。
```

它们本身无需修复且不属于 required family，因此不得计入 `requiredFamilyPassCount`，也不得解除最终
required-family/R4-08/08D blocker；其中通过 development intake 的模型可解锁 R4-07 开发。
`yecan/4.obj` 仍是未跟踪用户资产，不进入 CI 或提交。

## 4. Intake Manifest 合同

计划 schema：

```text
slicesoft.repaired_asset_intake.12e_08c_r4.1
```

每个 required family candidate 必须提供：

```text
familyId/candidateId/candidateKind；
original.path/sourceHash/resourceHash；
candidate.path/sourceHash/resourceHash；
provenance.provider/tool/toolVersion/operationSummary/timestamp/operator；
coordinate.unit/handedness/upAxis/transform；
geometry.triangleCount/componentCount/bbox/volume；
topology.boundary/nonManifold/duplicate/oppositeDuplicate/winding/degenerate；
selfIntersection.auditComplete/candidatePairs/confirmedPairs/coplanarPairs/auditHash；
attributes.materialCount/textureCount/uvCoverage/attributeHash/resourceDiff；
delta.bbox/triangle/component/material/texture/uv；
postStrict.status/issues；
repeatability.runCount/hashMatch；
admission.status/reasonCodes。
```

所有 hash 使用内容 hash，不以文件名或修改时间代替。原始资产只读，修复资产不得覆盖原文件。

## 5. 准入规则

单 candidate 只有同时满足下列条件才能 `admitted=true`：

```text
family 身份与清单一致；
`strict_pass_original` 具有来源 hash；修复/重建 candidate 的 provenance 完整；
文件和所有外部资源可读取；
最终 transform 后单位、姿态和尺寸在批准容差内；
材质槽、UV 和纹理引用未丢失，或差异有显式批准；
完整自相交审计 complete；
confirmedIntersectionPairs=0；
coplanarOverlapPairs=0；
post-strict 无 boundary/non-manifold/duplicate/opposite-duplicate/winding blocker；
重复两次审计的 geometry/attribute/audit hash 一致。
```

任一字段缺失、审计 sampled/incomplete、资源丢失、hash 不匹配或属性差异未经批准时 fail-closed。
不提供 warn-and-attempt 或 silent legacy fallback。

## 6. 计划代码边界

R4-06 开发时推荐新增：

```text
src/slicer_core/preflight/RepairedAssetIntakeTypes.{h,cpp}
src/slicer_core/preflight/RepairedAssetIntakeService.{h,cpp}
src/slicer_core/diagnostics/RepairedAssetIntakeReport.{h,cpp}
apps/repaired_asset_intake/Main.cpp
tests/unit/repaired_asset_intake/
tests/golden/expected/12e_r4_repaired_asset_intake_projection.json
scripts/run_12e_08c_r4_06_repaired_asset_intake.ps1
```

优先复用 `ModelPreflightService`、完整自相交分析、SceneModel adapter、内容 hash 和 ValidationIssue；不把
intake 逻辑塞回 importer、legacy slicer 或 Qt UI。

## 7. 验证矩阵

准备阶段冻结以下 case：

| Case | 输入 | 预期 |
|---|---|---|
| clean control | 已跟踪 `xiao_ma` 主 OBJ | rejected as required-family / clean control PASS |
| missing provenance | generated manifest | BLOCKED / stable code |
| original hash mismatch | generated manifest | BLOCKED / stable code |
| missing texture | generated fixture | BLOCKED / stable code |
| incomplete audit | generated fixture | BLOCKED / stable code |
| each required family candidate | 原始 PASS/外部修复/独立重建 | post-strict + attribute + repeatability PASS |

真实 required family PASS 缺失时，单元合同测试、服务开发和 R4-07 Development Gate 可以完成，但
R4-06 真实矩阵状态必须保持 `REAL FAMILY INPUT BLOCKED`，不得把 R4-07 final 或 R4-08 标记为可执行。

## 8. R4-07 与 R4-08 准备度

| 任务 | 准备度 | 启动条件 |
|---|---|---|
| R4-06 | IMPLEMENTATION COMPLETE / DEVELOPMENT 2/2 / REAL FAMILY 0/3 | final Gate 仍需三个 family 各一个 candidate admitted |
| R4-07 development | COMPLETE | 至少一个 development_model_pool candidate admitted |
| R4-07 final | WAIT REAL FAMILY 3/3 | 三个 family 全部 admitted，再运行真实族 Release/global/legacy 矩阵 |
| R4-08 | DEPENDENCY PREPARED / WAIT R4-07 FINAL | 真实族四 case、预算、legacy TIFF/RIP 和 Quick CI 证据齐全 |

R4-07 的第四个 case 继续使用闭合 Texture2D 3MF。R4-08 只刷新 12E-08D GO/NO-GO，不在决策任务中
偷跑 production adapter。

## 9. 当前停止条件

```text
没有 development_model_pool admitted candidate：不启动 R4-07 development；
三个 required family 没有 admitted candidate：不启动 R4-07 final/R4-08；
要求用跨族 clean 模型替代 required family：停止；
要求放宽 strict 或接受 sampled/incomplete 审计：停止；
要求覆盖原模型或提交用户未跟踪资产：停止；
需要改变 TIFF/RIP 协议或写 global production package：停止；
需要项目内实现通用复杂重建：转独立 R5 决策，不扩大 R4-06。
```
