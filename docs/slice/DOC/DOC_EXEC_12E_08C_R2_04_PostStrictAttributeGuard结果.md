# DOC_EXEC_12E-08C-R2-04 Post-Repair Strict 与 Attribute Guard 结果

> 文档状态：COMPLETE / NON-PRODUCTION
> 日期：2026-07-20
> 前置：12E-08C-R2-03 COMPLETE

## 1. 完成范围

本任务新增独立、只读的 `MeshRepairEvidenceValidator`，不执行隐式修复，也不接受 repair 阶段自报的成功
状态。validator 固定复核：

```text
operation id 与阶段顺序；
source triangle mapping 全覆盖与 duplicate retained provenance；
vertex mapping 全覆盖、canonical representative 与组件隔离；
generated triangle mapping、source id、boundary vertex 与 attribute policy；
retained material/UV 与 material resource；
完整 post-strict topology/robustness；
post geometry/attribute、operation、options canonical hash；
productionAllowed=false / productionOutputWritten=false。
```

任一 Gate 失败时 `candidateAccepted=false`，service 丢弃候选并返回原始网格；契约/属性/hash 破坏归类为
`repair_failed`，未完整 strict 或剩余拓扑 blocker 归类为 `manual_repair_required`。

## 2. 契约扩展

`MeshRepairOptions` 增加默认关闭的 `validatePostRepairEvidence`，并纳入 `optionsHash`。报告增加
`evidenceValidation`，记录各 Gate、稳定 blocker code、issues 和候选接受状态。Schema id 仍为
`slicesoft.mesh_repair.12e_08c.1`，RGBWSV 生产协议未改变。

## 3. Negative Fixture

单元测试覆盖：

```text
缺失 source mapping；
vertex source 重复覆盖；
generated output 越界；
generated material 破坏；
operation id 跳号；
operation hash 篡改；
duplicate removal 缺 retained source；
generated source id 与原 source 冲突；
sampled post-strict；
open boundary post-strict blocker 与 candidate discard。
```

## 4. 真实模型证据

`scripts/run_12e_08c_r2_04_post_strict_evidence.ps1` 对四个 required case 各运行两次并比较稳定投影：

| Case | 结果 | Validator | Candidate | Repeatability |
|---|---|---|---|---|
| `nai_you_new` | `manual_repair_required` | `blocked_incomplete_post_strict` | rejected | PASS |
| `aishen_fudiao` | `manual_repair_required` | `blocked_incomplete_post_strict` | rejected | PASS |
| `meigui_fudiao` | `manual_repair_required` | `blocked_incomplete_post_strict` | rejected | PASS |
| Texture2D 3MF | `strict_pass_no_repair` | `passed` | accepted | PASS |

三个真实 OBJ 的 source/vertex/generated/attribute Gate 均通过，但完整 post-strict 因 sampled
self-intersection evidence 阻断；该结果没有把 sampled 证据伪装成 strict PASS。

证据摘要：
`output/benchmarks/12e_08c_r2_04_post_strict/post_strict_summary.json`。

## 5. 已运行验证

```powershell
cmake --build build --config Debug --target mesh_repair_evidence_validator_unit_tests mesh_repair_cleanup_unit_tests mesh_repair_contract_unit_tests mesh_repair_preflight
ctest --test-dir build -C Debug -R "mesh_repair_(r2_04|r2_03|r2_02|evidence_validator|cleanup|contract|preflight)" --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_12e_08c_r2_04_post_strict_evidence.ps1 -BuildDir build -Config Debug
```

验证结果：

```text
Debug 全量构建：PASS；
Debug CTest：32/32 PASS；
R2-04 定向 CTest：9/9 PASS；
Qt --self-test：startup、experimental-report-summary PASS；
真实模型证据：4/4 case、4/4 repeatability PASS；
run_ci_quick.ps1：在既有 golden 基线 material_process_top2 widthPx expected=48 actual=226 处失败。
```

quick CI 的 width baseline 与本任务 mesh repair DTO/validator 无关；本任务没有修改切片网格尺寸或 legacy
Profile。失败被如实保留，不将 quick CI 记录为通过。

## 6. 安全与阶段结论

```text
修复默认关闭；
OpenVDB optional/OFF；
legacy、Qt、TIFF writer 未接入；
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print 不变；
所有 R2-04 输出仍为 non-production；
12E-08D 继续 BLOCKED。
```

12E-08C-R2 已完成。下一原子任务为 R3-01 Non-Manifold Pattern Classifier；R3-01A 完整自相交证据仍需
等待 classifier 的模式证据。
