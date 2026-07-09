# ROADMAP_12B 切片引擎性能分阶段路线

> 文档状态：Roadmap
> 日期：2026-07-08

## Goal

把 12B 从“大而泛的性能阶段”拆成可执行、可验证、可停止的三段路线。

## Scope

```text
12B-R0：Benchmark contract + Release baseline + OpenVDB gate；
12B-R1：Legacy/heightfield 优化原型；
12B-R2：OpenVDB hybrid / SDF utility 定位。
```

## R0：Benchmark 契约与真实 Release 对比

目标：

```text
1. 复用现有 --benchmark-core-only；
2. 新增 run_12b_core_benchmark.ps1；
3. 输出 slicesoft.benchmark.12b.1；
4. 至少覆盖 model/obj 下 3 个真实模型；
5. 给出 OpenVDB replacement gate 结论。
```

退出标准：

```text
Release legacy core-only 数据存在；
OpenVDB candidate 可用时有同口径数据；
不可比时有 failureReasons；
形成 REPORT_12B_R0。
```

## R1：Legacy 优化与 Heightfield Fast Path 原型

目标：

```text
基于 R0 结果选择一个低风险优化方向。
```

候选：

```text
1. z-bucket / active triangle filter；
2. support projection cache；
3. texture sampling cache；
4. 2.5D heightfield top/bottom mask prototype。
```

退出标准：

```text
同一 benchmark case 下 Release coreComputeMs 有可解释变化；
输出语义可比；
不破坏默认 legacy production path。
```

## R2：OpenVDB Hybrid / SDF Utility 定位

状态：

```text
已开启；当前入口为 docs/codex_task/current/TASKS_12B_R2_OpenVDB_SDFUtility定位任务清单.md。
```

目标：

```text
判断 OpenVDB 是否只作为 SDF utility 使用。
```

候选能力：

```text
outer varnish shell offset；
surface clearance；
complex topology diagnostics；
material closure gap analysis support。
```

退出标准：

```text
OpenVDB OFF 构建不受影响；
OpenVDB ON 仅服务明确 utility；
不替代默认 production slicer engine。
```

## Rollback

```text
任何 R1/R2 优化失败时，保留 R0 benchmark 和报告；
默认生产路径回到 legacy slicer_cli --config；
OpenVDB 保持 optional experimental。
```
