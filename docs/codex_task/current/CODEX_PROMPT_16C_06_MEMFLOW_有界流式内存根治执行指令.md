# CODEX_PROMPT_16C-06-MEMFLOW 有界流式内存根治执行指令

> 文档状态：**ACTIVE**
> 版本：v1.0 ｜ 日期：2026-08-18
> 任务真源：`TASKS_16C_06_MEMFLOW_有界流式内存根治专项任务清单.md`

## 1. 开工规则

```text
只执行任务真源中当前 IN PROGRESS 的一张卡；
开工前 git status --short，区分并行改动；
先读 Decision/DEV/PREP 和当前源文件；
完成后更新本专项任务状态、完成日期、实际验证和修订记录；
未运行的 Gate 不得写 PASS。
```

用户已授权从准备进入开发，但这不等于一次性授权改变生产默认、Sparse 自动启用、并行化或设备
SLA 结论。

## 2. 永久红线

```text
PM_SPI_VERSION、11 pm_* 导出、15 capabilities 不变；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print、Model > Support > Empty 不变；
Legacy/S0/P0 默认不变；
OpenVDB OFF；
Report/Host 不决定业务路由；
staging 未经严格 Reader 不得发布；
工作中发现并行改动时不回退、不覆盖。
```

## 3. 执行顺序

```text
MF-01 Budget contract
MF-02 Owned layer contract
MF-03 Bounded occupancy/support
MF-04 Single-instance streaming package
MF-05 Multi-instance barrier
MF-06 Sparse candidate
MF-07 Adaptive production route
MF-08 Closure
```

不得把 MF-06 偷渡进 MF-03，也不得跳过单线程生命周期优化直接进入 16C-09 并行。

## 4. 每卡验证

```text
L1 当前模块 Unit/Contract
L2 受影响路径 Golden/逐层 hash
L3 Package/manifest/report/preview
L4 RIP strict 与坏包/取消/恢复
L5 Runtime self-test（仅生产接入卡）
L6 Release A/B（仅有完全相同请求时）
```

性能结论必须同时报告参数、构建身份、重复次数和输出一致性。不同 DPI、层厚、Profile、压缩或
系统缓存的结果不得作为直接 before/after。

## 5. 停止条件

```text
逐层 RGBWSV/ownership/hash 漂移；
支撑类型、铺底层范围、材料闭合或 Stage 15 语义漂移；
取消后发布半包；
多实例结果依赖完成顺序；
预算器溢出或低估被当作峰值保证；
需要修改冻结 SPI/Worker/p0 合同；
发现当前任务依赖未授权的 16B-04/16D-05/16C-09。
```

命中后保留 Retained Dense，记录证据并停止扩大接入。
