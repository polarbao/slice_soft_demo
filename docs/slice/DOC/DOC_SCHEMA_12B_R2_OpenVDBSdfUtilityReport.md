# DOC_SCHEMA_12B_R2 OpenVDB SDF Utility Report

> 文档状态：Schema / Stage 12B-R2
> 日期：2026-07-08
> Schema：`slicesoft.openvdb_sdf_utility.12b_r2.1`
> 对应任务：12B-R2-02 Utility Report Schema

## 1. 目标

该 schema 用于固定 12B-R2 阶段 OpenVDB SDF utility 的诊断报告字段。

它只描述 OpenVDB 作为可选 SDF utility 的可用性、执行状态、能力矩阵和推进建议，不描述 production RGBWSV package，也不替代 benchmark report。

明确边界：

```text
1. 不是 p0.rgbwsv.2 production package schema；
2. 不是 slicesoft.benchmark.12b.1 core-only benchmark schema；
3. 不允许写 production RGBWSV TIFF；
4. 不允许把 OpenVDB 标记为 production slicer replacement；
5. productionReplacementAllowed 必须为 false；
6. USE_OPENVDB=OFF 时也允许输出 unavailable report。
```

## 2. Report 根结构

根结构：

```json
{
  "schema": "slicesoft.openvdb_sdf_utility.12b_r2.1",
  "generatedAt": "2026-07-08T00:00:00+08:00",
  "producer": {},
  "build": {},
  "input": {},
  "config": {},
  "outputPolicy": {},
  "utilities": {},
  "decision": {},
  "validation": {},
  "issues": []
}
```

字段说明：

| 字段 | 必填 | 说明 |
|---|---|---|
| `schema` | 是 | 固定为 `slicesoft.openvdb_sdf_utility.12b_r2.1` |
| `generatedAt` | 是 | ISO-8601 时间字符串 |
| `producer` | 是 | 生成工具、版本、入口命令 |
| `build` | 是 | OpenVDB 构建可用性和构建类型 |
| `input` | 是 | 模型、格式、姿态、准入模式 |
| `config` | 是 | voxel、shell、distance、closure 等 utility 参数 |
| `outputPolicy` | 是 | 输出限制，必须表明不写 production package |
| `utilities` | 是 | 四类 utility 的执行结果 |
| `decision` | 是 | 12B-R2 推进结论 |
| `validation` | 是 | 本报告自身校验与外部验证命令 |
| `issues` | 是 | report 级 warnings/blockers |

## 3. producer

示例：

```json
{
  "tool": "openvdb_sdf_utility_probe",
  "toolVersion": "12b-r2-prototype",
  "commandLine": "slicer_cli --openvdb-sdf-utility-report ...",
  "workingDirectory": "E:/__Code/__Work/slice_test_demo/slice_soft_demo"
}
```

规则：

```text
tool 可以是 CLI、脚本或 demo app；
commandLine 允许为空，但 validation.commands 必须记录实际验证命令；
不得把 slicer_cli production package run 伪装成 utility report。
```

## 4. build

示例：

```json
{
  "buildType": "Debug",
  "useOpenVdb": false,
  "openVdbAvailable": false,
  "openVdbVersion": null,
  "buildDir": "build",
  "vcpkgRoot": null,
  "openVdbUnavailableReason": "use_openvdb_off"
}
```

规则：

```text
useOpenVdb=false 时 openVdbAvailable 必须为 false；
openVdbAvailable=false 时 utilities 中依赖 OpenVDB 的能力必须为 unavailable 或 skipped；
OpenVDB ON lane 未运行时不能写 openVdbAvailable=true；
OpenVDB 版本未知时写 null，不得猜测。
```

常见 `openVdbUnavailableReason`：

```text
use_openvdb_off
openvdb_library_missing
openvdb_build_missing
openvdb_runtime_error
not_checked
```

## 5. input

示例：

```json
{
  "modelPath": "model/obj/nai_you_new/example.obj",
  "format": "obj",
  "samePoseWithLegacy": true,
  "admissionMode": "strict_closed",
  "meshDiagnosticsAvailable": true,
  "grid": {
    "widthPx": 283,
    "heightPx": 531,
    "layerCount": 717,
    "pixelPitchUm": 42.3,
    "layerThicknessMm": 0.01
  },
  "transform": {
    "autoOrientApplied": true,
    "rotationDeg": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0],
    "translationMm": [0.0, 0.0, 0.0]
  }
}
```

规则：

```text
samePoseWithLegacy 表示是否可与 legacy production run 做几何/性能对照；
admissionMode 必须明确 strict_closed / diagnostic_only / unavailable；
grid 字段用于解释 utility 指标，不代表 production TIFF 已生成；
transform 缺失时必须在 issues 中记录 input_transform_unknown。
```

## 6. config

示例：

```json
{
  "voxelSizeMm": 0.05,
  "narrowBandVoxel": 3,
  "outerVarnishShell": {
    "enabled": true,
    "thicknessMm": 0.1,
    "allowXYExpansion": true
  },
  "clearance": {
    "enabled": true,
    "nearSurfaceDistanceMm": 0.1
  },
  "topology": {
    "enabled": true,
    "strictBlockers": true
  },
  "materialClosureAssist": {
    "enabled": true,
    "source": "semantic_mask_plus_sdf_assist"
  }
}
```

规则：

```text
voxelSizeMm 必须大于 0；
outerVarnishShell.thicknessMm 单位为 mm；
materialClosureAssist 不允许单独判定 production PASS；
12D 材料闭环仍以 RGBWSV/semantic masks 为真源。
```

## 7. outputPolicy

示例：

```json
{
  "writesProductionPackage": false,
  "writesProductionTiff": false,
  "writesPreview": false,
  "writesUtilityReport": true,
  "modifiesLegacyOutput": false,
  "protocolSchemaTouched": false
}
```

硬性规则：

```text
writesProductionPackage 必须为 false；
writesProductionTiff 必须为 false；
modifiesLegacyOutput 必须为 false；
protocolSchemaTouched 必须为 false。
```

任何违反上述规则的 report 都应被判为 invalid。

## 8. utilities

根结构：

```json
{
  "outerVarnishShell": {},
  "clearanceDistance": {},
  "topologyDiagnostic": {},
  "materialClosureAssist": {}
}
```

每个 utility item 使用统一结构：

```json
{
  "available": true,
  "executed": true,
  "status": "pass",
  "source": "openvdb_sdf",
  "promoteDecision": "keep_experimental",
  "metrics": {},
  "timingsMs": {},
  "blockers": [],
  "warnings": [],
  "notes": []
}
```

统一字段：

| 字段 | 必填 | 说明 |
|---|---|---|
| `available` | 是 | 当前构建和输入是否具备执行条件 |
| `executed` | 是 | 本次是否实际执行该 utility |
| `status` | 是 | 执行状态 |
| `source` | 是 | 指标来源 |
| `promoteDecision` | 是 | 是否推进该 utility |
| `metrics` | 是 | utility 指标 |
| `timingsMs` | 是 | utility 内部耗时，未知则为空对象 |
| `blockers` | 是 | 阻断原因 |
| `warnings` | 是 | 风险提示 |
| `notes` | 是 | 说明 |

### 8.1 status 枚举

```text
pass
fail
unavailable
blocked
skipped
not_evaluated
```

语义：

| status | 含义 |
|---|---|
| `pass` | utility 成功执行，指标满足本阶段验收 |
| `fail` | utility 执行完成，但指标不满足验收 |
| `unavailable` | 构建或依赖不可用，未执行 |
| `blocked` | 输入或 admission blocker 阻止执行 |
| `skipped` | 按配置或任务边界跳过 |
| `not_evaluated` | 尚未评估，不允许作为完成证据 |

### 8.2 promoteDecision 枚举

```text
promote
keep_experimental
reject
not_evaluated
```

语义：

| promoteDecision | 含义 |
|---|---|
| `promote` | 建议进入后续 production-adjacent utility 设计 |
| `keep_experimental` | 保留实验能力，不接入生产路径 |
| `reject` | 不建议继续投入 |
| `not_evaluated` | 证据不足，不能下结论 |

R2 默认不允许 `promote` 到 production slicer replacement；`promote` 只能表示推进为辅助 utility。

## 9. outerVarnishShell metrics

示例：

```json
{
  "available": true,
  "executed": true,
  "status": "pass",
  "source": "openvdb_sdf_shell",
  "promoteDecision": "keep_experimental",
  "metrics": {
    "voxelSizeMm": 0.05,
    "requestedThicknessMm": 0.1,
    "effectiveThicknessMmMin": 0.05,
    "effectiveThicknessMmMax": 0.15,
    "insideVoxels": 1000,
    "shellVoxels": 120,
    "outsideVoxels": 5000,
    "candidateShellPixels": 320,
    "xyExpansionAllowed": true
  },
  "timingsMs": {
    "buildGrid": 10.0,
    "classifyShell": 5.0,
    "report": 1.0
  },
  "blockers": [],
  "warnings": [],
  "notes": []
}
```

规则：

```text
candidateShellPixels 是候选统计，不代表 production V 通道已写入；
thickness 误差必须结合 voxelSizeMm 解释；
当 mesh 未通过 strict_closed 时，允许输出 blocked 或 diagnostic_only，但不得写 production shell。
```

## 10. clearanceDistance metrics

示例：

```json
{
  "available": false,
  "executed": false,
  "status": "not_evaluated",
  "source": "openvdb_sdf_distance",
  "promoteDecision": "not_evaluated",
  "metrics": {
    "minDistanceMm": null,
    "maxDistanceMm": null,
    "nearSurfaceVoxelCount": null,
    "thinRegionCount": null
  },
  "timingsMs": {},
  "blockers": ["clearance_utility_not_implemented"],
  "warnings": [],
  "notes": []
}
```

规则：

```text
距离单位必须为 mm；
尚未实现独立 utility 时必须标记 not_evaluated 或 unavailable；
不得把 surface shell texture prototype 的 maxTransferDistance 直接等价为 clearance 验收。
```

## 11. topologyDiagnostic metrics

示例：

```json
{
  "available": true,
  "executed": true,
  "status": "blocked",
  "source": "mesh_diagnostics_plus_openvdb_admission",
  "promoteDecision": "promote",
  "metrics": {
    "boundaryEdges": 12,
    "nonManifoldEdges": 0,
    "selfIntersections": 0,
    "duplicateFaces": 0,
    "admissionStatus": "rejected",
    "admissionMode": "strict_closed"
  },
  "timingsMs": {
    "meshDiagnostics": 3.0
  },
  "blockers": ["boundary_edges"],
  "warnings": [],
  "notes": ["Topology diagnostic can be promoted as a gate/report utility only."]
}
```

规则：

```text
confirmed self-intersection、non-manifold、boundary edges 在 strict production admission 中仍是 blocker；
warn_and_attempt 不得标记为 production-safe；
promote 只表示可推进诊断/gate，不表示模型可生产。
```

## 12. materialClosureAssist metrics

示例：

```json
{
  "available": false,
  "executed": false,
  "status": "not_evaluated",
  "source": "semantic_mask_plus_sdf_assist",
  "promoteDecision": "not_evaluated",
  "metrics": {
    "gapPixelCount": null,
    "nearSurfaceGapVoxelCount": null,
    "modelSupportContactGapCount": null,
    "confidence": "low"
  },
  "timingsMs": {},
  "blockers": ["material_closure_assist_not_implemented"],
  "warnings": ["Production closure must be decided by RGBWSV semantic masks."],
  "notes": []
}
```

规则：

```text
12D material closure PASS/FAIL 仍由 production semantic masks 决定；
OpenVDB 只能提供 assist source；
confidence 必须为 high / medium / low / unknown。
```

## 13. decision

示例：

```json
{
  "openVdbRole": "sdf_utility_candidate",
  "productionReplacementAllowed": false,
  "productionReplacementReason": "12B-R0 replacementPass=false and R2 is diagnostic-only",
  "recommendedNextStep": "continue_r2_utility_report_prototype",
  "capabilitySummary": {
    "outerVarnishShell": "keep_experimental",
    "clearanceDistance": "not_evaluated",
    "topologyDiagnostic": "promote",
    "materialClosureAssist": "not_evaluated"
  }
}
```

硬性规则：

```text
productionReplacementAllowed 必须为 false；
openVdbRole 只能使用 sdf_utility_candidate / diagnostic_only / unavailable / rejected；
recommendedNextStep 必须能从 utilities 的 promoteDecision 推导出来。
```

## 14. validation

示例：

```json
{
  "schemaValid": true,
  "commands": [
    {
      "command": "powershell -ExecutionPolicy Bypass -File ./scripts/run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p",
      "ran": false,
      "exitCode": null,
      "reason": "openvdb_on_build_not_checked_in_this_task"
    }
  ],
  "legacyGuard": {
    "ran": false,
    "reason": "schema_docs_only_task"
  }
}
```

规则：

```text
ran=false 时必须填写 reason；
不能把未运行命令写为 PASS；
docs-only task 可把 build/benchmark 标记为未运行，但必须说明原因。
```

## 15. issues

issue 示例：

```json
{
  "severity": "warning",
  "code": "openvdb_on_build_not_checked",
  "message": "OpenVDB ON lane was not executed in this task.",
  "path": "build.openVdbAvailable"
}
```

severity 枚举：

```text
info
warning
blocker
error
```

常见 code：

```text
use_openvdb_off
openvdb_on_build_missing
openvdb_runtime_error
strict_closed_rejected
boundary_edges
non_manifold_edges
confirmed_self_intersection
clearance_utility_not_implemented
material_closure_assist_not_implemented
production_replacement_forbidden
schema_invalid
```

## 16. OFF lane 最小 report 示例

```json
{
  "schema": "slicesoft.openvdb_sdf_utility.12b_r2.1",
  "generatedAt": "2026-07-08T00:00:00+08:00",
  "producer": {
    "tool": "openvdb_sdf_utility_probe",
    "toolVersion": "12b-r2-prototype",
    "commandLine": ""
  },
  "build": {
    "buildType": "Debug",
    "useOpenVdb": false,
    "openVdbAvailable": false,
    "openVdbVersion": null,
    "buildDir": "build",
    "openVdbUnavailableReason": "use_openvdb_off"
  },
  "input": {
    "modelPath": null,
    "format": null,
    "samePoseWithLegacy": null,
    "admissionMode": "unavailable",
    "meshDiagnosticsAvailable": false
  },
  "config": {
    "voxelSizeMm": null,
    "narrowBandVoxel": null
  },
  "outputPolicy": {
    "writesProductionPackage": false,
    "writesProductionTiff": false,
    "writesPreview": false,
    "writesUtilityReport": true,
    "modifiesLegacyOutput": false,
    "protocolSchemaTouched": false
  },
  "utilities": {
    "outerVarnishShell": {
      "available": false,
      "executed": false,
      "status": "unavailable",
      "source": "openvdb_sdf",
      "promoteDecision": "not_evaluated",
      "metrics": {},
      "timingsMs": {},
      "blockers": ["use_openvdb_off"],
      "warnings": [],
      "notes": []
    },
    "clearanceDistance": {
      "available": false,
      "executed": false,
      "status": "unavailable",
      "source": "openvdb_sdf_distance",
      "promoteDecision": "not_evaluated",
      "metrics": {},
      "timingsMs": {},
      "blockers": ["use_openvdb_off"],
      "warnings": [],
      "notes": []
    },
    "topologyDiagnostic": {
      "available": false,
      "executed": false,
      "status": "unavailable",
      "source": "mesh_diagnostics_plus_openvdb_admission",
      "promoteDecision": "not_evaluated",
      "metrics": {},
      "timingsMs": {},
      "blockers": ["use_openvdb_off"],
      "warnings": [],
      "notes": []
    },
    "materialClosureAssist": {
      "available": false,
      "executed": false,
      "status": "unavailable",
      "source": "semantic_mask_plus_sdf_assist",
      "promoteDecision": "not_evaluated",
      "metrics": {},
      "timingsMs": {},
      "blockers": ["use_openvdb_off"],
      "warnings": [],
      "notes": []
    }
  },
  "decision": {
    "openVdbRole": "unavailable",
    "productionReplacementAllowed": false,
    "productionReplacementReason": "USE_OPENVDB is OFF",
    "recommendedNextStep": "run_off_guard_or_configure_openvdb_on_lane",
    "capabilitySummary": {
      "outerVarnishShell": "not_evaluated",
      "clearanceDistance": "not_evaluated",
      "topologyDiagnostic": "not_evaluated",
      "materialClosureAssist": "not_evaluated"
    }
  },
  "validation": {
    "schemaValid": true,
    "commands": [],
    "legacyGuard": {
      "ran": false,
      "reason": "schema_example"
    }
  },
  "issues": [
    {
      "severity": "warning",
      "code": "use_openvdb_off",
      "message": "OpenVDB utility is unavailable because USE_OPENVDB is OFF.",
      "path": "build.useOpenVdb"
    }
  ]
}
```

## 17. Reader / 校验规则

最小校验：

```text
1. schema 必须等于 slicesoft.openvdb_sdf_utility.12b_r2.1；
2. outputPolicy.writesProductionPackage=false；
3. outputPolicy.writesProductionTiff=false；
4. outputPolicy.modifiesLegacyOutput=false；
5. decision.productionReplacementAllowed=false；
6. utilities 必须包含四个固定 key；
7. status / promoteDecision / severity 必须属于枚举；
8. ran=false 的 validation command 必须有 reason；
9. available=false 且 executed=true 属于 invalid；
10. openVdbAvailable=false 时 OpenVDB 依赖 utility 不能 status=pass。
```

## 18. 与其他 schema 的关系

| Schema | 用途 | 关系 |
|---|---|---|
| `p0.rgbwsv.2` | production package | R2 不修改、不替代 |
| `slicesoft.benchmark.12b.1` | core-only benchmark | R2 utility report 可引用其结论，但不混用字段 |
| `p0.surface_shell_texture_report.2` | 09B surface shell texture prototype | 可参考字段，不再扩展为 R2 正式 schema |
| `slicesoft.openvdb_sdf_utility.12b_r2.1` | R2 utility diagnostic | 本文定义 |

## 19. 完成判定

R2-02 完成条件：

```text
1. schema 名称固定；
2. unavailable / blocked / skipped / executed / promoteDecision 均已定义；
3. outputPolicy 明确禁止 production 写入；
4. OFF lane 示例可表达 unavailable；
5. 后续 R2-03/R2-06 可据此实现最小 report 或校验脚本。
```
