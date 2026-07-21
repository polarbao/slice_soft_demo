# DOC_SCHEMA_12E Mesh Repair Matrix

> 文档状态：IMPLEMENTED / NON-PRODUCTION
> Schema：`slicesoft.mesh_repair_matrix.12e_08c_r3_02.1`
> 日期：2026-07-21

## 1. 目的

本 Schema 用于汇总 12E-08C-R3-02 四个 required case 在 `strict_no_repair` 与
`conservative_repair` 两条 lane 下的确定性证据。它只回答“矩阵证据是否完整”和“生产 Gate 是否通过”，
不替代单 case 的 `slicesoft.mesh_repair.12e_08c.1` 报告，也不授予 production admission。

建议输出路径：

```text
output/benchmarks/12e_08c_r3_02_repair_matrix/repair_matrix_summary.json
```

## 2. 根结构

```json
{
  "schema": "slicesoft.mesh_repair_matrix.12e_08c_r3_02.1",
  "generatedAt": "2026-07-21T00:00:00+08:00",
  "stage": "12E-08C-R3-02",
  "build": {},
  "contract": {},
  "cases": [],
  "result": {}
}
```

`generatedAt` 只用于追踪本次执行，不参与稳定投影比较。

## 3. Build 与 Contract

```json
{
  "build": {
    "buildType": "Debug",
    "buildDir": "build",
    "useOpenVdb": false
  },
  "contract": {
    "lanes": ["strict_no_repair", "conservative_repair"],
    "completeSelfIntersectionRequired": true,
    "conservativeOperations": [],
    "confirmedIntersectionFailFast": true,
    "repeatRunsPerLane": 2,
    "productionOutputWritten": false
  }
}
```

约束：

```text
useOpenVdb=false；
completeSelfIntersectionRequired=true；
confirmed/coplanar evidence 必须在 mutation 前 fail-fast；
conservativeOperations 只能列 R2 已有操作；
productionOutputWritten 始终为 false。
```

## 4. Case 结构

每个 case 至少包含：

```json
{
  "caseId": "nai_you_new",
  "strictStatus": "rejected_self_intersection",
  "conservativeStatus": "rejected_self_intersection",
  "analysisStatus": "confirmed_intersection",
  "candidatePairCount": 0,
  "confirmedIntersectionPairs": 0,
  "coplanarOverlapPairs": 0,
  "touchingOnlyPairs": 0,
  "candidatePairHash": null,
  "nonManifoldStatus": "not_present",
  "nonManifoldEdgeCount": 0,
  "allUniqueFanSplitsFeasible": false,
  "repairAttempted": false,
  "operationCount": 0,
  "evidenceValidationStatus": "not_evaluated",
  "candidateAccepted": false,
  "attributePreservationPass": false,
  "taskEvidenceStatus": "complete_rejected",
  "productionGateStatus": "blocked_confirmed_self_intersection",
  "strictStableSha256": null,
  "conservativeStableSha256": null,
  "strictRepeatability": "passed",
  "conservativeRepeatability": "passed",
  "productionOutputWritten": false
}
```

稳定 SHA-256 来自排除计时字段后的单 case report projection。两个 lane 分开计算，不能跨 lane 比较，因为
repair options、mode 和 options hash 按设计不同。

## 5. 双状态语义

`taskEvidenceStatus` 固定为：

```text
complete_rejected
complete_manual
complete_no_op_pass
complete_repaired_pass
incomplete
```

`productionGateStatus` 固定为：

```text
blocked_confirmed_self_intersection
blocked_manual_repair_required
blocked_incomplete_evidence
non_production_only
eligible_for_later_admission
```

两者不得合并。`complete_rejected` 表示 R3-02 已诚实完成该 case 的证据，不表示生产通过；
`non_production_only` 表示当前专项中的 strict PASS 仍需后续 R3-03/R3-04 才能形成 production 决策。

## 6. Result 结构

```json
{
  "evidenceStatus": "complete",
  "caseCount": 4,
  "taskEvidenceCompleteCases": 4,
  "confirmedIntersectionCases": 3,
  "noOpStrictPassCases": 1,
  "productionGatePassedCases": 0,
  "nextTask": "12E-08C-R3-03",
  "nextTaskReadiness": "ready_non_production_release_evidence",
  "productionAdmission": "blocked"
}
```

只有全部 required case 的矩阵执行、完整相交分析和双运行稳定性均通过时，`evidenceStatus` 才能为
`complete`。`productionGatePassedCases` 不得根据任务完成数推导。

## 7. 兼容与安全边界

```text
不修改 slicesoft.mesh_repair.12e_08c.1；
不修改 p0.rgbwsv.2；
不修改 channelOrder=R G B W S V、uint8、black_is_print；
OpenVDB 保持 OFF；
legacy production path 不变；
本矩阵不写 TIFF、preview 或 production package。
```
