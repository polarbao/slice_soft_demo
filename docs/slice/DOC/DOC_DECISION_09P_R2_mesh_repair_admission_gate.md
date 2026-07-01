# DOC_DECISION_09P_R2_mesh_repair_admission_gate

> 文档版本：v0.1
> 文档状态：Accepted / Stage 09P-R2-4
> 生成日期：2026-07-01
> 决策主题：mesh repair 前置判断与 `repair_then_strict` 准入边界

---

## 1. Context

09P-R2 的目标是 hardening experimental OpenVDB surface-shell pipeline，而不是实现完整自动 mesh repair。

09B-R3 与 09P-R2 已确认：

```text
1. 真实 OBJ / 3MF 可进入 experimental diagnostic path；
2. 真实模型仍可能包含 non-manifold、duplicate/opposite duplicate、local winding 等 blocker；
3. confirmed self-intersection 必须 fail fast；
4. warn_and_attempt 只能 nonProduction；
5. repair_then_strict 当前只是占位，未实现 repair 前不得 productionAllowed。
```

因此 09P-R2-4 只定义 repair 前置判断和后续准入门槛，不实现自动修复算法。

---

## 2. Decision

采用以下策略：

```text
repair_then_strict 只能表示“未来显式 repair 阶段 + 修复后重新 strict_closed 诊断”的准入路径。
09P-R2 当前不允许 repair_then_strict 返回 productionAllowed。
任何 repair 候选 issue 被修复后，都必须重新生成 diagnostics、repair report 和修复前后 hash。
只有修复后 EvaluateProductionAdmission(postRepairIssues, strict_closed) 返回 productionAllowed，才可进入生产候选判断。
```

注意：即使未来 `repair_then_strict` 通过 strict 准入，也不等同于 09P-R2 experimental CLI 可以写 production RGBWSV package。生产写入仍受后续 release gate、profile 和输出契约控制。

---

## 3. Issue Classification

| Issue | 09P-R2 当前动作 | 未来 repair 候选 | 说明 |
|---|---|---:|---|
| `MESH_SELF_INTERSECTION_CONFIRMED` | reject / fail_fast | false | 已确认自相交不能在 09P-R2 自动修，避免错误内外判断 |
| `OPENVDB_UNAVAILABLE` | block experimental OpenVDB path | false | 环境或依赖问题，不是 mesh repair 问题 |
| `OPENVDB_LEVEL_SET_FAILED` | block experimental OpenVDB path | false | kernel 失败，需先定位 OpenVDB/SDF 构建原因 |
| `MESH_BOUNDARY_EDGES` | strict blocker | conditional | 未来仅在显式 hole-fill/stitch 策略和阈值明确后可尝试 |
| `MESH_NON_MANIFOLD_EDGES` | strict blocker | conditional | 未来仅在局部可分解、可复闭合的情况下尝试 |
| `MESH_DUPLICATE_FACES` | strict blocker | true | 可评估去重，但必须保持材质/UV/纹理归属一致 |
| `MESH_OPPOSITE_DUPLICATE_FACES` | strict blocker | conditional | 可评估冲突面解析；无法确定内外或材质归属时必须 reject |
| `MESH_LOCAL_WINDING_INCONSISTENCY` | strict blocker | conditional | 可评估局部绕序/法线重定向；修复后必须重建拓扑诊断 |

多组件模型不是 09P-R2-3 的稳定 gate code，但未来 repair/admission 专项必须单独决策：不能隐式 merge 多组件，也不能默认假设所有组件属于同一可打印实体。

---

## 4. Repair Preconditions

未来若实现 repair stage，必须满足：

```text
1. repair 必须显式启用，不能作为 strict_closed 的隐式副作用；
2. repair 输入必须来自稳定 ValidationIssue code；
3. repair 前必须记录 source model path、normalized config、preRepair diagnostics；
4. repair 前必须记录 preRepair hash；
5. repair 必须输出 operation list、threshold、affected face/edge/vertex count；
6. repair 后必须记录 postRepair hash；
7. repair 后必须重新运行 topology diagnostics 和 OpenVDB level set 检查；
8. repair 后必须重新调用 strict_closed admission；
9. repair 失败、超预算或结果不可解释时必须回退到 nonProduction 或 reject。
```

---

## 5. Hash Contract

repair report 至少需要以下 hash 概念，具体算法可在后续 DEV 文档固定：

| 字段 | 用途 |
|---|---|
| `sourceHash` | 原始输入文件或解包后模型数据摘要 |
| `preRepairGeometryHash` | repair 前归一化顶点与三角面拓扑摘要 |
| `preRepairAttributeHash` | repair 前材质、UV、纹理索引等属性摘要 |
| `postRepairGeometryHash` | repair 后归一化顶点与三角面拓扑摘要 |
| `postRepairAttributeHash` | repair 后材质、UV、纹理索引等属性摘要 |
| `repairOperationHash` | repair 操作列表与参数摘要 |

hash 的目标不是安全加密，而是让 QA、UI 和后续 AI 接续能判断“同一输入是否经过同一 repair 策略得到同一候选结果”。

---

## 6. When repair_then_strict May Allow Production

当前 09P-R2：永不允许。

未来允许进入 production candidate 的必要条件：

```text
1. repair stage 已在独立任务中实现并有测试；
2. repair profile 显式开启；
3. preRepair / postRepair diagnostics 和 hash 完整；
4. postRepair diagnostics 不包含 09P-R2 topology blocker；
5. confirmed self-intersection 不存在；
6. OpenVDB 可用且 level set 成功；
7. EvaluateProductionAdmission(postRepairIssues, strict_closed).productionAllowed == true；
8. output contract、profile、release gate 均允许写 production package。
```

任何一项不满足，`repair_then_strict` 都只能输出 `non_production_only` 或 `diagnostic_only`。

---

## 7. Alternatives Considered

| 方案 | 结论 | 原因 |
|---|---|---|
| 所有 blocker 一律 reject | 暂不采用为唯一策略 | 安全但会阻断 duplicate/winding 等可能可修模型 |
| `warn_and_attempt` 允许 production | 拒绝 | 违反 09P-R2 安全红线 |
| strict_closed 内隐式 repair | 拒绝 | 会让生产准入不可解释，也无法记录修复前后证据 |
| 显式 `repair_then_strict` + 重新诊断 | 采纳为未来方向 | 可解释、可回归，并保持当前 production gate 保守 |

---

## 8. Consequences

正向影响：

```text
1. 真实模型不会因为可疑 repair 被误标 production-safe；
2. 后续 repair 专项有明确输入、输出和验收口径；
3. UI/report 可以解释为什么当前模型只能 nonProduction；
4. 保持 09P-R2 hardening 范围，不扩张到大算法实现。
```

代价：

```text
1. 当前真实复杂 OBJ/3MF 仍可能无法进入 production candidate；
2. 后续必须补 repair report、hash contract 和重新诊断测试；
3. duplicate / winding 等问题的生产可用性要延后到 repair 专项判断。
```

---

## 9. Validation

本决策是文档任务，验证方式：

```powershell
git status --short
git diff --check
```

后续实现 repair stage 时，需要新增：

```text
repair report schema test
pre/post hash deterministic test
post-repair strict admission unit test
real OBJ/3MF repair fixture
```

---

## 10. Follow-up

后续可拆为独立阶段或 09P-R3/R4 前置任务：

```text
1. duplicate / opposite duplicate repair prototype；
2. local winding repair prototype；
3. non-manifold repair feasibility audit；
4. multi-component admission policy；
5. repair report schema；
6. repair_then_strict release gate。
```

