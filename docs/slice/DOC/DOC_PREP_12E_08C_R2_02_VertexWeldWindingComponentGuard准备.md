# DOC_PREP_12E-08C-R2-02 Vertex Weld、Winding 与组件守门准备

> 文档状态：PREPARED / R2-02 READY
> 日期：2026-07-20
> 前置：12E-08C-R2-01 COMPLETE

## 1. 目标

在 R2-01 隔离 candidate 上实现两类仍具唯一解的操作：受约束 vertex weld 和组件内唯一 winding 传播。
任何可能合并组件、改变材质/UV 归属或存在多解的情况必须保持 `manual_repair_required`。

## 2. Vertex Weld 契约

```text
阈值必须来自 MeshRepairOptions.weldToleranceMm 和 MeshScaleTolerance；
默认 0 表示禁用；不得硬编码模型专用距离；
候选按量化位置和稳定 source vertex id 排序；
禁止跨 connected component 合并；
禁止产生退化面、duplicate/opposite duplicate 或 non-manifold；
禁止让一个 per-corner UV/material 三角形失去 source provenance；
每个输出 vertex 记录有序 source vertex set。
```

## 3. Winding 契约

只允许在单个连通组件内根据共享边邻接图得到唯一方向时翻转。翻转三角形必须同步交换 vertex corner 和 UV
corner。冲突环、零体积组件、多解组件和 sampled/confirmed self-intersection 均不得自动修复。

## 4. 组件守门

R1 baseline 的组件数为 `10 / 10 / 2 / 1`。R2-02 前后组件数量必须相同；任何 weld 计划连接原先不同组件时，
在执行前整体阻断。不得为了减少 boundary 数量隐式 merge 甲片壳、浮雕或装饰组件。

## 5. DTO 与报告准备

R2-02 允许在现有契约上补充：

```text
source vertex -> output vertex mapping；
weld group 的阈值、source ids 和 output id；
flip operation 的 source/output triangle id；
component count before/after；
material/UV exact preservation 结果。
```

新增字段必须进入 deterministic operation hash，计时不得进入 hash。

## 6. 测试矩阵

```text
同组件、阈值内、不会退化的 weld：PASS；
跨组件近邻点：blocked，组件数不变；
weld 后退化/non-manifold：blocked；
唯一 winding fixture：翻转 vertex/UV corner 后 post strict PASS；
冲突 winding 环：manual required；
clean 3MF：no-op strict PASS；
相同输入两次：operation/mapping/post hash 一致。
```

## 7. 实施模块

优先扩展 `MeshRepairService` 的有序 operation set，必要时拆出 `RepairOperations.*`；不复制 mesh DTO，
不接入 Qt/TIFF/pipeline router。R2-02 仍只生成诊断 candidate 和 report。

## 8. 退出标准

generated positive fixture post strict PASS；跨组件和歧义 fixture 稳定 blocked；属性与组件守门通过；默认 OFF
构建和 legacy 回归不变。完成后才允许启动 R2-03 Boundary Loop Repair。
