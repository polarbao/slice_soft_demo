# DOC_PREP_16 切片几何采样、甲片接触姿态与性能实施准备

> 文档状态：PREPARED / IMPLEMENTATION BLOCKED BY STAGE 14
> 日期：2026-08-06
> 对应阶段：Stage 16
> 执行前置：Stage 14 收口 + 16-00 GO/DEFER/NO-GO 复核

## 1. 准备目标

将 2026-08-06 `segment_101` 分析得到的采样/姿态问题，与 12F、13F 和 13B 尚未完成的性能任务整理为可以在 Stage 14 后独立评审的 Stage 16。

本文档不代表已授权新采样默认、连续调平或性能重写。

## 2. 证据分级

### A 级：当前可复核代码/输出

```text
src/slicer_core/slicer.cpp：层中心和 relief column range；
src/slicer_core/model.cpp：直角定向、正反面和平面朝向；
src/slicer_core/support/SupportBaseProjection.cpp：铺底只做层号/物理抬升；
runtime/slicesoft/Release 中的 2026-08-06 无铺底 Package；
12B-R1、12E-10C、13B-07、13F 性能报告。
```

### B 级：当前正式目标/决策

```text
DOC_DECISION_13E：正面 +Z、尖端 +Y 和确定性定向；
DOC_AUDIT_13G：Reality 正反面和支撑连续性；
DOC_DECISION_12F：Release Runtime 和性能优先级；
DOC_DECISION_13F：分项计时、缓存和有限并行边界；
Stage 15：按需补白和材料闭合已完成。
```

### 未版本化的本地参考证据

```text
model/obj/reality/260730-13-35-10-849-segment_101.txt.obj；
model/obj/reality/000000.tiff；
runtime/slicesoft/Release/test_slice/切片数据/000000..000331.tiff；
runtime/slicesoft/Release/output/ui_sessions/
  260730-13-35-1_single_material_scene_legacy_20260806_145025_168_06d2c01a/package。
```

上述本地资产当前不应直接作为 CI Golden。Stage 16 开工前必须确认授权、存储位置、hash、外部 TIFF 通道语义和重分发边界。

## 3. `segment_101` 已知定量证据

### 3.1 几何和分层

```text
模型定向后 Z 高度：6.9752 mm；
层高：0.021 mm；
首层中心：0.0105 mm；
首层中心的最窄连续截面：约 0.04746 mm；
首层顶面 0.021 mm 的最窄连续截面：约 0.09499 mm；
截面最窄宽度在 z 约 0.06816 mm 才达到 0.3 mm。
```

### 3.2 SliceSoft 与参考堆栈

| 指标 | SliceSoft | 参考切片器 |
|---|---:|---:|
| TIFF 文件数 | 333 | 332 |
| 非空模型层 | 332 | 332 |
| 尾部空层 | 1 | 0 |
| 首层模型像素 | 16 | 71 |
| 模型总像素 | 4,800,409 | 5,092,513 |
| 模型+支撑总像素 | 33,384,676 | 33,516,491 |

两者并非整层错位。参考首层更符合首个物理 layer slab 的投影/保守覆盖，而 SliceSoft 使用 0.0105 mm 单平面。

### 3.3 候选离线模拟

| 候选 | 首层 | 模型总像素 | 相对参考总量 |
|---|---:|---:|---:|
| 当前中心采样 | 16 | 4,800,409 | -5.74% |
| layer slab + 2x2，>=1/4 | 83 | 5,110,020 | +0.34% |
| layer slab + 2x2，>=2/4 | 79 | 5,049,177 | -0.85% |
| 参考切片器 | 71 | 5,092,513 | 基准 |

该离线模拟只证明 S3/S4 值得进入候选，不能单独授权默认切换。

### 3.4 姿态证据

```text
当前定向：rotate_x_180_rotate_z_minus_90；
一侧边界带先接触，对侧边界带最低区约高 1.24..1.71 mm；
以两侧最低带估算的等高滚转约 7..9 度；
但在 -12..+12 度只读角度扫描中，
用“首层范围内的边界顶点数”作粗略代理指标时，约 -3.6 度优于强制 7..9 度等高。
```

这证明“两个点同时着地”不是充分的接触优化目标。

## 4. 需要保留的历史结论

```text
12B-R1 已完成，不应重启一套新 heightfield engine；
新采样应包装现有 column range，而不是立即重写全内核；
12F-02..09 和 13F-R1-01..05 仍未完成；
13B 功能矩阵已 PASS，22 实例正式预算仍 OPEN；
12E-10C 已证明 Global 比 Legacy 慢且更耗内存，Stage 16 不重开默认引擎切换；
Stage 15 已是 production，新 occupancy 必须重跑补白和闭合回归。
```

## 5. 开工前必读

```text
.agents/AGENTS.md；
.agents/docs/SLICE_AI_SKILL_MASTER.md；
.agents/docs/architecture-boundary.md；
DOC_DECISION_16；
PRD_16；
DEV_16；
DOC_DECISION_13E；
DOC_AUDIT_13G；
DOC_DECISION_12F + TASKS_12F；
DOC_DECISION_13F + TASKS_13F；
REPORT_12B_R1；
REPORT_12E_10C；
REPORT_13B_07；
Stage 14 最终收口报告（开工时再确定真源）；
REPORT_15 及 Stage 15 Gate。
```

## 6. Stage 14 后复核流程

16-00 只做文档和基线复核：

```text
1. 读取 Stage 14 最终 Facade/SPI/Worker/取消/telemetry 真源；
2. 重跑当前 Release `segment_101` 基线，不假定 2026-08-06 二进制仍等于当前代码；
3. 记录 Stage 15 和 Quick CI 基线；
4. 审计 12F/13F 是否已被 Stage 14 部分吸收；
5. 确认外部 TIFF 资产和语义；
6. 输出 GO / DEFER / NO-GO，并明确可启动的最小子范围。
```

## 7. 文件所有权建议

| 领域 | 建议落点 | 不应落点 |
|---|---|---|
| 采样策略/Provider | `src/slicer_core/geometry` / `raster` | Qt UI、Writer |
| 接触姿态 | `src/slicer_core/model` / `geometry` | support、materials、reports |
| 场景应用 | `src/slicer_core/pipeline` | importer 直接写层 |
| Telemetry | pipeline DTO + reports | report writer 决定策略 |
| 配置/UI | 验证后的 config/Facade/UI | UI 直访 `slicer.cpp` 临时结构 |
| Package/RIP | 只回归现有 output | 新建协议或改通道 |

## 8. 基线和 Gate 资产

Stage 16 开工后应先生成，但本准备阶段不伪造：

```text
output/benchmarks/stage16/baseline_identity.json；
output/benchmarks/stage16/sampling_matrix.json；
output/benchmarks/stage16/posture_matrix.json；
output/benchmarks/stage16/performance_matrix.json；
output/benchmarks/stage16/layer_channel_diff.json；
output/benchmarks/stage16/stage16_summary.json。
```

## 9. 实施停止条件

```text
Stage 14 未收口或尚有与 Stage 16 交叉的大规模重构；
新采样需要修改 p0.rgbwsv.2 才能表达；
Legacy 默认出现任何 golden 漂移；
候选需要对 Reality 文件名特判；
调平无法保持 +Z/+Y 合同或导致准入状态改变；
无法区分策略计算时间与 TIFF/preview/report I/O；
为达成速度需要无界并行或超过内存预算；
外部资产权威语义不明却要求直接生产默认切换。
```

## 10. 准备结论

Stage 16 的问题、候选、依赖、历史性能任务整并、验证资产和停止条件已完成文档准备。当前状态仍为：

```text
DOCUMENTATION READY
IMPLEMENTATION NOT AUTHORIZED
WAIT FOR STAGE 14 CLOSURE AND 16-00 REVIEW
```
