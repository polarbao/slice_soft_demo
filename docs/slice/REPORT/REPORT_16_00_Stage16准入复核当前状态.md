# REPORT_16-00 Stage 16 准入复核当前状态

> 状态：**COMPLETE / PARTIAL GO**
> 版本：v1.0 | 日期：2026-08-12
> 对应任务：`16-00-01..04`
> 基线身份：`5333bc10b3218eae2d6196a2fc4a8cfd46f115be`

## Current State

R-F-01、R-F-02 已独立完成并提交，工作树在 16-00-01 开始时干净。Stage 16 的问题边界、候选策略、停止条件和验证层级已经准备完成。本次准入只冻结审计结论，不修改切片代码、生产协议或默认策略。

机器可读身份已写入：

```text
output/benchmarks/stage16/baseline_identity.json
```

当前可复现的软件基线为 Release、Legacy、635 x 600 dpi、0.038 mm 层高、LibTIFF 默认 Writer、OpenVDB OFF。`segment_101` core-only 复测通过：

| 指标 | 当前值 |
|---|---:|
| grid | 303 x 614 x 184 |
| modelPixels | 2,652,739 |
| supportPixels | 15,790,835 |
| coreCompute | 1,440.6402 ms |
| sliceProcessing | 1,327.9573 ms |
| layerCompute | 404.9092 ms |
| layerCompose | 416.6738 ms |
| supportGeneration | 892.7557 ms |
| peakWorkingSet | 286,560,256 bytes |

该命令使用 `--benchmark-core-only`，没有写 TIFF、Preview 或报告，因此可作为当前核心计算身份，不与旧 handwritten Writer 的完整写包数据混用。

## Stage 14 Closure Evidence

`REPORT_14_切片能力包封装与打印软件集成准备状态.md` v3.61 已确认：

```text
14C-06B = COMPLETE
14D-05  = COMPLETE
14D-06  = COMPLETE
14D-07  = COMPLETE
14D-08  = COMPLETE
14F-05  = SLICER-SIDE COMPLETE
```

因此真实 Worker、安全发布、取消、Facade 重能力路由和 SPI 生命周期均可作为 Stage 16 的已满足前置。`16C-08` 不再被 Stage 14 内部边界阻塞，但 Preview/I/O 解耦本身尚未实现。

打印侧、目标 RIP、干净机和实物证据仍是 `EXTERNAL ACCEPTANCE DEFERRED`。本报告不会把这些外部项写成 PASS。

## Stage 15 Regression Baseline

在当前 commit 和 Release 构建上完成以下复核：

| 项目 | 结果 |
|---|---|
| Stage 15 G1/G2/G4/G5 | PASS |
| `texture_white_carrier_policy_unit_tests` 等定向测试 | 5/5 PASS |
| Golden SHA-256 | 28/28 PASS |
| Quick CI | BLOCKED_INFRASTRUCTURE |

Quick CI 失败不是材料语义回归：`scripts/run_ci_quick.ps1` 固定使用历史 `build` 目录，该目录重新配置时找不到 `meshoptimizer`。日志位于 `output/benchmarks/stage15/logs/quick_ci_zero_drift.log`。该债不阻塞合成 fixture、诊断-only 和 telemetry 卡，但在 `16D-03` 统一回归及任何默认切换前必须解决。

## 12F/13F Carry-In Audit

| 来源 | 原任务 | 状态 | Stage 16 归属与说明 |
|---|---|---|---|
| 12F-02 | Release 基线刷新 | `still_required` | 旧 Writer 基线作废；并入 16C-01/02 |
| 12F-03 | 支撑统计扫描融合 | `still_required` | 并入 16C-03 |
| 12F-04 | Bottom Projection Range Provider | `still_required` | 13G 已实现铺底行为，但未实现后端中立 range provider；并入 16C-04 |
| 12F-05 | Compose 扫描融合/Buffer 复用 | `still_required` | 并入 16C-05 |
| 12F-06 | Occupancy Provider | `still_required` | 并入 16C-06 |
| 12F-07 | 几何/支撑增量缓存 | `still_required` | ViewData/预览缓存不等价于生产 occupancy 缓存；并入 16C-07 |
| 12F-08 | Preview/I/O 解耦 | `still_required` | Stage 14 Worker 前置已满足；读侧 LRU 不等价于生产写侧解耦；并入 16C-08 |
| 12F-09 | 12F 阶段收口 | `superseded` | 由 16D-03/04 统一收口 |
| 13F-R1-01 | 单实例 core/compose telemetry | `still_required` | H-F-05 只有作业级阶段耗时；并入 16C-01 |
| 13F-R1-02 | import parse/texture/preview/hash telemetry | `still_required` | 并入 16C-01 |
| 13F-R1-03 | 自适应 Surface Preview | `still_required` | 并入 16C-08 |
| 13F-R1-04 | 平移实例复用 | `already_satisfied` | Stage 13B 已对纯平移场景实现 producer 2 / reuse 20；更广的几何/支撑缓存仍由 16C-07 处理 |
| 13F-R1-05 | 有限并行 | `still_required` | 并入 16C-09，必须先有内存预算 |
| 13F-R1-06 | Reality 定向与旧性能基线 | `superseded` | 定向/落台行为已满足；handwritten 时代性能数由 16C-02 重测替代 |
| Stage 14 | Worker/Facade/cancel 前置 | `already_satisfied` | 允许 16C-08 排期 |
| 13B OPEN INPUT | 设备体积与 22 实例 SLA | `blocked_external` | 16C-10 只能输出实测，不能关闭 production Gate |

## Asset And External Evidence Status

| 资产 | 当前身份 | 准入结论 |
|---|---|---|
| `260730-13-35-10-849-segment_101.txt.obj` | tracked，SHA-256 `83FD...DAB0` | 可用于本地回归和候选比较 |
| `000000.tiff` | tracked，649 x 286 x 6，uint8，contiguous，uncompressed，SHA-256 `A0FF...DCCA` | 仅可作为本地对照，不是生产 Golden |
| `reality_101.json` | SHA-256 `4CF6...603C`，635 x 600 dpi，0.038 mm | 当前软件基线配置 |

参考 TIFF 没有可用的 X/Y 物理分辨率权威值，且通道语义、来源授权和重分发边界未被正式文件确认；常见图像读取器也不保证能识别其六通道私有布局。因此不得用它单独授权采样默认切换。

历史外部对比使用 0.021 mm 层高，当前软件基线使用 0.038 mm。两者只能做几何趋势参考，不能做逐字节或耗时同比。

## Pending Product Inputs

以下输入不会阻塞 16A 合成候选和 16B diagnostic-only，但会阻塞生产默认决策：

1. 尺寸忠实与亚像素薄特征保存之间的产品偏好，以及可接受的正向像素偏差。
2. 外部 TIFF 的 R/G/B/W/S/V 语义、像素尺寸、层高、来源授权和重分发权限。
3. 接触姿态应以实测接触面积、两侧包络还是目视形态为主，及最大允许调平角。
4. 正式设备 buildVolume、原点、X/Y 轴向、22 实例 SLA 和峰值内存预算。
5. 新采样/姿态参数是否需要进入公开 SPI；在冻结前只允许内部 candidate 配置。

## GO / DEFER / NO-GO

### GO

```text
16A-01..05：合成 fixture、Provider 包装、Layer Slab、固定 2x2 候选和候选矩阵；
16B-01..03：接触指标基线、diagnostic-only 分析和 A/B 矩阵；
16C-01：补齐真实单实例/import telemetry；
16C-03..09：在各自依赖满足后按卡推进。
```

这些工作必须保持 Legacy 默认、`p0.rgbwsv.2`、RGBWSV 顺序、uint8/black_is_print、Stage 15 补白语义不变。

### DEFER

```text
16A-06 的默认采样结论：等 sampling_matrix 和产品偏好；
16B-04/05 的实际调平与默认决策：等矩阵、工艺证据和单独授权；
16C-02 的完整多策略基线：等 16A-05 冻结候选；
16C-10 production Gate：等设备和 SLA 产品输入；
16D-01..05：等采样/姿态/性能矩阵与 Quick CI 基础设施修复。
```

### NO-GO

当前没有证据否定 Stage 16 的诊断或候选工作。但以下做法为 NO-GO：直接替换 Legacy 默认、修改生产协议、以外部 TIFF 私有语义充当权威 Golden、用无界并行换速度、用形态学膨胀伪装几何修复。

## Authorized First Task

用户在本轮明确要求 R-F 完成且准备工作完成后开启 Stage 16。该授权满足 `16-00-04` 的启动确认，第一张代码卡确定为：

```text
16A-01 合成 fixture 和差异 schema = READY / USER AUTHORIZED
```

`16B-01` 与 `16C-01` 虽可并行，但不得与 16A-01 在同一原子提交中混做。

## Verification Plan

1. `16A-01`：手算平底、上/下降斜楔、圆弧边、亚像素薄片、多区间负例，并验证 schema 稳定性。
2. `16A-02..04`：保持 Legacy golden 零漂移；候选默认关闭；非 heightfield fail-closed。
3. `16A-05`：输出逐层/通道/连通分量/尺寸/时间/内存矩阵。
4. `16B`：只读诊断先行，保留 +Z/+Y、autoOrient=false 和不缩放约束。
5. `16C`：只使用同机 Release before/after，core-only 与 end-to-end 分开，无法分解的字段写 null。
6. `16D`：修复 Quick CI 构建入口后再跑统一 Gate；外部证据缺失项保持 DEFERRED。

## 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-12 | v1.0 | 完成 16-00-01..04：固化当前 Release/资产/Profile 身份，复核 Stage 14/15，审计 12F/13F carry-in，形成 PARTIAL GO 并授权首张代码卡 16A-01。 |
