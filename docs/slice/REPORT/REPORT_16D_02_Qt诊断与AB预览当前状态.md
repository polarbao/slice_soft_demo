# REPORT_16D_02 Qt 诊断与 A/B 预览当前状态

> 状态：COMPLETE
> 日期：2026-08-13
> 2026-08-14 补充：用户裁定取消结果页双预览；16D-02-R1 扩展单材料 W/V 浮雕的 S3 显式适用范围。

## 1. 当前实现

1. 参考宿主“切片设置”新增几何采样选择：S0 生产默认与 S3 诊断候选。
2. S3 继续只允许 `relief_heightfield`；可用于彩色纹理或单材料 W/V 浮雕，普通无纹理 RGB
   组合沿用 16D-01 fail-closed。
3. 采样策略进入有效 Profile/hash，并随工作区 schema v5 持久化。
4. “切片作业”展示本次采样策略、P0/P3 姿态边界和 `supportStatisticsScanCount`。
5. “结果”只展示当前生产层；通道像素统计来自当前 manifest layer，预览来自生产 TIFF 渲染。**默认预览模式已于 MV-07C 由 RGBWSV 六通道组合改为 RGB-only 判读入口**，因为六通道组合会把 S 叠成纯绿伪彩色而易被误读为 RGB 材质色；六通道组合仍可显式选择。
6. 性能摘要只读取 Worker timing，不在 Qt 重算几何。

## 2. 默认与边界

- S0/P0 仍是生产默认。
- S3 只是显式诊断候选，不会因打开 UI 自动启用。
- 单材料白墨 W/光油 V 选择 S3 时只改变几何采样链路，`texture.enabled` 仍为 false。
- P3 只展示“诊断未应用”，未执行姿态修改。
- 首层/当前层双预览属于已被用户裁定替代的历史呈现，不再额外渲染首层。
- SPI v1、11 个导出、15 项能力与 RGBWSV Package 协议未变化。

## 3. 验证

```text
Debug build:
  slicer_ui_host_sim
  hostflow_hb05_slice_settings_tests
  hostflow_hb08_workspace_state_tests

CTest filter:
  hostflow_hb05|hostflow_hb08|hostflow_he03|hostflow_he04|hostflow_he05

原始 16D-02：10/10 PASS

2026-08-14 补充验证：
  hostflow_hb05_slice_settings
  hostflow_hb05_settings_ui_smoke
  hostflow_hb07_package_review
  hostflow_hb07_result_ui_smoke
结果：4/4 PASS；运行时 self-test 与结果 UI smoke PASS

2026-08-14 单材料浮雕 S3 修订：
  Release build: hostflow_hb05_slice_settings_tests / slicer_ui_host_sim / stage16_geometry_sampling_fixture_tests
  CTest: hostflow_hb05_slice_settings / hostflow_hb05_settings_ui_smoke / stage16_geometry_sampling_fixture_tests
  结果：3/3 PASS
  单材料光油 S3 Package：306 x 718 x 192，RIP strict PASS
  Release Runtime：发布完成，HOSTFLOW_HB05_UI_PASS
```

## 4. 下一步

`16D-03` 执行统一回归 Gate。`16B-04` 和 `16D-05` 仍要求独立的产品授权，当前不得把 P3/S3 改为默认。
