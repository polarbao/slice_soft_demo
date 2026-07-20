# PRD_12E-08C 真实模型拓扑修复与严格准入

> 文档版本：v0.1
> 文档状态：PRD / PREPARED
> 日期：2026-07-20
> 前置阶段：12E-08C Release Evidence
> 后续阶段：12E-08D Production Admission

## 1. 背景

真实 OBJ 可在 legacy 切片路径中被处理，但 12E 全局三维纹理/填充分区采用严格闭合网格作为
生产安全前提。当前三个真实模型因 boundary/non-manifold 问题未进入 12E 核心计算。

产品需要一种可解释的处理方式：能安全修复的模型自动修复并重新严格验证；不能安全修复的模型
给出明确原因和人工处理建议，而不是崩溃、静默降级或输出不可证明的生产包。

## 2. 用户与价值

目标用户：切片软件操作者、模型资产工程师、切片算法工程师和 QA。

用户价值：

```text
知道模型为什么不能进入全局纹理策略；
知道软件做了哪些修复；
知道 UV/材质/纹理是否被保持；
获得可重复的 strict PASS 或明确的人工修模建议；
避免错误几何进入实际打印。
```

## 3. 产品目标

1. 对最终姿态和变换后的 SceneModel 生成稳定拓扑诊断。
2. 将问题分为可自动修复、条件修复、人工修复和不可修复。
3. 提供默认关闭的显式 `repair_then_strict` 模式。
4. 记录修复前后哈希、操作、计数、阈值和属性变化。
5. 修复后重新运行 strict topology、纹理传递和 12E 全链路 Gate。
6. 不满足任一生产条件时不写 production package。

## 4. 范围

### 4.1 包含

```text
OBJ/MTL/Texture 和 3MF SceneModel；
最终 transform/autoOrient 后的网格；
boundary、non-manifold、duplicate/opposite duplicate、local winding、degenerate；
geometry/material/UV/texture provenance；
pre/post repair report 与 deterministic hash；
真实模型 Release 复测；
人工修复建议和阻断状态。
```

### 4.2 非目标

```text
不实现通用 CAD 修复器；
不保证任意损坏模型均可自动修复；
不自动修复 confirmed self-intersection；
不修改 RGBWSV 协议；
不把 OpenVDB 设为默认；
不在本专项直接启用 12E production Profile；
不修改支撑、白墨、光油的既有材料语义。
```

## 5. 用户故事

### US-01 拓扑阻断可解释

作为操作者，我需要看到稳定错误码、问题数量和位置摘要，以便判断是可自动修复还是需要修模。

### US-02 显式安全修复

作为操作者，我只能通过显式选项启用修复；普通 `strict_closed` 不应偷偷改变模型。

### US-03 属性保持

作为彩色模型用户，我需要修复后的 UV、材质和纹理归属保持一致，不能只让几何闭合却破坏颜色。

### US-04 生产准入可信

作为 QA，我需要 post-repair strict、Release budget、RIP 和协议证据全部通过后才能看到生产准入。

### US-05 复杂模型安全退出

作为资产工程师，当模型无法无歧义修复时，我需要 `manual_repair_required` 和具体建议，而不是伪 PASS。

## 6. 功能需求

### FR-01 诊断快照

报告必须记录输入标识、最终 transform、顶点/三角形/组件数量和所有稳定 topology issue。

### FR-02 修复资格

每类 issue 必须给出：

```text
eligible；
conditional；
manual_only；
fail_fast。
```

资格判断必须包含原因、阈值和建议动作。

### FR-03 显式模式

默认模式仍为 `strict_closed`。`repair_then_strict` 必须由独立 Profile 或高级配置显式启用，
且不得自动迁移旧 Profile。

### FR-04 确定性修复

相同输入、配置和构建版本必须产生相同 operation list、post-repair hash 和结果状态。

### FR-05 属性保护

必须验证 triangle material、per-corner UV、texture resource 和 component ownership。任何未知归属都会阻断生产。

### FR-06 修复后重新严格验证

修复后必须重新生成 topology diagnostics；只有 blocker 为零且 self-intersection 未确认时，才可进入
12E partition。

### FR-07 失败安全

超阈值、歧义、资源不足、属性丢失或修复后仍不闭合时，状态必须为 blocked/manual/failed，且不写包。

### FR-08 报告

输出独立 `mesh_repair_report.json`，不得把修复决定放进 report writer 内部临时判断。

### FR-09 Release 证据

修复时间与 12E core time 分开统计；TIFF/PNG/JSON 写盘时间不得混入核心预算。

### FR-10 Legacy 兼容

修复关闭时，旧 Profile 生产 TIFF 和现有行为必须不变。

## 7. 当前真实模型要求

| 模型 | 首要目标 | 可接受结果 |
|---|---|---|
| `nai_you_new` | 分类 boundary loops，评估 stitch/hole-fill | repaired strict PASS 或 manual required |
| `aishen_fudiao` | 分类少量 boundary 与局部 non-manifold | repaired strict PASS 或 manual required |
| `meigui_fudiao` | 识别大规模 non-manifold 来源和组件关系 | repaired strict PASS 或明确 manual required |
| Texture2D/ColorGroup 3MF fixture | 保证本来闭合的模型不被修改 | strict pass no repair |

“可接受结果”用于修复专项本身的诚实收口；12E-08D 仍要求既有 required-case matrix 全部满足生产 Gate，
除非后续正式决策调整矩阵。

## 8. 状态与错误

产品状态：

```text
not_evaluated
strict_pass_no_repair
repair_candidate
repaired_strict_pass
manual_repair_required
rejected_self_intersection
repair_failed
```

稳定错误码至少覆盖：输入无效、修复未启用、资格不足、预算超限、属性不一致、修复后 strict 失败、
self-intersection 和 hash 不确定。

## 9. 验收标准

1. generated positive/negative fixtures 均有稳定结果。
2. 修复关闭时 mesh/hash/TIFF 行为不变。
3. 修复开启时 pre/post hash、操作和属性映射完整。
4. repaired PASS 必须再次通过 strict，而不是仅“问题数量减少”。
5. confirmed self-intersection 始终 fail-fast。
6. 三个真实 OBJ 均生成可解释报告，不允许崩溃或空 PASS。
7. Release matrix、legacy regression、RIP strict 可重复运行。
8. 未满足生产 Gate 时 `productionOutputWritten=false`。

## 10. 完成定义

修复专项完成的最低定义是“所有 required input 都得到真实、稳定、可审计的结果”。它不承诺所有损坏模型
均自动修复。12E-08D 只有在 required production matrix 全部 strict PASS 且性能预算冻结后才能解锁。

## 11. 与双切片模式的边界

拓扑修复和 strict admission 是 `slicePipeline.mode=global_surface_shell` 的生产前置条件，不是 legacy
流水线的强制前置条件。选择 legacy 时保持当前模型导入、生产 TIFF 和 Profile 行为，不得为了 global
准入而自动修改 legacy 输入网格。

修复专项输出只决定 global 的 `blocked / diagnostic / admitted` 状态。即使 global 被阻断，legacy 仍可由
用户显式选择并独立运行；系统不得自动回退，也不得把 legacy 成功写包记作 global 成功。
