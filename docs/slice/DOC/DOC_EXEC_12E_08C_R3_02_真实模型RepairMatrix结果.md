# DOC_EXEC_12E-08C-R3-02 真实模型 Repair Matrix 结果

> 文档状态：COMPLETE / NON-PRODUCTION
> 日期：2026-07-21
> 分支：`feature/12e-08c-mesh-repair`

## 1. 任务结论

R3-02 已完成四个 required case 的 `strict_no_repair` 与 `conservative_repair` 矩阵。每条 lane 均执行两次，
完整自相交证据、状态、hash、non-manifold 分类、operation、属性和 evidence validator 投影保持稳定。

任务证据 4/4 完整，但 production Gate 0/4 通过。三个真实 OBJ 在 mutation 前被完整 confirmed/coplanar
self-intersection 证据 fail-fast；闭合 Texture2D 3MF 保持 no-op strict PASS，但在本专项中仍是
`non_production_only`。12E-08D 继续 BLOCKED。

## 2. 实现内容

新增以下非生产入口：

```text
mesh_repair_preflight --execute-r3-02；
mesh_repair_r3_02_cli_test；
tests/golden/expected/12e_mesh_repair_matrix_expectations.json；
scripts/run_12e_08c_r3_02_repair_matrix.ps1；
slicesoft.mesh_repair_matrix.12e_08c_r3_02.1 summary。
```

`--execute-r3-02` 只组合 R2 已完成的 cleanup、weld、winding、simple boundary、evidence validator，
以及 R3-01/R3-01A 的分类和完整相交分析。它没有新增自相交重建算法，也没有改变这些能力的默认关闭状态。

## 3. Matrix 结果

| case | strict | conservative | complete evidence | repair/ops | task evidence | production Gate |
|---|---|---|---|---:|---|---|
| `nai_you_new` | rejected | rejected | 236181 candidates / 8409 confirmed | false / 0 | complete_rejected | blocked |
| `aishen_fudiao` | rejected | rejected | 491365 / 19270 confirmed / 20 coplanar | false / 0 | complete_rejected | blocked |
| `meigui_fudiao` | rejected | rejected | 346104 / 5592 confirmed | false / 0 | complete_rejected | blocked |
| Texture2D 3MF | strict no-op PASS | strict no-op PASS | 8 / no intersection | false / 0 | complete_no_op_pass | non-production-only |

补充分类：

```text
aishen_fudiao：59 条 non-manifold edge 完整分类；
meigui_fudiao：10940 条 non-manifold edge 完整分类；
两者 allUniqueFanSplitsFeasible=false；
所有 rejected case 均未进入 evidence validator 或创建 candidate；
3MF evidence validator=passed，attribute preservation=passed，candidateAccepted=true。
```

## 4. 重复性与输出

矩阵脚本对每个 case/lane 执行两次，排除计时字段后比较稳定 projection：

```text
strict repeatability：4/4 PASS；
conservative repeatability：4/4 PASS；
complete self-intersection：4/4 complete；
budget/resource blocked：0；
productionOutputWritten：全部 false。
```

汇总文件：

```text
output/benchmarks/12e_08c_r3_02_repair_matrix/repair_matrix_summary.json
```

## 5. 已运行验证

TDD 先证明新增入口缺失：`mesh_repair_r3_02_cli_test` 初次运行因未知参数 `--execute-r3-02` 失败。
实现后已运行：

```powershell
ctest --test-dir build -C Debug -R "mesh_repair_(cleanup|preflight|r3_02)" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r3_02_repair_matrix.ps1 -BuildDir build -Config Debug
```

结果：定向 CTest 3/3 PASS；真实模型矩阵 PASS，耗时约 226.2 秒；完整 Debug build PASS；全量 Debug CTest
37/37 PASS；Qt `--self-test` 输出 `PASS startup`、`PASS experimental-report-summary`。

仓库当前 Quick CI 仍存在 R3-01A 已记录的独立 legacy golden blocker：`material_process_top2` 期望
`widthPx=48`、实际 `226`。R3-02 没有修改姿态、缩放、legacy writer 或该 golden，不将其记录为通过。

## 6. 安全边界

```text
repair 默认关闭；
confirmed/coplanar self-intersection mutation 前 fail-fast；
没有通用自相交重建或批量 fan split；
OpenVDB OFF；
legacy Profile 与 slicer_cli production path 未修改；
不写 TIFF、preview 或 production package；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 未修改。
```

## 7. 下一任务

下一允许原子任务为 `12E-08C-R3-03 Release Core 与 Legacy Regression`。该任务只允许形成非生产 Release
证据：三个 OBJ 的 global lane 必须记录 `skipped_due_topology`，不得绕过 blocker。专用准备入口为
`DOC_PREP_12E_08C_R3_03_ReleaseCore与LegacyRegression准备.md`。
