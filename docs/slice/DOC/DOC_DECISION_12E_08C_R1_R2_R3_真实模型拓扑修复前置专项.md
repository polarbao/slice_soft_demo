# DOC_DECISION_12E-08C-R1/R2/R3 真实模型拓扑修复前置专项

> 文档状态：Accepted / PREPARED
> 日期：2026-07-20
> 插入位置：12E-08C 之后、12E-08D 之前
> 上游依据：`DOC_DECISION_09P_R2_mesh_repair_admission_gate.md`、`DOC_PREP_12E_R4_ProductionAdmission准备.md`

## 1. Context

12E-08C 已完成默认 OpenVDB OFF 的 Release 证据脚本、旧 Profile 回归、RIP strict 和 TIFF
不变性验证，但三个真实 OBJ 均被 `strict_closed` 拓扑门禁阻断：

| 模型 | 当前阻断 |
|---|---|
| `model/obj/nai_you_new` | boundary edges = 113，degenerate triangles = 1 |
| `model/obj/aishen_fudiao` | boundary edges = 3，non-manifold edges = 59，degenerate triangles = 1 |
| `model/obj/meigui_fudiao` | non-manifold edges = 10,940 |

当前代码没有 Mesh Repair 服务。`repair_then_strict` 只是 admission 占位，始终返回
`non_production_only`。因此无法取得真实 OBJ 的 occupancy、distance、partition、Release 时间和峰值内存，
也不能进入 12E-08D production package。

## 2. Decision

在 12E-08D 前插入独立的真实模型拓扑修复专项：

```text
12E-08C-R1：拓扑分类、修复资格、哈希与报告契约；
12E-08C-R2：确定性、属性保持的保守 Mesh Repair；
12E-08C-R3：真实模型 post-repair strict、Release 预算与准入复核；
12E-08D：仅在全部生产 Gate 关闭后执行。
```

修复必须是显式阶段，不得成为 `strict_closed` 的隐式副作用。修复后必须重新执行完整诊断，
不得复用修复前的 topology/admission 结果。

## 3. Stable Outcome

每个输入最终只能进入以下稳定状态之一：

```text
strict_pass_no_repair
repaired_strict_pass
manual_repair_required
rejected_self_intersection
repair_failed
diagnostic_only
```

专项完成不等于所有模型都能自动修复。无法无歧义修复的真实模型必须保留
`manual_repair_required`，不得为满足进度强行生成 PASS。

12E-08D 的既有 required-case matrix 不因本决策自动缩减。若任一 required case 未达到
`strict_pass_no_repair` 或 `repaired_strict_pass`，12E-08D 继续 BLOCKED；如需调整 required cases，
必须另建产品/准入决策。

## 4. Repair Boundary

允许评估：

```text
退化面显式清理；
同属性精确重复面去重；
可证明一致的局部绕序修正；
受阈值约束的几何顶点焊接；
简单边界环 stitch/hole-fill；
可解释的局部 non-manifold fan 分解。
```

首版禁止：

```text
confirmed self-intersection 自动修复；
隐式 boolean union；
多组件静默合并；
丢弃 UV/材质/纹理归属后继续生产；
OpenVDB 体素重建后冒充原始纹理网格；
warn_and_attempt production admission；
无阈值、无操作清单的“一键修复”。
```

## 5. Attribute Preservation

修复必须分别证明：

```text
几何拓扑可追溯；
triangle source/material index 可追溯；
per-corner UV 与 texture resource 可追溯；
组件数量变化可解释；
新增面具有明确 attribute/fallback policy；
修复后 Texture Transfer 不得产生未知来源颜色。
```

如果 hole-fill 新增面无法获得可证明的纹理属性，只能标记为 diagnostic 或使用显式、可审计的
fallback 策略；不能静默使用任意 RGB 后进入生产。

## 6. Production Gate

`repair_then_strict` 只有同时满足以下条件才可成为 12E-08D 输入：

```text
repair Profile 显式启用；
pre/post diagnostics 与 hash 完整；
operation list 和受影响元素计数完整；
confirmed self-intersection = 0；
post-repair strict topology PASS；
attribute preservation PASS；
12E partition/texture transfer/raster/full closure PASS；
Release 时间和内存预算 PASS；
legacy regression、RIP strict 和协议不变性 PASS。
```

## 7. Alternatives Considered

| 方案 | 结论 | 原因 |
|---|---|---|
| 放宽 `strict_closed` | 拒绝 | 会把开放边界和非流形输入误标为生产安全 |
| `warn_and_attempt` 写包 | 拒绝 | 违反既有 Production Safety Rules |
| `strict_closed` 内隐式修复 | 拒绝 | 无法审计修复前后差异和回滚 |
| OpenVDB 体素重建统一修复 | 拒绝作为首版 | 无法自然保持 OBJ/3MF UV、材质和 triangle provenance |
| 只要求用户外部修模 | 不作为唯一方案 | 缺少统一诊断、报告和可重复准入，但仍保留为复杂模型回退 |
| 显式保守修复后重新 strict | 采用 | 可解释、可回归，并保持生产门禁保守 |

首版不新增第三方依赖。若 R2/R3 证明项目内保守算法不足，应另建依赖 ADR，至少比较两个候选的
CMake/vcpkg、许可证、属性保持和维护风险后再决定。

## 8. Consequences

正向影响：

```text
真实模型阻断原因可机器读取；
修复前后变化可追溯；
简单问题可自动处理；
复杂模型不会被虚假准入；
12E-08D 获得明确、可重复的前置证据。
```

代价：

```text
12E-08D 延后；
需要新增 geometry/diagnostics/report/test 能力；
复杂 non-manifold 模型可能仍需资产侧修复；
Release matrix 必须重新运行。
```

## 9. Validation

本决策通过任务清单、Schema、验收矩阵和后续代码测试验证。无论专项结果如何，都必须保持：

```text
p0.rgbwsv.2；R G B W S V；uint8；black_is_print；
OpenVDB optional/OFF；legacy production path 不替代；
未通过 admission 时不写 12E production package。
```

## 10. Follow-up

执行入口：

```text
docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md
docs/codex_task/current/CODEX_PROMPT_12E_08C_真实模型拓扑修复执行指令.md
```

## 11. 双模式补充决策

本修复专项只解除 `global_surface_shell` 的生产准入阻断，不替代或重写 legacy。legacy 保持默认可用；
global 必须显式选择并通过 repair/post-strict、属性保持和 Release Gate。两条模式未来共享生产 TIFF writer，
但修复专项本身仍不写 TIFF，也不允许失败时静默切换流水线。
