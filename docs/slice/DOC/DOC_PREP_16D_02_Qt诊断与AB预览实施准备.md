# DOC_PREP_16D_02 Qt 诊断与 A/B 预览实施准备

> 阶段：Stage 16D-02
> 状态：PREPARATION_GATE=PASS
> 日期：2026-08-13

## 1. 任务边界

本卡只把 16D-01 已贯通的诊断数据展示到参考宿主，不新增几何算法，也不在 Qt 线程重算网格、姿态或支撑：

1. 在“切片设置”中显示并编辑 S0/S3 几何采样策略；S0 继续为生产默认。
2. 在“切片作业”中显示本次策略、P0/P3 姿态状态和 Worker 性能摘要。
3. 在“结果”中并排展示首层 A 与当前层 B，并显示 RGBWSV 打印像素差值。
4. 持久化宿主选择；未知策略必须 fail-safe，不能静默切换。

## 2. 冻结语义

| 项目 | 结论 |
|---|---|
| S0 | `legacy_center_sample`，默认且可用于全部现有生产 Profile |
| S3 | `layer_slab_supersample_2x2_at_least_two_candidate`，仅允许 `relief_heightfield` 纹理 Profile |
| P0 | 生产默认姿态；UI 只展示状态 |
| P3 | 已有诊断证据，但未获得 16B-04 默认/应用授权；本卡不得应用 |
| A/B | A=已校验生产包首层，B=当前层；预览来自 `package.render_preview`，差值来自 manifest layer 统计 |
| 性能 | 只展示 Worker 已返回 timing 与 `supportStatisticsScanCount`，不在 UI 推导算法耗时 |

## 3. 修改点

1. `HostSliceSettingsPanel`：新增带明确“生产默认/诊断候选”文案的采样下拉框。
2. `HostWorkspaceState`：新增 `geometrySamplingStrategy`，schema 由 4 升至 5。
3. `HostSliceJobPanel`：新增 Stage 16 策略/姿态摘要和支撑统计扫描次数。
4. `HostPackageReviewPanel`：新增首层与当前层双预览、逐通道差值及性能摘要。
5. `HostMainWindow`：只编排已有 Profile、Worker timing 和 package preview，不调用新几何接口。

## 4. 验收 Gate

1. 默认选择 S0；显式选择 S3 后有效 Profile/hash 同步更新。
2. S3 与非纹理/非 relief 组合继续 fail-closed。
3. 工作区重启后恢复合法策略；未知持久化值回退安全默认并拒绝脏状态。
4. 首层与当前层使用同一生产包、同一通道组合，不跨层兜底。
5. UI 显示 `supportStatisticsScanCount`；没有 telemetry 时显示“未提供”。
6. 既有 SPI v1、11 个导出、15 项能力、RGBWSV Package 与 S0/P0 默认不变。
