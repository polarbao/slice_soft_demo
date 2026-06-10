# DOC_DECISION_08_R2后进入支撑形态与工艺优化阶段

> 文档版本：v0.1  
> 阶段：REPORT_R2 之后  
> 建议目录：`docs/slicer/`

## 1. 阶段判断

R2 已完成配置、报告、测试、CI 的第一轮工程化固化，并且 `run_ci_quick.ps1`、quick regression、schema tests、golden tests、UI self-test、overlay-load-real 均通过。

因此可以进入：

```text
08：支撑形态与工艺优化
```

## 2. 08 定位

08 是支撑形态优化阶段，不是协议重构阶段，也不是几何内核重写阶段。

目标：

```text
在不改变 RGBWSV 输出协议、不改变 Model > Support > Empty 优先级的前提下，
改进支撑 mask 的连通性、孤岛处理、边缘割裂诊断和工艺可制造性。
```

## 3. 08 必须解决的问题

```text
支撑小岛过滤
支撑碎片组件统计
支撑断裂 / 狭缝诊断
支撑膨胀 / 闭运算
小间隙桥接
支撑轮廓约束
support_shape_report
支撑 preview overlay 验证
golden summary 回归
```

## 4. 08 不做

```text
不修改 p0.rgbwsv.2
不改变 R G B W S V
不改变 8-bit / black_is_print
不让 SupportType 写入 TIFF channel
不实现 surface_shell_texture
不实现 compensated_varnish
不引入 OpenVDB / SDF
不做复杂树状支撑
不做设备通信
不做 RIP 半色调
不做 ICC / 色彩管理
```
