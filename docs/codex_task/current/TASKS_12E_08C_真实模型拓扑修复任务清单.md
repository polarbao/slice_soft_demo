# TASKS_12E-08C 真实模型拓扑修复任务清单

> 文档状态：IN PROGRESS / R1 COMPLETE / R2-01 COMPLETE / R2-02 READY
> 日期：2026-07-20
> 阶段位置：12E-08C 与 12E-08D 之间
> 当前原子任务：无；下一允许任务为 12E-08C-R2-02

## 1. 总目标

建立显式、确定、属性保持的 `repair_then_strict` 前置链路，使真实模型获得可审计的 strict PASS 或明确的
人工修复结论。不得为了开启 12E-08D 放宽生产安全门禁。

## 2. 全局规则

```text
每次只执行用户明确指定的一个原子任务；
任务开始前检查 git status --short；
修复默认关闭；
confirmed self-intersection fail-fast；
warn_and_attempt 不得 production；
R1/R2/R3 不写 12E production package；
OpenVDB optional/OFF；
协议和 legacy Profile 不变；
完成验证后更新任务状态；提交仅按用户明确要求或活动任务指令执行。
```

## 3. R1 Contract & Eligibility

### 12E-08C-R1-01 DTO、错误码、Hash 与 Report Skeleton

状态：COMPLETE。

范围：

```text
MeshRepairOptions/Eligibility/Operation/Result DTO；
稳定 status 和 E_12E_REPAIR_* 错误码；
canonical geometry/attribute/options/operation hash；
slicesoft.mesh_repair.12e_08c.1 report skeleton；
unit/schema tests。
```

完成标准：相同输入 hash 稳定；geometry/attribute 修改可区分；不执行 repair；不写 package。

实际结果：DTO、稳定错误码、`mesh_repair_canonical.1` SHA-256、report skeleton、unit/golden 已实现；
定向测试 1/1、默认 Debug CTest 22/22 PASS。结果见
`docs/slice/DOC/DOC_EXEC_12E_08C_R1_01_MeshRepairContract结果.md`。

### 12E-08C-R1-02 Eligibility Policy

状态：COMPLETE。

范围：复用现有 topology/robustness diagnostics，分类 eligible/conditional/manual/fail-fast；输出稳定建议。

完成标准：所有已知 issue 有唯一分类；self-intersection fail-fast；无 silent fallback。

准备入口：`docs/slice/DOC/DOC_PREP_12E_08C_R1_EligibilityFixtureBaseline准备.md`。

实际结果：新增纯 `MeshRepairEligibilityPolicy`，复用 topology/robustness diagnostics，固定
`fail_fast > manual_only > conditional > eligible` 优先级；定向测试 2/2、默认 Debug CTest 23/23 PASS。
结果见 `docs/slice/DOC/DOC_EXEC_12E_08C_R1_02_EligibilityPolicy结果.md`。

### 12E-08C-R1-03 Generated Fixtures 与 Golden

状态：COMPLETE。

范围：clean、duplicate、winding、boundary、non-manifold、self-intersection、attribute conflict fixtures。

完成标准：状态、code、hash 和 report golden 可重复。

实际结果：新增 11 个 generated policy-contract fixtures，冻结 `slicesoft.mesh_repair_fixture_golden.12e_08c_r1.1`
golden；geometry/attribute hash、status、classification、reasonCode、affectedCount 与非生产标志重复稳定。
结果见 `docs/slice/DOC/DOC_EXEC_12E_08C_R1_03_GeneratedFixtureGolden结果.md`。

### 12E-08C-R1-04 真实模型 Pre-Repair Baseline

状态：COMPLETE。

范围：`nai_you_new`、`aishen_fudiao`、`meigui_fudiao` 和闭合 3MF；记录 issue pattern、hash、eligibility。

完成标准：四个 case 均有可审计结果；不要求自动修复；R2 范围据此复核。

实际结果：新增只读 Preflight service/CLI/脚本，四个 case 均执行两次并通过稳定证据比较；三个真实 OBJ
为 `manual_repair_required`，闭合 Texture2D 3MF 为 `strict_pass_no_repair`。R2-01 范围已复核，且因真实
OBJ 自相交诊断进入 sampled 模式，新增 R3-01A 完整自相交证据准备。结果见
`docs/slice/DOC/DOC_EXEC_12E_08C_R1_04_真实模型PreRepairBaseline结果.md`。

## 4. R2 Conservative Repair

### 12E-08C-R2-01 Degenerate/Duplicate Cleanup

状态：COMPLETE。

范围：显式退化面清理、同属性 exact duplicate 去重和 source mapping。

完成标准：generated fixtures post strict PASS；属性冲突稳定 blocked。

准备入口：`docs/slice/DOC/DOC_PREP_12E_08C_R2_ConservativeRepair准备.md`。

实际结果：新增显式 cleanup service、adapter rejected-degenerate provenance、`sourceMappings[]`、CLI/脚本和
generated/real-model 验证。同属性同向 exact duplicate 可清理；属性冲突、opposite duplicate 和 confirmed
self-intersection 保持阻断。结果见 `docs/slice/DOC/DOC_EXEC_12E_08C_R2_01_保守清理结果.md`。

### 12E-08C-R2-02 Vertex Weld、Winding 与组件守门

状态：READY。

范围：受约束顶点焊接、唯一 local winding 传播、组件不隐式 merge。

完成标准：阈值和受影响元素可报告；歧义 case manual required。

准备入口：`docs/slice/DOC/DOC_PREP_12E_08C_R2_02_VertexWeldWindingComponentGuard准备.md`。

### 12E-08C-R2-03 Boundary Loop Repair

状态：BLOCKED BY R2-02。

范围：简单闭环分类、stitch/hole-fill、new-face attribute/fallback policy。

完成标准：简单 fixture post strict PASS；非平面/超预算/属性未知 case blocked。

### 12E-08C-R2-04 Post-Repair Strict 与 Attribute Guard

状态：BLOCKED BY R2-03。

范围：统一 post diagnostics、attribute validator、operation/hash repeatability 和 negative gate。

完成标准：任何 repaired PASS 均由重新 strict 得出；修复失败丢弃 candidate mesh。

## 5. R3 Real Model & Release Gate

### 12E-08C-R3-01 Non-Manifold Pattern Classifier

状态：BLOCKED BY R2。

范围：分类 edge fan、重复壳、重叠组件和导出器重复模式；只评估唯一局部 fan split。

完成标准：`meigui_fudiao` 不进行无模式批量修复；所有 pattern 有稳定结果。

准备入口：`docs/slice/DOC/DOC_PREP_12E_08C_R3_RealModelReleaseGate准备.md`。

### 12E-08C-R3-01A 完整自相交证据

状态：BLOCKED BY R2 AND R3-01。

范围：用确定性空间索引完整枚举 required real model 自相交候选，复用当前 narrow-phase，替代 sampled
证据；confirmed intersection 继续 fail-fast。

完成标准：小 fixture 与 O(N^2) 对照一致；真实模型输出 complete 或稳定 budget blocked；不得把 sampled
计为 post-strict PASS。

准备入口：`docs/slice/DOC/DOC_PREP_12E_08C_R3_01A_完整自相交证据准备.md`。

### 12E-08C-R3-02 真实模型 Repair Matrix

状态：BLOCKED BY R3-01A。

范围：四个 required cases 的 no-op/repair/manual 状态、属性保持和 post strict。

完成标准：不崩溃、不伪 PASS；required-case production PASS 与专项完成状态分开记录。

### 12E-08C-R3-03 Release Core 与 Legacy Regression

状态：BLOCKED BY R3-02。

范围：修复、partition、texture transfer、raster/full closure 的分段计时和 peak memory；旧 Profile/RIP/TIFF 回归。

完成标准：写盘时间排除；预算可冻结或明确 BLOCKED；默认 OFF lane PASS。

### 12E-08C-R3-04 12E-08D GO/NO-GO

状态：BLOCKED BY R3-03。

范围：更新 admission matrix、正式报告和上下文，给出 08D 决策。

完成标准：只有 required cases 与所有生产 Gate PASS 才输出 GO，否则保留 blocker 和 NO-GO。

## 6. 计划验证层级

```text
R1：unit/schema/golden；
R2：generated repair + attribute preservation + post strict；
R3：real model Release + 12E pipeline + legacy/RIP/TIFF invariant；
每个任务：git diff --check。
```

具体命令以任务实施时实际创建的 target/script 为准，不把计划入口记录为已运行。

## 7. 12E-08D 关系

12E-08D 当前继续 BLOCKED。R3-04 是唯一可把状态变为 READY 的前置任务；R1/R2 单独完成不构成生产准入。

## 8. 双模式边界

本清单只处理 `global_surface_shell` 流水线的修复与 strict admission。legacy 保持默认生产路径且不自动
调用 repair；global 失败时不得切换到 legacy。修复任务不写 TIFF，统一生产 writer 接入由后续
12E-08D-01..04 完成。
