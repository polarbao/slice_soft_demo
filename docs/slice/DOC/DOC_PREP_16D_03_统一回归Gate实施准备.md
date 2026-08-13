# DOC_PREP_16D_03 统一回归 Gate 实施准备

> 阶段：Stage 16D-03
> 状态：PREPARATION_GATE=PASS
> 日期：2026-08-13

## 1. 目标

在不修改生产默认、不引入新语义的前提下，对 Stage 16 已完成的采样、姿态诊断、性能统计和宿主展示执行统一回归。该 Gate 只验证当前候选与历史生产边界，不授权 P3/S3 成为默认。

## 2. 固定验证矩阵

| 轨道 | 验证入口 | 通过条件 |
|---|---|---|
| Debug/Release 构建 | `build-slicesoft/main` 定向目标 | 两个配置均构建通过 |
| Stage 16 fixture | `stage16_*` CTest | sampling/posture/provider/矩阵全部 PASS |
| Reality 5/5 | `stage16_sampling_matrix`、`stage16_posture_matrix` | 五个 Reality 资产全部有机器可读结果 |
| Legacy 零漂移 | Quick CI、Golden、Stage 16 sampling matrix | S0 既有输出不漂移 |
| Stage 15 | `run_stage15_white_carrier_gate.ps1` | 补白、材料闭合和 RIP strict PASS |
| 13G | `run_13g_support_base_projection_tests.ps1` | 支撑连续与 30 层铺底合同 PASS |
| 13B/16C | `run_stage16_release_baseline.ps1` 的版本化证据 | 1/11/12/22 已完成矩阵可复核；设备 SLA 仍为 INPUT_OPEN |
| Package/RIP | full regression 和专项 Gate | `p0.rgbwsv.2`、RGBWSV、uint8、black_is_print 严格通过 |
| Stage 14 | Facade/SPI/Worker/cancel 定向 CTest | 冻结 ABI、Worker 和取消合同 PASS |
| Qt | HostFlow 设置、作业、结果、持久化测试 | S0/S3 可见、P0/P3 边界和 A/B 预览 PASS |
| Runtime | `PrepareSliceSoftRuntime.ps1` Debug/Release | 两个 Runtime 可发布并自检 |

## 3. 执行规则

1. 先运行定向 CTest，再运行 Quick CI 和 full regression，失败时保留原始日志并停止收口。
2. Quick CI 与子回归脚本必须支持显式 `BuildDir/Config`；默认参数继续兼容历史 `build` 入口，
   统一 Gate 使用 `build-slicesoft/main` 时必须在证据中写明，不能混淆两个构建身份。
3. Reality 和性能矩阵复用 16A-05、16B-03、16C-02 已生成的版本化证据；16D-03 不重复进行耗时基准，也不改写设备 SLA。
4. 外部 RIP、打印机和实物证据不在本 Gate 内，必须保持 pending。
5. 用户未授权 16B-04 和 16D-05，因此 P3 只诊断、S3 只显式候选，S0/P0 保持默认。

## 4. 失败处置

```text
构建失败 -> 记录实际目标、配置和日志，不切换构建轨道掩盖失败；
语义漂移 -> 阻断 16D-04，不允许刷新 Golden；
专项脚本缺失或陈旧 -> 先修复验证入口，再重跑；
外部证据缺失 -> 标记 EXTERNAL_PENDING，不伪造 PASS；
产品 SLA 未冻结 -> 16C-10 继续 INPUT_OPEN。
```

## 5. 准备结论

16D-03 的矩阵、入口、通过条件和失败处置已冻结，`PREPARATION_GATE=PASS`，可进入分层执行。16D-03 通过后只解锁 16D-04 阶段报告，不自动解锁 16B-04 或 16D-05。
