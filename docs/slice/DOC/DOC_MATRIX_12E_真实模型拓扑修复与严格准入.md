# DOC_MATRIX_12E 真实模型拓扑修复与严格准入

> 文档状态：IN PROGRESS / R2、R3-01、R3-01A EVIDENCE FROZEN / R3-02 READY
> 日期：2026-07-21

## 1. Gate Matrix

| Gate | R1 | R2 | R3 | 08D 要求 |
|---|---|---|---|---|
| pre diagnostics | PASS required | keep | keep | PASS |
| deterministic hash | PASS required | pre/post | repeat | PASS |
| eligibility | PASS required | enforce | enforce | no unresolved automatic ambiguity |
| repair operations | not executed | fixture PASS | real model evidence | operation list complete |
| attribute preservation | contract | fixture PASS | real model PASS | PASS |
| post strict | not applicable | generated PASS | required models PASS | PASS |
| Release budget | not measured | diagnostic only | freeze or BLOCK | PASS |
| production package | forbidden | forbidden | forbidden | explicit admission only |

## 2. Issue Policy Matrix

| Issue | 默认动作 | 自动修复资格 | 生产条件 |
|---|---|---|---|
| degenerate triangle | diagnostic/cleanup candidate | eligible under explicit mode | post strict + attribute PASS |
| exact duplicate face | diagnostic/cleanup candidate | attributes identical only | post strict + hash stable |
| opposite duplicate | block | conditional | unique interior/exterior and attributes |
| local winding | block | conditional | unique orientable propagation |
| boundary loop | block | conditional | simple/planar/within budget/new-face policy |
| non-manifold edge | block | conditional/manual | unique local fan decomposition |
| confirmed self-intersection | fail-fast | never | never auto-admitted |
| multi-component | explicit policy | no implicit merge | component ownership proven |

## 3. Generated Fixtures

| Fixture | Expected status |
|---|---|
| clean closed | strict_pass_no_repair |
| duplicate same attributes | repaired_strict_pass |
| duplicate conflicting UV | manual_repair_required |
| simple planar hole | repaired_strict_pass when enabled |
| oversized/non-planar hole | manual_repair_required |
| winding-only | repaired_strict_pass |
| separable local fan | repaired_strict_pass or documented unsupported in first increment |
| ambiguous fan | manual_repair_required |
| self-intersection | rejected_self_intersection |

## 4. Real Models

| Case | Baseline | R2-01/02/03 output | R2-04 evidence guard | R3-01A complete evidence | R3-02 expected matrix |
|---|---|---|---|---|---|
| `nai_you_new` | boundary=113, degenerate=1, components=10, self-check sampled | 1 adapter-degenerate operation；weld/flip/fill=0 | `blocked_incomplete_post_strict`；candidate rejected；repeatable | 236181 tested；8409 confirmed | rejected/manual；production blocked |
| `aishen_fudiao` | boundary=3, nonManifold=59, opposite=2, components=10, self-check sampled | 1 adapter-degenerate operation；weld/flip/fill=0 | `blocked_incomplete_post_strict`；candidate rejected；repeatable | 491365 tested；19270 confirmed；20 coplanar | rejected/manual；production blocked |
| `meigui_fudiao` | nonManifold=10940, opposite=7192, components=2, self-check sampled | no-op；weld/flip/fill=0 | `blocked_incomplete_post_strict`；candidate rejected；repeatable | 346104 tested；5592 confirmed | rejected/manual；production blocked |
| Texture2D 3MF | closed | no-op `strict_pass_no_repair` | all validator Gates PASS；candidate accepted；repeatable | 8 tested；0 confirmed/coplanar/touching | no-op strict PASS；non-production-only |

专项验收允许诚实的 manual required；12E-08D required-case Gate 不允许把 manual required 计为 PASS。
R3-01A 已证明三个真实 OBJ 均存在 confirmed self-intersection，不能计为 strict PASS，也不能进入当前保守
自动修复；R3-02 只负责冻结 no-op/repair/manual/rejected 矩阵。

R2-03 实际结果：`nai_you_new`/`aishen_fudiao` 因 sampled intersection evidence 保持 boundary 不变；
`meigui_fudiao` 和闭合 3MF 无 boundary，均不生成新面。generated simple planar no-UV hole 可 repaired strict
PASS；planarity/budget/UV/branching fixture 均稳定 blocked。

R2-04 实际结果：四个 required case 均完成两次稳定投影比较；operation/source/vertex/generated mapping 和属性
Gate 均通过。三个真实 OBJ 因 self-intersection 检查未完整而在 post-strict Gate 阻断，闭合 3MF 全 Gate
PASS；所有 case 均保持 `productionOutputWritten=false`。

R3-01 实际结果：`nai_you_new` 与闭合 3MF 无 non-manifold edge；`aishen_fudiao` 的 59 条 edge 分类为
2 duplicate exporter + 57 attribute conflict；`meigui_fudiao` 的 10940 条分类为 10935 duplicate exporter +
5 attribute conflict。全部 edge 有稳定 source/component 证据，0 个真实 case 满足 all-unique fan split。

R3-01A 实际结果：四个 required case 均完成两次完整、稳定的确定性 AABB BVH 分析；三个真实 OBJ 均为
confirmed intersection，闭合 3MF 为 complete no intersection；无 budget/resource blocked。

## 5. Attribute Matrix

| Attribute | No-op | Existing face repair | New face |
|---|---|---|---|
| material | exact unchanged | source mapping required | explicit policy required |
| UV | exact unchanged | per-corner mapping required | generated/fallback policy required |
| texture resource | valid | valid | valid or production blocked |
| component | unchanged | change explained | no implicit merge |
| triangle provenance | stable | mapped | operation-owned |

## 6. Negative Matrix

```text
repair disabled -> no mutation；
strict blocker + no eligible repair -> manual/blocked；
self-intersection -> fail-fast；
attribute conflict -> blocked；
post strict fail -> blocked；
repeat hash differs -> blocked；
budget exceeded -> blocked；
any blocked -> productionOutputWritten=false。
```

## 7. Protocol Matrix

```text
p0.rgbwsv.2 unchanged；
R G B W S V unchanged；
uint8 unchanged；
black_is_print unchanged；
OpenVDB optional/OFF；
legacy Profile output unchanged。
```

## 8. 08D Readiness

R3-04 只有全部 required case 为 strict PASS、属性 PASS、Release budget PASS、12E full closure PASS 和
legacy/RIP PASS 时输出 GO；否则输出 NO-GO 并保留具体 blocker。
