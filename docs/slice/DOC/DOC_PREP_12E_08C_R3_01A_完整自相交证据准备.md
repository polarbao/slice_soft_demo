# DOC_PREP_12E-08C-R3-01A 完整自相交证据准备

> 文档状态：PREPARED / EXECUTION BLOCKED BY R2 AND R3-01
> 日期：2026-07-20
> 来源：12E-08C-R1-04 真实模型 baseline

## 1. 准备原因

三个 required OBJ 的当前 `MeshRobustnessDiagnostics` 都因 `max_triangle_pair_checks=250000` 进入采样模式。
Eligibility 正确输出 `MESH_SELF_INTERSECTION_SAMPLED/manual_only`，但这会使任何真实模型 post-strict
结果都无法成为生产准入证据。采样结果既不能证明存在自相交，也不能证明不存在自相交。

## 2. 原子任务目标

在不引入第三方库、不执行 repair、不写 production package 的前提下，为最终姿态后的大网格建立确定性、
完整的自相交候选 broad-phase，并复用现有 `TestTriangleIntersection` 语义完成 narrow-phase。

输出必须区分：

```text
complete_no_intersection；
confirmed_intersection；
coplanar_overlap；
touching_only；
budget_or_resource_blocked。
```

只有完整枚举完成且 confirmed/coplanar blocker 为零，才允许清除
`MESH_SELF_INTERSECTION_SAMPLED`。确认相交仍保持 fail-fast。

## 3. 设计约束

```text
使用稳定 triangle index 排序构建 AABB BVH 或等价确定性空间索引；
候选 pair 固定按 (minTriangleId, maxTriangleId) 排序和去重；
共享顶点邻接 pair 继续排除；
不能以随机采样或运行时遍历顺序决定结果；
计时和 peak memory 单列，不进入 hash；
超内存/超预算时输出稳定 blocked，不能降级为 PASS。
```

首选项目内确定性 AABB BVH。全量 O(N^2) 仅用于小 fixture 对照，不作为真实模型方案；OpenVDB 体素化不能
替代三角面自相交证据，因为会改变几何与属性语义。

## 4. 测试矩阵

```text
小型闭合无相交 fixture：complete_no_intersection；
已知穿透 fixture：confirmed_intersection；
共面重叠 fixture：coplanar_overlap；
仅接触 fixture：touching_only；
大规模无相交 fixture：不进入 sampled；
相同输入三次：候选计数、pair hash、分类完全一致；
三个真实 OBJ：均获得 complete 或稳定 budget blocked，不允许 sampled 冒充 complete。
```

## 5. 退出标准

```text
候选 broad-phase 对小 fixture 与 O(N^2) 结果一致；
required real model 不再依赖 250000 pair 采样结论；
confirmed self-intersection fail-fast 不变；
报告记录 complete、candidate count、tested count、pair hash、时间和内存；
repairAttempted=false；productionOutputWritten=false。
```

## 6. 阶段位置

执行顺序为：

```text
R2-01..04
  -> R3-01 Non-Manifold Pattern Classifier
  -> R3-01A Complete Self-Intersection Evidence
  -> R3-02 Real Model Repair Matrix
```

该任务只补齐 strict 证据，不放宽 required-case Gate，也不授权 12E-08D。
