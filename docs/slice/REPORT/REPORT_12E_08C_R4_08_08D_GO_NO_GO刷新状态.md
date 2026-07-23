# REPORT_12E-08C-R4-08 08D GO/NO-GO 刷新状态

> 文档状态：R4-08 HISTORICAL EXECUTION COMPLETE / ORIGINAL DECISION BLOCKED / GATE AMENDED
> 原始日期：2026-07-22
> 准入规则修改时间：2026-07-23 11:32 +08:00
> 后续决策：`../DOC/DOC_DECISION_12E_08C_R4_08_R1_受限生产候选准入规则.md`
> 基线提交：`c2c7f73`
> 生产结论：12E-08D NOT READY / NO-GO

## 1. 结论

R4-08 已按当前可取得证据完成一次正式决策刷新。R4-01..05 软件和正向链通过，R4-06 development intake
为 `2/2 admitted`，R4-07 development four-case 为 `4/4 PASS`，legacy TIFF invariant 与 RIP strict 通过。

当前不能进入 12E-08D：爱神、玫瑰、梯田 required family 仍为 `0/3`，最终真实族 four-case 尚未执行，
生产 Release 预算未冻结，Quick CI 在既有 `material_process_top2` golden 上失败，且没有 08D production
path 明确授权。

2026-07-23 用户接受“两独立 strict/admitted 真实模型族”的受限生产候选规则。该修订使
xiao_ma/yecan 可以进入 R4-07-R1 候选验证，但不把本报告的原始 `BLOCKED` 追溯改写为 `GO`。新的
GO/NO-GO 必须由 R4-08-R2 在预算、Quick CI 和独立授权闭环后重新输出。

依据状态定义，本次 R4-08 为 `BLOCKED`，不是 `GO` 或 `CONDITIONAL_TECHNICAL_PASS`。任务执行已完成，
但生产准入没有完成。

## 2. Gate 证据矩阵

哈希均为 SHA-256。`output/benchmarks` 是本地忽略证据目录，不纳入 Git 提交。

| Gate | 证据路径 | SHA-256 | 状态 | Blocker |
|---|---|---|---|---|
| R4-01 ModelPreflight contract | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_01_ModelPreflightContract结果.md` | `cb1b30a52b1b0306cd4e7cd8fd1a1225eaa9ee4aa58f32f224454f45e38e0508` | PASS | 无 |
| R4-02 Two-stage service | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_02_TwoStagePreflightService结果.md` | `f079830f7976cf36a7b34706f5f12a202e1b16ca466ac64036f6f88f70801cc1` | PASS | 无 |
| R4-03 Mode admission | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_03_ModeAdmission与PipelineGate结果.md` | `8da99e1c96e8c19d9a40e29dd738018d8766d7e942c0711e983c029b1d826f5b` | PASS | 无 |
| R4-04 Qt preflight | `docs/slice/DOC/DOC_EXEC_12E_08C_R4_04_QtPreflightUI结果.md` | `819abd935b0b21ec9d00f18361156e629c1edf26ca8ac377c9cf6c917a785dda` | PASS | 无 |
| R4-05 clean positive matrix | `output/benchmarks/12e_08c_r4_07_development_gate/positive_matrix/summary.json` | `4e4f2d37d77a28e1ba8a11089bcf0b81d48a89309989517638a217d623fd9f1d` | PASS | 无 |
| R4-06 required-family intake | `output/benchmarks/12e_08c_r4_06_repaired_asset_intake/required_family_matrix.json` | `112749e8188746d3c4a4d47814fc829a3e0cc942f6296a45d7b998df784ef62a` | FAIL / 0/3 | 三个真实模型族均无 admitted 候选 |
| R4-06 development intake | `output/benchmarks/12e_08c_r4_06_repaired_asset_intake/development_gate_matrix.json` | `3447e22f00662deed552f5dd2b8616ab6a0bb3b546df1d3f787a220b17e650a5` | PASS / 2/2 | 仅开发准入，不计 required family |
| R4-07 development four-case | `output/benchmarks/12e_08c_r4_07_development_gate/four_case_development_summary.json` | `469cf83edcfa019a8c0489410975e89aaf9f4f83dfbbc5f4ca680f7f6807a3e6` | PASS / 4/4 | 仅 development matrix |
| R4-07 final real-family four-case | 尚无证据 | `not_available` | BLOCKED | required family 0/3 |
| Release production budget | 尚无冻结文件 | `not_available` | BLOCKED | development 测量不得替代生产预算 |
| Legacy TIFF invariant | R4-07 development summary | 同上 | PASS | 无 |
| RIP strict | R4-07 development summary | 同上 | PASS | 无 |
| Quick CI | `scripts/run_golden_tests.ps1` | `ed386db7fa834188c9d2cdd2019b257c5c7bc576b86dc0e89222a18ea81a8cb1` | FAIL | `material_process_top2 widthPx expected=48 actual=226` |
| Protocol invariants | R4-07 result | `ff4fc3a5dcfaaeea884466cb9eab5f80cf086c430a89a3362ebf79e9d25fddb6` | PASS | 未修改协议 |
| Explicit production authorization | 无授权记录 | `not_available` | BLOCKED | 用户本次授权执行 R4-08，不等于授权 08D production path |

## 3. GO 条件判定

| 条件 | 当前值 | 结论 |
|---|---|---|
| `requiredFamilyAdmitted=3/3` | `0/3` | FAIL |
| final `fourCaseStrictPass=4/4` | 未执行 | BLOCKED |
| final partition/texture/raster/full closure `4/4` | 未执行 | BLOCKED |
| `releaseBudgetFrozen=true` | `false` | FAIL |
| `releaseBudgetPass=true` | 未评估 | BLOCKED |
| `legacyTiffInvariantPass=true` | `true` | PASS |
| `ripStrictPass=true` | `true` | PASS |
| `quickCiPass=true` | `false` | FAIL |
| `protocolChanged=false` | `true` | PASS |
| `globalProductionOutputWritten=false` | `true` | PASS |
| `explicitUserAuthorization=true` | `false` | FAIL |

## 4. Quick CI 实测

实际运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

结果：退出码 `1`，总耗时约 `437.9 s`。Debug 全量构建、support shape、quick regression、schema 和
support shape smoke 已执行；流程在 golden tests 的 `material_process_top2` 用例停止：

```text
material_process_top2 widthPx expected=48 actual=226
```

本任务不擅自刷新 golden。该差异必须作为独立 baseline 决策处理。

## 5. 当前阻断项

```text
B-R4-08-01：required family admission 0/3；
B-R4-08-02：最终真实族 four-case 缺失；
B-R4-08-03：Release production budget 未冻结；
B-R4-08-04：Quick CI golden baseline 失败；
B-R4-08-05：08D production path 未取得明确授权。
```

任一项未关闭时，12E-08D-01..04、12E-09B production Profile 和 global production TIFF 写包均不得启动。

## 6. 后续阶段准备度

| 后续任务 | 准备度 | 是否可执行 |
|---|---|---|
| required-family 候选外部修复/重建与 R4-06 复审 | 合同与工具完整 | 有候选后可执行 |
| R4-07 final real-family matrix | 脚本边界与验收项已准备 | 等待 family 3/3 |
| Quick CI baseline 专项 | 已定位稳定失败点 | 可单独立项，不在 R4-08 内刷新 golden |
| 12E-08D 双模式生产写包 | PRD/DEV/原子任务已准备 | 不可执行，等待 R4-08 GO |
| 12E-09A diagnostic UI | 09A-01..06 准备完整 | 可独立执行，不开放生产 |
| 12E-09B production Profile | 目标已准备 | 不可执行，等待 08D admission |
| 12E-10A preview | 基础准备完成 | 等待 09A-05 |
| 12E-10B/10C production evidence | 基础准备完成 | 等待 R4 final 与 08D |

当前下一项可执行开发任务是 `12E-09A-01 只读 diagnostic facade 与 UI DTO`。若继续 R4 生产准入路线，
则必须先提供 required family 修复/重建候选，不能通过开发 Gate 结果绕过。

## 8. 2026-07-23 准入规则修订

原“必须先完成爱神/玫瑰/梯田 3/3 才能开展生产候选验证”被部分取代：

```text
受限生产候选验证：至少两个独立 strict/admitted 真实模型族；
当前候选：xiao_ma_wu_yu_new + yecan；
复杂浮雕覆盖：aishen/meigui/titian 仍为 0/3；
production budget：仍未冻结；
Quick CI：仍未解决；
08D production path：仍未取得独立授权。
```

R4-07-R1 已于 2026-07-23 完成，两独立模型族与四用例候选证据 PASS。当前新状态为
`R4-07-R1 COMPLETE / R4-07-R2 PREP NEXT / 12E-08D BLOCKED`。本报告保留 2026-07-22 的原始证据和
哈希，R4-07-R1 新证据记录在独立结果文档中。

## 9. 安全边界

```text
legacy 继续为默认生产模式；
global_surface_shell 继续 diagnostic-only；
OpenVDB 默认关闭；
不写 global production package；
不修改 p0.rgbwsv.2；
不修改 channelOrder=R G B W S V；
不修改 bitDepth=8；
不修改 polarity=black_is_print；
不把 clean development model 计入 required family；
不把本次“开始 R4-08”解释为 production path 授权。
```
