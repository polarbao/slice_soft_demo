# REPORT_12E-08C-R4-08-R2 08D GO/NO-GO 刷新状态

> 文档状态：COMPLETE / GO / 12E-08D AUTHORIZED
> 完成时间：2026-07-23
> 授权时间：2026-07-23 15:15:27 +08:00
> 基线提交：`a9a52b4`
> 准入规则：两独立 strict/admitted 真实模型族
> 生产结论：12E-08D-01 READY FOR DEVELOPMENT

## 1. Current State

R4-08-R2 已完成受限生产候选技术 Gate 刷新：

```text
候选身份：2 个独立真实模型族 PASS；
四用例材料闭环：4/4 PASS；
参考机器候选预算：2026-07-23.r1 / FROZEN PASS；
legacy TIFF invariant：PASS；
RIP Reader strict：PASS；
Quick CI：PASS；
RGBWSV 协议边界：PASS / unchanged；
复杂浮雕覆盖：aishen/meigui/titian 仍为 0/3 披露缺口；
08D 独立用户授权：PASS。
```

全部技术 Gate 已通过。用户于 2026-07-23 15:15:27 +08:00 明确授权开展 12E-08D 开发，原
`CONDITIONAL_TECHNICAL_PASS` 现转为 `GO`。授权范围是按 08D-01..04 原子任务实施双模式生产写包，
不等于允许跳过 admission、默认替换 Legacy 或放宽协议红线。

## 2. Target State

12E-08D 的目标状态仍是：

```text
slicePipeline.mode=legacy | global_surface_shell；
legacy 保持默认；
global_surface_shell 只接受 strict/admitted 输入；
两种生产成功路径共用现有 RGBWSV TIFF/package/RIP writer；
global 失败时不静默回退 legacy；
OpenVDB 保持 optional/OFF；
任何生产成功都必须输出 p0.rgbwsv.2 TIFF 包。
```

该目标尚未实现。本报告只完成启动 Gate 决策，不实现 Router、adapter 或生产 writer 接入。

## 3. Historical State

2026-07-22 的 R4-08 原始执行基于“爱神/玫瑰/梯田 3/3”规则，且当时预算未冻结、Quick CI
Golden 失败，因此输出 `BLOCKED`。

2026-07-23 用户批准 R4-08-R1 规则修订：允许至少两个独立 strict/admitted 真实模型族进入受限生产
候选验证；复杂浮雕 `0/3` 改为必须披露的覆盖缺口，不再是受限候选的硬启动条件。随后 R4-07-R1、
R4-07-R2 和 Quick-CI-R1 已分别闭环。

原报告继续作为历史证据保留，不追溯改写：

`REPORT_12E_08C_R4_08_08D_GO_NO_GO刷新状态.md`。

## 4. Authorization Record

用户明确指令：

```text
我明确授权给你可以进行12E-08D的开发任务
```

授权已登记，`explicitUserAuthorization=true`，当前没有 08D-01 启动确认缺口。08D-01..04 仍必须
逐任务实现、验证和提交；任一任务出现技术 Gate 失败时必须停止，不能用本授权覆盖失败证据。

## 5. 证据与 SHA-256

本地 `output/benchmarks` 证据不提交 Git，以下哈希固定本次读取身份：

| Gate | 证据路径 | SHA-256 | 状态 |
|---|---|---|---|
| R4-06 candidate intake | `output/benchmarks/12e_08c_r4_06_repaired_asset_intake/development_gate_matrix.json` | `b28f44677fa128e720bb4604749ac1888056e34891d87f2e2ea5575af2e21808` | PASS |
| R4-07-R1 restricted candidate | `output/benchmarks/12e_08c_r4_07_restricted_candidate/restricted_candidate_summary.json` | `a685c432fd7045b3bf08ae3a57aab4bd69c68c0d6ada54e75af6c0fc8677ad3f` | PASS |
| R4-07-R1 result | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_07_R1_受限生产候选验证结果.md` | `d1899ab1781bd785ae47dbb6ae1438ddc9761b5c50587dadc3d73c00d26b8032` | PASS |
| R4-07-R2 refreshed budget | `output/benchmarks/12e_08c_r4_07_r2_candidate_budget/r4_08_r2_refresh/candidate_budget_summary.json` | `0d8de2bfd6fe78e074ba28f07d30e7cd497e44d90e66c7e992bd4bb8a8f901b9` | FROZEN PASS |
| Candidate budget policy | `tests/golden/expected/12e_r4_07_r2_candidate_budget_policy.json` | `b64981fe3a46fcf458955fb0a67aca28d76bf69c6815cf1dac282b9252809f80` | PASS |
| Quick-CI-R1 result | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_Quick_CI_R1_GoldenBaseline收口结果.md` | `7ab7c727cdd8d853e7a3dbdc19a981cd35ef29c0c1fd55e9a151e5b0a24d626e` | PASS |
| R4-08-R1 decision | `docs/slice/DOC/DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md` | `4102aee05fa46ed5b5ddd89e9baff19ca45ac9a0195ec52a6de2a2487b4ecf6c` | ACCEPTED |

## 6. 候选身份

| 模型族 | 模型 | sourceHash | strict/intake |
|---|---|---|---|
| `xiao_ma_wu_yu_new` | `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | `4f2012e7d584c7d8f4e3a4467d0af112216f93c222046f61a987880af8820ddc` | admitted |
| `yecan` | `model/obj/yecan/3.obj` | `a3a421005112292a71f49bed5734ce186c2b97a1379aa50e6df8be1a6914363d` | admitted |

两个候选分别来自不同真实模型族，均包含可审计 UV/材质/纹理资源。3MF Texture2D 样例只作为控制组，
不计入两个真实模型族。

## 7. 四用例与材料闭环

R4-07-R1 结果：

```text
candidate identity=2/2 PASS；
fourCase=4/4 PASS；
minimum/intermediate/allTexture 覆盖 PASS；
partition/texture transfer/raster/full material closure PASS；
attribute preservation PASS；
diagnosticOnly=true；
productionOutputWritten=false。
```

复杂浮雕爱神、玫瑰、梯田继续是 `0/3`。每个失败模型仍必须 fresh preflight 并 fail-closed，不能继承
xiao_ma/yecan 的候选 PASS。

## 8. 候选预算刷新

执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run_12e_08c_r4_07_r2_candidate_budget.ps1 `
  -BuildDir build `
  -Config Release `
  -MeasurementSummaryPath output/benchmarks/12e_08c_r4_07_r2_candidate_budget/measurements/four_case_development_summary.json `
  -OutputRoot output/benchmarks/12e_08c_r4_07_r2_candidate_budget/r4_08_r2_refresh
```

结果：

```text
environment identity=PASS；
MSVC 19.51.36248.0 / x64 / Release；
USE_OPENVDB=false；
backend=legacy_cpu_global_distance；
policyVersion=2026-07-23.r1；
budget.status=frozen_pass；
cases=4/4 PASS；
productionAdmission=not_evaluated；
productionOutputWritten=false。
```

| Case | 样本 | median core ms | max core ms | peak working set |
|---|---:|---:|---:|---:|
| `development_xiao_ma_minimum` | 5 | 328.1570 | 331.5887 | 26,632,192 B |
| `development_xiao_ma_all_texture` | 5 | 402.9719 | 433.5741 | 27,348,992 B |
| `development_yecan_intermediate` | 5 | 473.2710 | 479.5123 | 31,457,280 B |
| `texture2d_3mf_control` | 5 | 16.7596 | 17.3148 | 7,323,648 B |

预算只表示当前参考机器上的工程候选 Gate，不是产品 SLA。刷新摘要中的
`quick_ci_baseline_unresolved` 和 `nextTask=Quick-CI-R1` 是 R4-07-R2 runner 的历史字段；当前状态由
本报告结合已提交的 Quick-CI-R1 PASS 证据重新判定。

## 9. Quick CI、Legacy 与协议

Quick-CI-R1 已把真实用户 Profile 与小型确定性 Golden Fixture 解耦。连续两轮 Golden 和完整
`scripts/run_ci_quick.ps1` 均 PASS，完整 Quick CI 耗时约 `366.6 s`。

R4-07-R1 同时确认：

```text
legacy repair-disabled TIFF invariant=PASS；
RIP Reader strict=PASS；
p0.rgbwsv.2=unchanged；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
legacy remains default。
```

## 10. GO/NO-GO 矩阵

| Gate | 结果 |
|---|---|
| 两个独立真实模型族 | PASS |
| 每族至少一个 strict/intake admitted | PASS |
| 四用例与完整材料闭环 | PASS |
| 属性保持 | PASS |
| 候选预算冻结并通过 | PASS |
| legacy TIFF invariant | PASS |
| RIP strict | PASS |
| Quick CI | PASS |
| 协议保持不变 | PASS |
| global production output 未提前写入 | PASS |
| 复杂浮雕覆盖 | GAP DISCLOSED / 非硬启动条件 |
| 08D 独立用户授权 | PASS / 2026-07-23 15:15:27 +08:00 |

决策：

```text
technicalGatePass=true
explicitUserAuthorization=true
decision=GO
productionAdmission=not_evaluated
12E-08D-01=READY
remainingBlockers=[]
```

## 11. 后续准备度

| 后续任务 | 准备度 | 当前是否可执行 |
|---|---|---|
| 12E-08D-01 | 技术设计、原子任务、回归矩阵完整 | 是，已明确授权 |
| 12E-08D-02..04 | 技术设计和依赖完整 | 等待前序原子任务分别 PASS |
| 12E-09A-02..06 diagnostic UI | 已准备 | 可独立执行，不开放生产 |
| 12E-09B production Profile | 目标已准备 | 否，等待 08D admission |
| 12E-10A preview | 基础准备完成 | 等待对应 09A 任务 |
| 复杂浮雕修复/重建 | 仍需独立专项 | 可继续准备，不影响受限候选结论 |

## 12. 安全边界

```text
仅按 08D-01..04 原子任务顺序实施；
不把授权本身当成 productionAdmission=passed；
不把候选预算写成产品 SLA；
不把复杂浮雕失败资产改判为生产可用；
不修改 production writer；
不修改协议、通道、位深或极性；
不默认启用 OpenVDB；
不允许 global -> legacy 静默回退。
```
