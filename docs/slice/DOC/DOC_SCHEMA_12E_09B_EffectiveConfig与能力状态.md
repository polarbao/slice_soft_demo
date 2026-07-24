# DOC_SCHEMA 12E-09B Effective Config 与能力状态

> 状态：FROZEN / 09B-02 IMPLEMENTED
> 日期：2026-07-24

## 1. 目的

冻结 Qt UI、session Effective Config、核心 Router、manifest/report 之间的双模式审计字段。本文不新增
核心配置 schema；配置字段必须落入现有 `slicePipeline` 和 08D Profile 合同。

## 2. UI 状态 DTO

```text
requestedPipelineMode: legacy | global_surface_shell
effectivePipelineMode: legacy | global_surface_shell | not_evaluated
requestedProfileId: string
effectiveProfileId: string | not_evaluated
admissionState: pending | stale | running | blocked | admitted
productionOutputWritten: bool
fallbackApplied: bool
blockingCode: string | empty
blockingMessage: string | empty
resourceCostLevel: normal | high | not_evaluated
measuredTotalMs: number | null
measuredPeakWorkingSetBytes: integer | null
sessionId: string
configPath: string
packagePath: string | empty
```

未测量值必须为 `null/not_evaluated`，不能用 `0` 冒充。

## 3. Effective Config 审计投影

session 生成结果至少能从配置及附属 UI audit data 还原：

```text
sourceProfileId；
requestedPipelineMode；
requestedProductionProfileId；
capabilityLockVersion；
disabledOverrides[]；
model/source identity；
session identity；
生成时间；
09A diagnostic width/modelFill fields（仅在 09A 已提供时记录）。
```

核心 parser 只消费其正式字段；UI audit 字段不得成为绕过核心校验的第二真源。

09B-02 冻结的实际 JSON 投影为：

```json
{
  "slicePipeline": {
    "mode": "legacy"
  },
  "materialProcessProfile": {
    "target": "existing_legacy_profile"
  },
  "uiAudit": {
    "production": {
      "schema": "slicesoft.ui.production_effective_config.12e_09b.1",
      "sourceProfileId": "source_profile",
      "requestedPipelineMode": "legacy",
      "effectivePipelineMode": "legacy",
      "requestedProductionProfileId": "existing_legacy_profile",
      "effectiveProductionProfileId": "existing_legacy_profile",
      "capabilityLockVersion": "slicesoft.ui.production_capability.12e_09b.1",
      "disabledOverrides": [],
      "sourceModelPath": "model/example.obj",
      "sourceTemplatePath": "samples/configs/example.json",
      "sessionId": "session_identity",
      "generatedAtUtc": "2026-07-24T09:30:00.000Z"
    }
  }
}
```

Global 模式下，`disabledOverrides[]` 记录被只读 Production Profile 恢复的 stale UI
override 叶路径。该数组是审计证据，不是核心 parser 的输入。

## 4. Profile 能力表

```text
legacy:
  support/varnish/material = 现有 Profile 决定

global_surface_shell_restricted_candidate:
  RGB = enabled
  W = enabled
  S = disabled
  V = disabled

global_surface_shell_material_parity_candidate:
  RGB = enabled
  W = enabled
  S = lower + internal_void
  V = surface + outer
```

任何未知 Profile、模式/Profile 不匹配或不支持 override 都必须 fail-closed。

## 5. 跨层一致性

生产成功时必须满足：

```text
UI requestedPipelineMode
  == Effective Config slicePipeline.mode
  == Router requested/effective mode
  == manifest requestedPipelineMode/effectivePipelineMode；

productionOutputWritten == true；
fallbackApplied == false；
packagePath 属于当前 sessionId。
```

生产失败时：

```text
productionOutputWritten == false；
packagePath 为空；
blockingCode 非空；
不得加载先前 session package。
```

## 6. 版本

首版能力锁定版本：

```text
slicesoft.ui.production_capability.12e_09b.1
```

能力版本只用于 UI audit 和测试，不替代 `p0.rgbwsv.2`。
