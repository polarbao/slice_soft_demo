# REPORT_16D_02 Qt 诊断与 A/B 预览当前状态

> 状态：COMPLETE
> 日期：2026-08-13

## 1. 当前实现

1. 参考宿主“切片设置”新增几何采样选择：S0 生产默认与 S3 诊断候选。
2. S3 继续只允许 `relief_heightfield` 纹理 Profile；错误组合沿用 16D-01 fail-closed。
3. 采样策略进入有效 Profile/hash，并随工作区 schema v5 持久化。
4. “切片作业”展示本次采样策略、P0/P3 姿态边界和 `supportStatisticsScanCount`。
5. “结果”并排展示首层 A 与当前层 B；RGBWSV 差值来自 manifest layer 统计，预览来自生产 TIFF 渲染。
6. 性能摘要只读取 Worker timing，不在 Qt 重算几何。

## 2. 默认与边界

- S0/P0 仍是生产默认。
- S3 只是显式诊断候选，不会因打开 UI 自动启用。
- P3 只展示“诊断未应用”，未执行姿态修改。
- SPI v1、11 个导出、15 项能力与 RGBWSV Package 协议未变化。

## 3. 验证

```text
Debug build:
  slicer_ui_host_sim
  hostflow_hb05_slice_settings_tests
  hostflow_hb08_workspace_state_tests

CTest filter:
  hostflow_hb05|hostflow_hb08|hostflow_he03|hostflow_he04|hostflow_he05

结果：10/10 PASS
```

## 4. 下一步

`16D-03` 执行统一回归 Gate。`16B-04` 和 `16D-05` 仍要求独立的产品授权，当前不得把 P3/S3 改为默认。
