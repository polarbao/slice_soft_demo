# REPORT_16D-01 采样候选集成当前状态

> 状态：**COMPLETE / S3 EXPLICIT OPT-IN ONLY**
> 日期：2026-08-13

## 1. 当前实现

16D-01 已将 Stage 16 批准的几何采样合同贯通到 Reference Host、Worker materializer、
Scene Effective Config 和 Multi-model Production Service：

```text
S0 = legacy_center_sample
S3 = layer_slab_supersample_2x2_at_least_two_candidate
```

Reference Host 默认显式生成 S0；S3 只能由代码显式选择，并且仅允许与
`relief_heightfield` Profile 组合。策略进入 Profile 自哈希和 Scene Effective Config 哈希，
Production Service 会再次比对 Profile 与 effective contract，避免候选被篡改或静默回退。

## 2. Fail-closed 边界

以下输入会在进入生产写包前失败：

```text
S2、S4 或未知 geometrySampling.strategy；
S3 与 closed_mesh_scanline / 非 relief Profile 组合；
Profile 与 Scene Effective Config 的 geometrySamplingStrategy 不一致；
Host 枚举无法映射到冻结 Profile 标识。
```

本卡没有接入 P3 姿态候选；16B-04 仍需单独授权。Qt 本卡只透传 Profile 字段，不新增可见
A/B 控件，相关诊断展示属于 16D-02。

## 3. 本轮验证

```text
Debug 定向构建：
  multimodel_scene_contract_unit_tests
  multi_model_production_service_unit_tests
  stage14d08_r2_slice_materializer_tests
  hostflow_hb05_slice_settings_tests
  slicer_debug_ui
  slicer_ui_host_sim

定向 CTest：4/4 PASS
  stage14d08_r2_slice_materializer_tests
  multimodel_scene_contract_unit_tests
  multi_model_production_service_unit_tests
  hostflow_hb05_slice_settings
```

Host CTest 需要把构建目录加入本进程 `PATH`，以加载 Debug `tiffd.dll`；首次未设置依赖搜索
路径时模块加载失败，补齐 `PATH` 后通过。该现象不是 S3 逻辑失败。

## 4. 保持不变

```text
生产默认仍为 S0；
S3 仍是显式候选，不自动启用；
SPI v1、11 个 pm_* 导出、15 项能力不变；
p0.rgbwsv.2、R G B W S V、uint8、black_is_print 不变；
Legacy slicer_cli 和 Stage 14 Worker 生命周期边界不变。
```

## 5. 下一步

`16D-02` 可基于本卡冻结字段实现 Qt 诊断与 A/B 展示，不得在 UI 重算几何；`16C-03`
可独立开始支撑统计扫描融合。`16B-04` 仍需用户针对 P3 风险单独授权，不能与采样接入混合。
