# DOC_DECISION_09B_R3_真实模型拓扑生产准入策略

> 文档版本：v0.1
> 文档状态：Decision / Production Admission
> 适用阶段：09B-R3
> 分支：`spike/09B-R3-shell-production-readiness`

## 1. 决策背景

09B-R2 证明真实 OBJ / 3MF 指甲模型可以在 OpenVDB 表面壳层纹理实验链路中跑通，但两者均以 `warn_and_attempt` + `nonProduction=true` 记录。R2 的自相交字段还是 AABB candidate，不能作为生产拒绝依据。

09B-R3 已补充 narrow-phase triangle-triangle self-intersection、稳定 issue code、repeat texture fixture 和 Windows process peak working set。本文件用于明确真实模型是否可进入 production admission。

## 2. 真实模型 R3 诊断摘要

| Case | Triangles | Components | Non-manifold | Duplicate | Opposite Duplicate | Local Winding | AABB Candidates | Confirmed Self-intersection | Result |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| OBJ golden | 70262 | 2 | 299 | 0 | 0 | 1305 | 6 | 0 | nonProduction |
| 3MF golden | 75596 | 3 | 10939 | 7190 | 7190 | 0 | 6 | 0 | nonProduction |

R3 narrow-phase 结论：

```text
OBJ / 3MF 的 R2 selfIntersectionPairs 在 R3 下均被识别为 AABB false positive；
当前真实模型没有 confirmed self-intersection；
生产准入失败的主因转移为 non-manifold、duplicate/opposite duplicate、local winding、multi-component。
```

## 3. Issue 分类与策略

| Issue | OBJ | 3MF | Production 策略 | 说明 |
|---|---:|---:|---|---|
| `MESH_NON_MANIFOLD_EDGES` | 299 | 10939 | reject 或 repair_then_strict | 不应在 production strict 中直接通过 |
| `MESH_DUPLICATE_FACES` | 0 | 7190 | repair_then_strict | 可评估去重修复，但修复后必须重新诊断 |
| `MESH_OPPOSITE_DUPLICATE_FACES` | 0 | 7190 | repair_then_strict / reject | 可能影响法线与 SDF，不能直接通过 |
| `MESH_LOCAL_WINDING_INCONSISTENCY` | 1305 | 0 | repair_then_strict / reject | 需要法线/朝向修复策略 |
| `MESH_SELF_INTERSECTION_CONFIRMED` | 0 | 0 | strict 可通过该项 | R3 没有 confirmed 自交 |
| `MESH_SELF_INTERSECTION_SAMPLED` | yes | yes | warning + 扩展检查 | 真实模型规模触发采样，production 前需确认预算或加速结构 |
| multi-component | 2 | 3 | diagnostic_only 或 explicit multi-object policy | 不能默认假设多组件都属于同一可打印实体 |

## 4. 准入模式决策

| Mode | 当前建议 | 原因 |
|---|---|---|
| `strict_closed` | 不允许真实 OBJ/3MF 进入 production | 两个真实模型均有 strict 拒绝项 |
| `repair_then_strict` | 下一阶段优先评估 | duplicate / winding 具备自动修复可能，但本阶段不实现完整 repair |
| `warn_and_attempt` | 允许实验预览，不允许 production package | 可用于 report/preview/benchmark，不可写入正式 RGBWSV TIFF |
| `diagnostic_only` | 允许 | 可用于 UI/report 提示模型不可直接生产 |

## 5. 进入 09P 的条件建议

可以进入 09P 设计阶段，但 09P-R1 implementation 必须带 feature flag，并保持 experimental production path，不得默认替代 legacy。

进入 09P-R1 前建议至少明确：

```text
1. production strict_closed 拒绝 non-manifold / duplicate / local winding；
2. warn_and_attempt 输出只能标记 nonProduction；
3. repair_then_strict 若实现，必须输出 repair report 和修复前后 hash；
4. confirmed self-intersection 必须 fail_fast；
5. sampled self-intersection 必须进入 warningCodes，并在 report 中记录预算。
```

## 6. 结论

真实 OBJ/3MF 当前不具备 production safe 准入资格。R3 消除了 R2 对自相交的误判风险，但暴露出更明确的生产阻塞项：真实模型拓扑质量不足。

下一步建议：

```text
09P 可以开始做 production pipeline 接入设计；
09P-R1 不应默认启用真实模型 production 输出；
若要让真实模型进入 production，应追加 09B-R4 或 09P 前置的 mesh repair / admission gate 设计。
```
