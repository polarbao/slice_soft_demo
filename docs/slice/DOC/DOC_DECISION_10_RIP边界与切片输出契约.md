# DOC_DECISION_10_RIP边界与切片输出契约

> 文档版本：v0.1
> 文档状态：DOC_DECISION / Stage 10
> 生成日期：2026-07-01

---

## 1. 决策

10 阶段不实现 RIP、设备通信、喷头 bitstream 或 RIP 半色调。

10 阶段只负责：

```text
定义稳定切片输出契约；
保证纹理 / UV / 材质 / 支撑 / 白墨 / 光油信息足够下游消费；
输出可验证 report / manifest / layer summary；
提供下游 handoff checklist。
```

---

## 2. 理由

RIP 处理由专门工程师负责。SliceSoft 当前主线应该避免把切片问题、RIP 问题和设备问题混在一起。

清晰边界能让后续联调变成：

```text
SliceSoft 提供稳定输入；
RIP 团队定义消费反馈；
双方通过输出契约和样例包对齐。
```

---

## 3. 允许事项

```text
分析下游 RIP 库或源码对输入格式的需求；
把下游需要的字段补入 output contract；
生成 handoff package；
提供 sample package 和 expected summary；
记录下游反馈。
```

---

## 4. 禁止事项

```text
不把 RIP SDK 作为 slicer_core 依赖；
不在 slicer_core 中实现半色调；
不生成设备 bitstream；
不修改 p0.rgbwsv.2；
不改变 RGBWSV channel order；
不把下游验收等同于真实打印验收。
```

