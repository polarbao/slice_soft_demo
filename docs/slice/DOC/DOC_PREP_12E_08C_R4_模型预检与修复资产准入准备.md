# DOC_PREP_12E-08C-R4 模型预检与修复资产准入准备

> 文档状态：READY FOR R4-01
> 日期：2026-07-21
> 前置：R3-04 COMPLETE / NO-GO

## 1. 启动目标

在不放宽 strict、不改变 legacy 默认输出、不引入新第三方库的前提下，建立模型导入预检、模式相关准入、
UI 阻断提示、正常模型正向矩阵和修复资产审计入口。

## 2. 已具备依赖

```text
SceneModel/OBJ/3MF importer；
GeometryRobustness/TopologyDiagnostics；
完整自相交 AABB BVH 证据；
MeshRepairEligibilityPolicy 与 canonical hash；
post-strict/attribute evidence validator；
global partition/texture/raster/full closure diagnostic chain；
Qt 一键 legacy/global candidate 入口；
R3-03 Release evidence schema。
```

## 3. 待新增契约

```text
ModelPreflightRequest/Result/Issue DTO；
ModeAdmissionResult：legacy/global 分别给出 pass/warn/blocked；
preflight cache key：source/resource/transform/options/algorithm version；
slicesoft.model_preflight.12e_08c_r4.1 report；
稳定错误码 E_12E_PREFLIGHT_*；
UI 状态：待检测/检测中/通过/警告/阻断/已过期；
RepairAssetAdmission record：原 required identity、新 source hash、来源、审计人/工具、属性 diff。
```

## 4. 停止条件

```text
预检结果缺失或 stale：不得启动当前动作；
global strict blocker：不得进入 global partition 或 writer；
完整自相交预算不足：不得把 sampled 结果当 PASS；
修复资产 UV/材质/纹理 provenance 不完整：不得登记为 required PASS；
正常 fixture PASS：只证明功能链可运行，不解除真实模型 Gate；
任何 global 失败：不得 silent fallback legacy。
```

## 5. 首个原子任务

`12E-08C-R4-01` 只实现/冻结 DTO、错误码、报告 schema、缓存键和模式 Gate 表；不接 UI，不运行修复，
不写生产 TIFF。

## 6. 计划验证

```text
R4-01：DTO/schema/error-code unit/golden；
R4-02：fast/full preflight、cache invalidation、transform hash；
R4-03：legacy/global admission matrix 与 no-fallback；
R4-04：Qt self-test、UI smoke、一键按钮阻断；
R4-05：clean OBJ/3MF minimum/intermediate/allTexture 正向矩阵；
R4-06：required repaired asset provenance/post-strict；
R4-07：Release core、峰值内存、legacy TIFF invariant、RIP strict；
R4-08：GO/NO-GO 文档审计。
```

## 7. 当前准备结论

R4-01 至 R4-05 的代码和验证输入已可准备并执行；R4-06 之后依赖三个 required OBJ 的外部修复版本。
因此 R4 可启动，但不能预先承诺 R4 最终 GO。

## 8. 真实模型目录预检基线

2026-07-21 已对 `model` 下 15 个 OBJ/3MF 完成统一 Release 完整自相交审计：

```text
7 个 strict PASS，可无需重建进入 R4 后续模块；
1 个无自相交但存在非流形/反向重复面，需人工修复；
7 个存在 confirmed/coplanar self-intersection，需重建；
0 个解析或审计预算阻断；
15 个模型 missingTextureResources=0。
```

R4-05 主 OBJ 使用 `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj`，独立复核使用
`model/obj/yecan/3.obj`；`model` 目录暂无 strict PASS 3MF，因此 3MF 正向链继续使用
`samples/models/3mf/texture2d_checker_cube.3mf`。完整表见
`../REPORT/REPORT_12E_08C_R4_模型资产预检清单.md`。

R4-01 的代码落点、合同字段、golden 和验证命令已进一步冻结在
`DOC_PREP_12E_08C_R4_01_ModelPreflightContract准备.md`，当前无需再次补充准备文档即可开始该原子任务。
