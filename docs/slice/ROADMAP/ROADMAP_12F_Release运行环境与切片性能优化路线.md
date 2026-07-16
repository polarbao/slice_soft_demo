# ROADMAP_12F Release 运行环境与切片性能优化路线

> 文档状态：Roadmap
> 日期：2026-07-16

## R0 统一构建与 Runtime

状态：COMPLETE

```text
统一 Debug/Release build root；
统一 Debug/Release runtime root；
整合两个 Qt Debug launch；
windeployqt；
同目录 CLI/RIP reader；
Debug/Release self-test。
```

## R1 当前代码 Release Benchmark

状态：PLANNED / NOT ACTIVE

```text
刷新三真实模型；
5 次中位数；
拆分 model/slice/output；
确认 12D 当前代码加入后的热点变化。
```

## R2 支撑生成优化

状态：PLANNED / NOT ACTIVE

顺序：统计扫描融合 -> range candidate -> lazy materialization -> 缓存/并行。

## R3 Layer Compose 优化

状态：PLANNED / NOT ACTIVE

顺序：扫描融合 -> buffer 复用 -> policy 预解析 -> 可选并行。

## R4 Relief Occupancy Provider

状态：PLANNED / NOT ACTIVE

以 wrapper/adaptor 方式逐步取消 relief 完整 3D model mask 常驻，禁止一次性重写 production slicer。

## R5 增量切片、Preview I/O 与收口

状态：PLANNED / NOT ACTIVE

```text
geometry/support cache；
preview 按需或异步；
最终 Release end-to-end 矩阵；
性能、内存、确定性和回退结论；
REPORT_12F。
```

## 执行规则

```text
当前 12D 唯一代码执行入口不因本路线自动改变；
每次只激活一个 12F 原子任务；
R2-R5 属于 production core 改动，开始前必须读取当前 12D/12E 状态；
任何失败都回退到 legacy 当前实现和已验证 Runtime。
```
