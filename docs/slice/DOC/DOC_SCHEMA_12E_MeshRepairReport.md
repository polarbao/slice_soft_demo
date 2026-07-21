# DOC_SCHEMA_12E Mesh Repair Report

> 文档状态：PARTIAL / R1、R2、R3-01、R3-01A DIAGNOSTIC CONTRACT IMPLEMENTED
> Schema：`slicesoft.mesh_repair.12e_08c.1`
> 日期：2026-07-21

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
  "sourceMappings": [],
  "vertexMappings": [],
  "generatedTriangleMappings": [],
  "attributePreservation": {},
  "evidenceValidation": {},
  "nonManifoldAnalysis": {},
  "completeSelfIntersectionAnalysis": {},
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

## 5.1 Repair Options

R2-02/R2-03 新增的显式开关和预算都属于 options hash：

```json
{
  "allowVertexWeld": false,
  "weldToleranceMm": 0.0,
  "allowWindingRepair": false,
  "allowBoundaryFill": false,
  "maxBoundaryLoopEdges": 0,
  "maxBoundaryLoopDiameterMm": 0.0,
  "maxBoundaryLoopPerimeterMm": 0.0,
  "maxBoundaryPlanarityErrorMm": 0.0,
  "maxHoleAreaMm2": 0.0,
  "maxAffectedFaceRatio": 0.0,
  "allowNewFaces": false,
  "newFaceAttributePolicy": "reject",
  "validatePostRepairEvidence": false,
  "classifyNonManifoldPatterns": false,
  "analyzeCompleteSelfIntersections": false,
  "maxCompleteSelfIntersectionCandidatePairs": 5000000
}
```

repair 开关默认关闭；零预算不授权 boundary fill。任何开关、阈值或属性策略变化都必须改变 `optionsHash`。

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

## 9. Source Mapping

```json
{
  "sourceTriangleIndex": 13,
  "outputTriangleIndex": null,
  "disposition": "removed_exact_duplicate",
  "retainedSourceTriangleIndex": 0
}
```

`disposition` 固定为 `retained | removed_degenerate | removed_exact_duplicate`。保留面必须有
`outputTriangleIndex`；exact duplicate 必须记录 `retainedSourceTriangleIndex`；退化面两个可选输出字段为
`null`。mapping 按 `sourceTriangleIndex` 排序，覆盖 adapter 已过滤退化面和所有 accepted triangle。

## 9.1 Vertex Mapping

```json
{
  "outputVertexIndex": 7,
  "sourceVertexIndices": [7, 8]
}
```

R2-02 执行后，每个 candidate 输出顶点必须有一条 mapping；`sourceVertexIndices` 升序、非空且不重复。
未发生 weld 时为一对一 identity mapping；实际 weld group 必须同时有 `weld_vertex` operation。

## 9.2 Generated Triangle Mapping

```json
{
  "outputTriangleIndex": 10,
  "generatingBoundaryVertexIndices": [4, 5, 6],
  "attributePolicy": "inherit_uniform_material_no_uv",
  "materialName": "fixture-material",
  "hasUv": false
}
```

R2-03 新面不进入 `sourceMappings[]`，必须由独立 mapping 覆盖。output index 唯一有效，三个 vertex id 与实际
triangle 一致；policy/material/hasUv 必须与实际 attributes 一致。`attributePreservation.newTriangles` 等于
mapping 数量。

## 10. Attribute Preservation

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

## 10.1 Evidence Validation

```json
{
  "status": "not_evaluated",
  "pass": false,
  "operationSequencePass": false,
  "sourceMappingPass": false,
  "vertexMappingPass": false,
  "generatedMappingPass": false,
  "attributePass": false,
  "postStrictComplete": false,
  "postStrictPass": false,
  "hashConsistencyPass": false,
  "candidateAccepted": false,
  "blockerCodes": [],
  "issues": []
}
```

R2-04 按上述字段顺序短路验证。稳定状态包括 `passed`、`blocked_operation_sequence`、
`blocked_source_mapping`、`blocked_vertex_mapping`、`blocked_generated_mapping`、
`blocked_generated_attribute`、`blocked_attribute_preservation`、`blocked_incomplete_post_strict`、
`blocked_post_strict`、`blocked_hash_consistency` 和 `blocked_non_production_safety`。只有全部 Gate PASS 才允许
`candidateAccepted=true`；该字段仍不等价于 production admission。

## 10.2 Non-Manifold Analysis

```json
{
  "status": "not_evaluated",
  "complete": false,
  "allEdgesClassified": false,
  "allUniqueFanSplitsFeasible": false,
  "nonManifoldEdgeCount": 0,
  "duplicateShellOrExporterDuplicateEdges": 0,
  "separableLocalEdgeFanEdges": 0,
  "overlappingComponentEdges": 0,
  "mixedWindingFanEdges": 0,
  "attributeConflictingFanEdges": 0,
  "unclassifiedEdges": 0,
  "edges": [],
  "issues": []
}
```

每条 edge 记录 `edgeVertexIndices`、`incidentTriangleIndices`、`incidentSourceTriangleIndices`、
`residualComponentIds`、forward/reverse uses、pattern flags、`uniqueFanSplitFeasible` 和稳定 `reasonCode`。
edge 按顶点 key 排序，triangle/source/component ids 升序；本对象只提供结构证据，不表示 repair 已执行。

## 10.3 Complete Self-Intersection Analysis

```json
{
  "status": "not_evaluated",
  "complete": false,
  "triangleCount": 0,
  "bvhNodeCount": 0,
  "candidatePairCount": 0,
  "testedPairCount": 0,
  "confirmedIntersectionPairs": 0,
  "coplanarOverlapPairs": 0,
  "touchingOnlyPairs": 0,
  "aabbOnlyPairs": 0,
  "candidatePairHash": null,
  "durationMs": null,
  "peakWorkingSetBytes": null,
  "blockerCode": "",
  "issues": []
}
```

`status` 固定为 `not_evaluated | complete_no_intersection | confirmed_intersection | coplanar_overlap |
touching_only | budget_or_resource_blocked`。完整结果要求 `complete=true`、
`testedPairCount=candidatePairCount` 且 `candidatePairHash` 为有效 SHA-256；预算或资源阻断时不得对部分候选
做 narrow-phase PASS，也不得生成最终 pair hash。

候选 pair 是排除共享顶点邻接面后的 AABB overlap 集合，统一按 `(minTriangleId,maxTriangleId)` 排序和去重。
`durationMs` 与 `peakWorkingSetBytes` 不参与稳定 hash。confirmed 或 coplanar 继续触发 strict blocker；
touching-only 可作为完整证据，但不等价于其他 topology Gate 自动通过。

## 11. Admission

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

## 12. Performance

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

## 13. Issues

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

## 14. Compatibility

```text
报告为附加诊断证据；
不修改 p0.rgbwsv.2；
不修改 RGBWSV/uint8/black_is_print；
legacy Profile 不要求该报告；
OpenVDB OFF 构建必须可生成 preflight/repair 报告。
```

R1-01 已实现内存 DTO、canonical hash 和 report skeleton serializer；R1-02 已实现纯 pre-repair eligibility
policy；R1-03 已用 11 个 generated policy-contract fixtures 冻结 report projection golden；R1-04 已为三个
真实 OBJ 和闭合 Texture2D 3MF 生成只读 Preflight report；R2-01 已实现 cleanup/source mapping；R2-02 已实现
受约束 weld/winding、vertex mapping 和组件守门；R2-03 已实现简单 boundary fill、generated mapping 和显式
new-face policy；R2-04 已实现统一 evidence validator/post-strict guard、候选丢弃与 negative tests；R3-01
已实现 non-manifold pattern classifier；R3-01A 已实现完整自相交分析、pair hash、预算阻断和真实模型证据。
尚未实现 fan split、通用自相交修复或 production admission；报告文件仅由诊断 app 写入，repair core 仍不拥有
文件系统写入职责。
