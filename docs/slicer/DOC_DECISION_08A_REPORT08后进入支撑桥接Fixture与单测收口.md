# DOC_DECISION_08A_REPORT08后进入支撑桥接Fixture与单测收口

> 文档版本：v0.1
> 适用阶段：REPORT_08 之后
> 建议提交目录：`docs/slicer/`

## 1. 阶段判断

`REPORT_08_支撑形态与工艺优化当前状态.md` 显示，08 已完成支撑形态优化第一版：

```text
SupportShapePolicy
SupportComponentAnalysis
SupportShapeOptimizer
SupportShapeReport
support_shape_smoke
support_shape_report.json
schema/golden/ci quick 接入
run_ci_quick.ps1 通过
p0.rgbwsv.2 输出协议不变
```

08 可以收口为“支撑形态优化第一版”。

## 2. 为什么不直接进入 09

08 仍存在以下限制：

```text
closing 是轻量简化版；
bridge gap 只支持水平/垂直短间隙；
当前 sample 主要验证 dilation/closing；
bridge gap 有代码和 report 支持，但尚未单独新增专用 fixture；
SupportShapeOptimizer 尚未完全进入正式 pipeline wrapper；
SupportComponentAnalysis / SupportShapeOptimizer 尚未形成 C++ unit test target；
真实 01/02/03 3MF 还没有可选 support shape profile。
```

因此建议先进入：

```text
08A：支撑桥接 Fixture、单元测试与真实模型 Profile 收口
```

## 3. 08A 阶段定位

08A 不是新的大算法阶段，而是 08 的工程验证收口：

```text
补 bridge gap 专用 fixture；
补 support shape C++ 单元测试；
把 optimizer 进一步封装到 support/pipeline wrapper；
新增真实 3MF support shape 可选 profile。
```

## 4. 08A 不做

```text
不修改 p0.rgbwsv.2；
不让 SupportType 写入 TIFF channel；
不实现 surface_shell_texture；
不实现 compensated_varnish；
不引入 OpenVDB / SDF；
不做树状支撑；
不做设备通信；
不做 RIP 半色调。
```

## 5. 完成后路线

08A 完成后，如果 bridge fixture、unit test、CI quick 都稳定通过，可以进入：

```text
09：OpenVDB / SDF 几何内核预研
```
