# DOC_SCHEMA_12E Mesh Repair Report

> 文档状态：PARTIAL / R1-01 CONTRACT + R1-02 ELIGIBILITY + R1-03 GOLDEN IMPLEMENTED
> Schema：`slicesoft.mesh_repair.12e_08c.1`
> 日期：2026-07-20

## 1. 目的

定义 12E-08C 修复专项的机器可读报告。报告用于证明修复前后诊断、操作、属性保持、确定性和 strict 结果，
不替代 `p0.rgbwsv.2` manifest，也不授予 production admission。

建议路径：

```text
reports/mesh_repair_report.json
```

## 2. 根结构

```json
{
  "schema": "slicesoft.mesh_repair.12e_08c.1",
  "status": "not_evaluated",
  "mode": "strict_closed",
  "repairEnabled": false,
  "repairAttempted": false,
  "productionOutputWritten": false,
  "input": {},
  "options": {},
  "hashes": {},
  "preRepair": {},
  "eligibility": {},
  "operations": [],
  "attributePreservation": {},
  "postRepair": {},
  "admission": {},
  "performance": {},
  "issues": []
}
```

## 3. 状态枚举

```text
not_evaluated
strict_pass_no_repair
repair_candidate
repaired_strict_pass
manual_repair_required
rejected_self_intersection
repair_failed
diagnostic_only
```

约束：

```text
repairEnabled=false 时 repairAttempted 必须为 false；
repaired_strict_pass 必须有非空 operations 和 postRepair.strictPass=true；
manual/rejected/failed 时 productionOutputWritten=false；
本专项所有状态在 08D 前 productionOutputWritten=false。
```

## 4. Input

```json
{
  "sourcePath": "model/obj/nai_you_new/model.obj",
  "inputFormat": "obj",
  "finalTransform": [],
  "vertexCount": 0,
  "triangleCount": 0,
  "componentCount": 0,
  "materialCount": 0,
  "textureResourceCount": 0
}
```

报告路径可以是仓库相对路径或经过脱敏的稳定标识，不要求记录开发机绝对路径。

## 5. Hashes

```json
{
  "algorithm": "sha256",
  "canonicalizationVersion": "mesh_repair_canonical.1",
  "sourceHash": null,
  "preRepairGeometryHash": null,
  "preRepairAttributeHash": null,
  "postRepairGeometryHash": null,
  "postRepairAttributeHash": null,
  "repairOperationHash": null,
  "optionsHash": null
}
```

未计算使用 `null`，不得用空字符串或全零冒充有效 hash。

## 6. Diagnostics

`preRepair` 和 `postRepair` 共享以下核心结构：

```json
{
  "available": true,
  "strictPass": false,
  "boundaryEdges": 0,
  "nonManifoldEdges": 0,
  "duplicateFaces": 0,
  "oppositeDuplicateFaces": 0,
  "localWindingIssues": 0,
  "degenerateTriangles": 0,
  "connectedComponents": 0,
  "confirmedSelfIntersectionPairs": 0,
  "issues": []
}
```

post-repair production candidate 要求所有 strict blocker 为零。

## 7. Eligibility

```json
{
  "status": "manual_repair_required",
  "automaticRepairAllowed": false,
  "decisions": [
    {
      "issueCode": "MESH_NON_MANIFOLD_EDGES",
      "classification": "conditional",
      "eligible": false,
      "reasonCode": "E_12E_REPAIR_AMBIGUOUS_TOPOLOGY",
      "affectedCount": 59,
      "threshold": null,
      "suggestedAction": "inspect edge fan components"
    }
  ]
}
```

`classification` 固定为 `eligible | conditional | manual_only | fail_fast`。

## 8. Operations

```json
{
  "operationId": 1,
  "type": "remove_exact_duplicate_face",
  "reasonCode": "MESH_DUPLICATE_FACES",
  "inputElementIds": [],
  "outputElementIds": [],
  "parameters": {},
  "attributeDecision": "preserved",
  "affectedVertices": 0,
  "affectedEdges": 0,
  "affectedFaces": 1,
  "durationMs": 0.0
}
```

操作顺序稳定，ID 从 1 递增。元素 ID 数量过大时可记录稳定摘要和 hash，但不能完全丢失可追溯性。

## 9. Attribute Preservation

```json
{
  "status": "not_evaluated",
  "sourceMappedTriangles": 0,
  "newTriangles": 0,
  "unknownSourceTriangles": 0,
  "materialConflicts": 0,
  "uvConflicts": 0,
  "missingTextureResources": 0,
  "fallbackTriangles": 0,
  "maxUvDelta": null,
  "pass": false,
  "issues": []
}
```

production candidate 要求 `unknownSourceTriangles=0`、冲突为零、资源完整，并满足显式 new-face policy。

## 10. Admission

```json
{
  "mode": "repair_then_strict",
  "status": "non_production_only",
  "postRepairStrictPass": false,
  "productionAllowed": false,
  "blockerCodes": [],
  "warningCodes": [],
  "suggestedActions": []
}
```

R1/R2/R3 报告即使 post-repair strict PASS，也只能作为 08D 输入证据；在 08D 实现前不得直接把
`productionAllowed` 写成 true。

## 11. Performance

```json
{
  "diagnosticsMs": null,
  "eligibilityMs": null,
  "repairMs": null,
  "attributeValidationMs": null,
  "postDiagnosticsMs": null,
  "hashMs": null,
  "totalRepairCoreMs": null,
  "peakWorkingSetBytes": null
}
```

JSON/TIFF/PNG 写盘不计入 `totalRepairCoreMs`。

## 12. Issues

```json
{
  "code": "E_12E_REPAIR_MANUAL_REQUIRED",
  "severity": "error",
  "message": "mesh repair requires manual intervention",
  "operationId": null,
  "elementId": null,
  "context": {}
}
```

测试优先断言稳定 `code` 和结构字段。

## 13. Compatibility

```text
报告为附加诊断证据；
不修改 p0.rgbwsv.2；
不修改 RGBWSV/uint8/black_is_print；
legacy Profile 不要求该报告；
OpenVDB OFF 构建必须可生成 preflight/repair 报告。
```

R1-01 已实现内存 DTO、canonical hash 和 report skeleton serializer；R1-02 已实现纯 pre-repair eligibility
policy；R1-03 已用 11 个 generated policy-contract fixtures 冻结 report projection golden。尚未实现真实模型
baseline、实际 repair、文件写入、post-strict 或 production admission。
