# DOC_DECISION_12B_R0_R1_R2 切片引擎性能阶段拆分

> 文档状态：Decision
> 日期：2026-07-08
> 上游文档：PRD_12B_切片引擎性能与OpenVDB替代评估.md、DEV_12B_切片引擎性能与OpenVDB替代评估设计.md

## 1. 决策结论

12B 不作为一个一次性完成的大阶段推进，拆分为三个子阶段：

```text
12B-R0：Benchmark 契约、真实模型 Release core-only 对比、OpenVDB replacement gate 结论；
12B-R1：Legacy 优化和 2.5D heightfield fast path 小型原型；
12B-R2：OpenVDB hybrid/SDF utility 定位、外侧光油/clearance/诊断类能力接入评估。
```

当前优先执行 12B-R0。R1/R2 只有在 R0 形成可复现数据后才进入代码实现。

## 2. 拆分原因

12B 原始任务同时覆盖 benchmark、OpenVDB 替代判断、legacy 优化、heightfield fast path、OpenVDB hybrid 定位。若一次性推进，会出现以下风险：

```text
1. 没有 Release core-only 基线就开始优化，无法证明收益；
2. OpenVDB outputSemanticsComparable=false 时仍讨论替代性能，结论无效；
3. legacy 优化、heightfield、OpenVDB hybrid 三条线并行会扩大改动面；
4. benchmark 口径不稳定时，任何性能数字都不可长期引用。
```

因此必须先做 R0，把“怎么比、比什么、哪些结果可比较”固化。

## 3. R0 范围

R0 只做工程化 benchmark 和结论，不做生产切片算法替换。

包含：

```text
1. 固化 benchmark report schema；
2. 固化 same-pose / same-resolution / same-semantics 规则；
3. 建立真实模型 benchmark matrix；
4. 新增或扩展 run_12b_core_benchmark.ps1；
5. 输出 Release legacy 与 OpenVDB candidate core-only 对比；
6. 输出 outputSemanticsComparable=false 的具体原因；
7. 输出 OpenVDB replacement gate 结论。
```

不包含：

```text
1. 不改 RGBWSV 协议；
2. 不默认启用 OpenVDB；
3. 不优化 legacy 内核；
4. 不实现 heightfield fast path；
5. 不将 OpenVDB 写入 production 默认路径。
```

## 4. R1 准入条件

只有满足以下条件才进入 R1：

```text
1. R0 有至少 3 个真实模型 Release legacy core-only 数据；
2. 已确认 coreComputeMs 主要瓶颈位置；
3. 已确认 outputSemanticsComparable 口径；
4. 已列出至少一个低风险 legacy 优化点；
5. 有可复现 benchmark 脚本能对比优化前后。
```

R1 优先级：

```text
1. legacy z-bucket / active triangle filter；
2. tile/cache 或支撑投影缓存；
3. 2.5D heightfield admission 与 mask 差异验证；
4. 多线程/SIMD 仅在有明确瓶颈后进入。
```

## 5. R2 准入条件

只有满足以下条件才进入 R2：

```text
1. R0 明确 OpenVDB 不能直接作为默认 slicer engine，或只在特定能力上有优势；
2. 12A/12D 材料语义已稳定；
3. 需要 SDF offset / shell / clearance / complex topology diagnostic 类能力；
4. OpenVDB OFF 构建仍保持默认通过。
```

R2 定位：

```text
OpenVDB 优先作为 SDF utility engine，而不是默认 production slicing engine。
```

## 6. 当前行动

立即启动：

```text
docs/codex_task/current/TASKS_12B_R0_Benchmark契约与真实Release对比任务清单.md
```

12B 总任务清单保留为阶段总览，R0 任务清单作为当前执行入口。
