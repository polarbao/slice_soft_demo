# PRD_08A_支撑桥接Fixture单测与真实模型Profile收口

> 文档版本：v0.1
> 适用阶段：08A
> 建议提交目录：`docs/slicer/`

## 1. 背景

08 已完成小组件过滤、xy dilation、简化 closing、水平/垂直 bridge gap、support_shape_report、support_shape_smoke、schema/golden/ci quick。

但当前 `support_shape_smoke` 主要证明 dilation/closing 生效，尚未让 `bridgedGaps` 在 golden 中稳定非零。

## 2. 产品目标

让支撑 bridge gap 行为有专用 fixture、专用 report 统计、专用 golden 回归和 C++ 单元测试守门。

## 3. 必须支持功能

### 3.1 Bridge Gap Fixture

新增：

```text
samples/configs/support/support_bridge_gap_smoke.json
```

要求：

```text
bridgeGapPx > 0
bridgedGaps.length > 0
addedSupportPixels > 0
support_shape_report.schema = p0.support_shape_report.1
```

### 3.2 C++ Unit Test Target

新增：

```text
support_shape_unit_tests
```

覆盖：

```text
SupportComponentAnalysis
SupportShapeOptimizer
```

### 3.3 Pipeline Wrapper 接入

将 support shape optimizer 从 `slicer.cpp` 进一步封装到正式 pipeline wrapper 或 support module facade。

### 3.4 Real 3MF Optional Profile

新增至少一个可选样例：

```text
samples/configs/3mf/three_mf_real_01_support_shape.json
```

不替换原真实模型配置，只作为 support shape profile 验证入口。

## 4. 验收标准

```text
support_bridge_gap_smoke 可生成 package；
bridgedGaps 在 report 中稳定非零；
bridge gap golden test 通过；
support_shape_unit_tests 通过；
run_support_shape_tests.ps1 覆盖 bridge fixture；
run_ci_quick.ps1 接入 unit test 和 bridge fixture；
真实 3MF support shape profile 至少 1 个通过；
原 quick regression 不受影响；
p0.rgbwsv.2 输出协议不变。
```
