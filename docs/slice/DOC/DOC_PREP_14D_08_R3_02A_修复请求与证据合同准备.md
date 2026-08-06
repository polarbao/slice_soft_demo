# DOC_PREP_14D-08-R3-02A 修复请求与证据合同准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R3-02A`
>
> 文档状态：`PREPARATION_GATE=PASS`

## 1. 准备结论

`geometry.repair` 的请求基线、目标资产、证据和 strict 复检顺序已冻结。R3-02B 实现前复核进一步
确认：现有字段不足以固定模型加载 Profile、资源范围与输出格式，仓库也没有生产资产 Writer 和
单模型 strict adapter。因此本文仍是 02A 基线，但 02B 的最新门禁以
`DOC_PREP_14D_08_R3_02B_修复Facade与Worker执行准备.md` 为准，不能直接进入 executor 实现。

## 2. file contract 字段映射

`request.input` 只接受下列业务字段；未知字段按 file contract 同 major 兼容规则忽略，但不得改变策略：

| `input` 字段 | 必填 | 规则 | 映射 |
|---|:--:|---|---|
| `modelId` | 否 | 非空时用于证据身份，不作路径推断 | result evidence identity |
| `modelPath` | 是 | 绝对、规范化、现有普通文件 | `api::RepairRequest::source_model_path` |
| `outputPath` | 是 | 绝对、位于当前 job 目录下、不得等于源文件 | `repaired_model_path` |
| `policy` | 是 | 固定 `conservative` | 既有保守 repair options |
| `requireStrictPass` | 是 | 固定 `true` | 修复后 full strict gate |

`jobId/correlationId/capability` 继续来自 envelope，不在 `input` 重复。调用方不得传“自动重建”、
“忽略属性冲突”或“跳过复检”等隐式开关。多模型 scene 的 repair 仍是单 source asset 操作；
`modelId` 只用于选定资产，不对某个 instance 的 world transform 做修复。

## 3. 输出与证据

成功 result 的 `output` 必须包含：

```text
outputPath
sourceDigest
outputDigest
preflightBefore
preflightAfter
evidence
elapsedMs
```

`evidence` 至少包含 repair policy、canonicalization version、执行操作列表、source triangle/vertex
映射、generated triangle 来源、UV/material/attribute preservation、pre/post topology、完整性和
strict admission。digest 固定为 `sha256:<64 lowercase hex>`；`outputDigest` 必须基于最终 repaired
asset 字节，不得以临时内存摘要替代。

## 4. 输出资产所有权

1. `outputPath` 必须位于 `<request.json parent>/repair/` 下，建议文件名为
   `<sanitized-model-id>.repaired.<source-extension>`。
2. 写入顺序固定为同目录临时文件 -> flush/close -> digest -> strict 复检 -> 原子 rename。
3. evidence 写入 `<outputPath>.evidence.json`，同样使用临时文件和原子替换。
4. 任何失败、取消或 strict 不通过都删除当前 job 的临时文件和未发布 repaired asset。
5. 原始 `modelPath` 只读，成功与失败均不得修改其字节、时间戳或相邻资源。
6. repaired asset 不等于生产 Package；最终跨 job 发布仍由 14D-05 的安全发布边界处理。

## 5. 固定执行顺序

```text
validate envelope/input/path ownership
  -> hash source bytes
  -> full strict preflight before
  -> classify conservative repair eligibility
  -> execute approved deterministic operations into staging
  -> validate attribute/UV/material evidence
  -> full strict preflight after
  -> require strict PASS and complete evidence
  -> hash staged asset
  -> atomically publish repaired asset and evidence
  -> build Worker result
```

若修复前已经 strict PASS，返回 source-copy 或 no-op repaired asset 必须有显式 `no_repair_required`
证据；不得把原路径直接冒充 job-owned 输出。复杂自相交、歧义 non-manifold、属性冲突和证据不完整
继续 `manual_repair_required`/blocked，禁止自动重建。

## 6. 既有服务映射

- `EvaluateMeshRepairPreflight`：修复前 topology/eligibility 证据。
- `ExecuteMeshRepairCleanup`：仅执行批准的保守操作，不承担文件发布。
- `BuildMeshRepairReport`：生成 evidence JSON 基础，不替代公开 result identity。
- `R3-01A SceneFullPreflightService`：提供可复用的 topology/admission 组件和 scene-wide 复检入口；
  repaired source asset 在进入真实 scene 前，必须先由单模型 strict adapter 重导入并完成属性检查，
  禁止伪造 committed scene。

因此 `R3-02B` 的开发前置不只包括 `R3-01A COMPLETE`，还包括 Writer/输出格式决策、同 Profile
重导入和单模型 strict adapter。前置未闭合前不得注册为生产 capability。

## 7. API DTO 加法扩展

当前 `api::RepairRequest/RepairResult` 只承载两个路径和两个 hash，不能表达冻结输出。`R3-02B`
必须加法补齐 `model_id`、`policy`、`require_strict_pass`、pre/post preflight 和 structured evidence；
这是 engine 内部 C++ API 修订，不修改 `print_module_spi.h`、11 个导出或能力数量。

## 8. 稳定错误与清理

| 情况 | 稳定码 | 发布 |
|---|---|:--:|
| 源不存在/不可读/路径越界 | `PM-SLICER-INPUT-0001` | 否 |
| 不可保守修复、strict topology blocked | `PM-SLICER-TOPOLOGY-0010/0011` | 否 |
| 输出写入/rename/digest 失败 | `PM-SLICER-OUTPUT-0050` | 否 |
| 取消 | `PM-SLICER-CANCELLED-0070` | 否，`stagingRemoved=true` |
| 未分类异常 | `PM-SLICER-INTERNAL-0099` | 否 |

domain blocked 必须保留 preflight/evidence 诊断，但不得留下可被识别为成功 repaired asset 的文件。

## 9. 文件所有权与验收

```text
src/slicer_core/engine/ProductionRepairFacadeFactory.h/.cpp
apps/slicer_worker/repair/WorkerRepairExecutor.h/.cpp
tests/stage14d_08_r3/WorkerRepairExecutorTests.cpp
```

最低用例：strict pass no-op、允许的退化面/重复面修复、source=output 拒绝、scope escape、
self-intersection/manual required、属性冲突、修复后 strict 失败、取消、输出失败、原资产不变、
双运行 digest/操作证据稳定。

## 10. 门禁

```text
14D_08_R3_02A_PREPARATION_GATE=PASS
FILE_CONTRACT_MINOR_CHANGE_REQUIRED=YES_FOR_PROFILE_AND_OUTPUT_IDENTITY
14D_08_R3_02B_PREPARATION_GATE=BLOCKED_BY_ASSET_WRITER_AND_STRICT_ADAPTER
14D_08_R3_02B_IMPLEMENTATION=NOT_STARTED
```
