# TASKS_16 切片几何采样、甲片接触姿态与性能专项任务清单

> 阶段：Stage 16
> 状态：PROPOSED / NOT ACTIVE / BLOCKED BY STAGE 14
> 版本：v0.1
> 日期：2026-08-06
> 规则：Stage 14 收口前不得执行任何 Stage 16 代码卡；收口后仍必须从 16-00 开始

## 1. 任务总览

| Wave | 主题 | 目标 |
|---|---|---|
| 16-00 | Stage 14 后准入 | 决定 GO/DEFER/NO-GO 和最小实施范围 |
| 16A | 几何采样 | 层体积、2x2 覆盖候选、diff 和默认决策输入 |
| 16B | 接触姿态 | 边界带/接触面积诊断、受限调平候选 |
| 16C | 性能整并 | 吸收 12F/13F 未完任务，收口单模型和 22 实例预算 |
| 16D | 集成与收口 | Stage 15/14/Package/RIP/UI 回归和最终默认决策 |

## 2. 固定边界

```text
Legacy center sample 在 Stage 16 全程保留；
新采样和姿态候选默认关闭；
先语义、后性能，不允许并行改动同一热路径的语义和存储形式；
12F/13F 保留历史状态，Stage 16 只记录 carry-in 和新证据；
每张性能卡必须有 Release before/after 和输出一致性；
任何一张卡完成后停止，不自动开始下一张。
```

## 3. 16-00 Stage 14 后准入复核

### 16-00-01 收口身份和脏工作树审计

**状态：BLOCKED BY STAGE 14**

```text
读取 Stage 14 最终报告、当前 commit、Facade/SPI/Worker 合同；
记录 git status --short；
区分用户未提交变更和 Stage 16 可用基线；
不把 2026-08-06 Runtime 二进制当作 Stage 14 后代码身份。
```

**出口：** `baseline_identity.json` 包含 commit/build/compiler/Profile/asset/config hash。

### 16-00-02 任务重叠审计

**状态：BLOCKED**

检查 Stage 14 是否已实现或更改 12F/13F 的 telemetry、取消、Worker I/O、buffer 和缓存能力。

**出口：** carry-in 矩阵的每项状态为 `already_satisfied|still_required|superseded|blocked_external`。

### 16-00-03 资产和外部语义审计

**状态：BLOCKED**

确认 `segment_101`、参考 TIFF 堆栈的版本化/授权/通道/层高/像素尺寸。不能确认时，保留为本地对照，不得升级为生产 Golden。

### 16-00-04 GO/DEFER/NO-GO 评审

**状态：BLOCKED**

```text
GO：明确可启动的 16A/16B/16C 子范围；
DEFER：记录未决输入和不重复进入条件；
NO-GO：记录新证据为何否定本专项。
```

**出口：** 用户明确确认后才可将第一张代码卡改为 READY。

## 4. 16A 几何采样

### 16A-01 合成 fixture 和差异 schema

**依赖：** 16-00 GO
**内容：** 建立平底、上升/下降斜楔、圆弧边、亚像素薄片和多区间负向 fixture；冻结 layer/channel/connected-component/dimension diff schema。
**出口：** fixture 不依赖 Reality 文件名，手算期望可复核。

### 16A-02 GeometryOccupancyPolicy 和 Provider 合同

**依赖：** 16A-01
**内容：** 新增策略 DTO/provider wrapper，Legacy 实现仍调用现有路径。
**出口：** 默认 Legacy package/golden 逐字节不变；公共核心 API 为 STL-only。

### 16A-03 Layer Slab Candidate

**依赖：** 16A-02
**内容：** 实现半开 layer slab 相交，首版只支持已准入 `relief_heightfield`。
**出口：** 上升/下降斜楔无边界重复或丢层；非 heightfield 配置 fail-closed。

### 16A-04 边界自适应 2x2 Candidate

**依赖：** 16A-03
**内容：** 仅对边界计算固定 2x2 子样本；同时支持 `>=2/4` 和 `>=1/4` 候选。
**出口：** 确定性；无随机 jitter；无完整高分辨率 3D 体常驻。

### 16A-05 机器可读候选矩阵

**依赖：** 16A-04
**内容：** 对 S0/S2/S3/S4 运行合成 fixture、Reality 5/5、Stage 15 和 Package/RIP 矩阵。
**出口：** `sampling_matrix.json`，包含逐层/通道 diff、尺寸偏差、连通分量、时间和内存。

### 16A-06 采样候选决策刷新

**依赖：** 16A-05
**内容：** 选择 `S3|S4|DEFER|NO-GO`，明确尺寸忠实与薄特征保存偏好。
**出口：** 独立 Decision 刷新；未授权时不切换默认。

## 5. 16B 甲片接触姿态

### 16B-01 边界带和接触指标基线

**依赖：** 16-00 GO；可与 16A-01 并行
**内容：** 冻结两侧边界带、前 1/2 slab 接触面积、角度和方向约束。
**出口：** Reality 5/5 和标准甲片的 `posture_baseline.json`。

### 16B-02 ContactLevelingAnalyzer diagnostic-only

**依赖：** 16B-01
**内容：** 在长轴限制下生成候选角和报告，不改变实际顶点。
**出口：** 无 Qt 核心；无文件名特判；报告含回退原因。

### 16B-03 两侧包络与接触面积 A/B

**依赖：** 16B-02，16A-05 提供已选采样口径
**内容：** 比较 P0/P2/P3，不以两点等高单独决策。
**出口：** `posture_matrix.json`，包含接触改善、高度、占地、支撑和准入变化。

### 16B-04 显式 opt-in 调平候选

**依赖：** 16B-03 和用户单独授权
**内容：** 将获批算法作为显式候选应用，P0 仍为默认。
**出口：** autoOrient=false 不受影响；失败 fail-closed，不静默使用其他角度。

### 16B-05 姿态默认决策

**依赖：** 16B-04 真实矩阵和必要工艺证据
**出口：** 保持 P0，或另立 Decision 授权 P5；不得在本卡中顺手改默认。

## 6. 16C 性能整并

### 16C-01 分项 Telemetry 收口

**carry-in：** 13F-R1-01/02，12F-02
**内容：** 补齐单实例 core/compose 和 import parse/texture/preview/hash 计时；优先复用 Stage 14 telemetry。
**出口：** 字段有真实计时边界，无法分解的字段为 null，不伪造。

### 16C-02 Stage 14 后 Release 基线

**依赖：** 16C-01，16A-05
**内容：** 单模型和 1/11/12/22 场景，S0/S3/S4，core-only/end-to-end，cold/warm。
**出口：** p50/p95、峰值内存、输出 hash、构建身份完整。

### 16C-03 支撑统计扫描融合

**carry-in：** 12F-03
**出口：** grid/model/support/type totals/hash 完全一致；三模型 Release before/after。

### 16C-04 Bottom Projection Range Provider

**carry-in：** 12F-04
**出口：** 逐层 support mask diff=0；失败可显式回退 Legacy materialization。

### 16C-05 Layer Compose 扫描融合与 Buffer 复用

**carry-in：** 12F-05
**出口：** RGBWSV 逐层 hash、report totals、Stage 15 闭合和 RIP strict 一致。

### 16C-06 Occupancy Provider 分块/流式化

**carry-in：** 12F-06
**依赖：** 16A 语义候选已冻结
**出口：** 减少完整 model mask 常驻；S0/S3/S4 输出与各自未优化基线一致。

### 16C-07 几何/支撑/平移实例缓存

**carry-in：** 12F-07，13F-R1-04
**出口：** cold/warm 输出一致；key 至少包含 asset/pose/DPI/layerHeight/sampling/support/material；失效原因可诊断。

### 16C-08 Preview/I/O 解耦和自适应 Preview

**carry-in：** 12F-08，13F-R1-03
**依赖：** Stage 14 Worker/Facade 最终边界
**出口：** preview 按需/异步，不阻塞核心完成；stale/cancel 不加载错误结果。

### 16C-09 有内存预算的有限并行

**carry-in：** 13F-R1-05，12F P3
**依赖：** 16C-03..08 单线程优化和内存预算
**出口：** 并行数由预算限制；确定性不回归；取消可以贯穿。

### 16C-10 22 实例预算与 production Gate

**carry-in：** 13B OPEN INPUT
**内容：** 用正式 DPI/层高/设备幅面/Profile/SLA 运行 1/11/12/22 矩阵。
**出口：** 外部 SLA 未冻结时只输出实测，状态仍 `INPUT_OPEN`。

## 7. 16D 集成与收口

### 16D-01 Effective Config / Facade / Worker 接入

**依赖：** 16A-06，必要时 16B-04
**内容：** 只暴露已批准候选；旧 Host 不识别新配置时 fail-closed；不改 Stage 14 v1 语义。

### 16D-02 Qt 诊断与 A/B 预览

**依赖：** 16D-01
**内容：** 展示策略、首层/当前层差异、姿态候选和性能摘要；不在 UI 重算几何。

### 16D-03 统一回归 Gate

```text
Legacy zero-drift；
sampling/posture fixture；
Reality 5/5；
Stage 15 补白和材料闭合；
13G 支撑连续/铺底；
13B 1/11/12/22；
Package/RIP strict；
Stage 14 Facade/SPI/Worker/cancel；
Debug/Release Runtime 和 Quick CI/full regression。
```

### 16D-04 阶段收口报告

输出 `REPORT_16`，将结论分为：

```text
Current State；
Candidate State；
Production Default Decision；
Performance Budget State；
External Pending Confirmation；
12F/13F carry-in disposition。
```

### 16D-05 默认切换授权

**状态：REQUIRES EXPLICIT USER AUTHORIZATION**

本卡只在 16D-04 完成且用户明确授权后执行。未授权时，Stage 16 可以以“候选可用，Legacy 仍默认”收口。

## 8. 依赖顺序

```text
Stage 14 closure
  -> 16-00-01..04
  -> 16A-01..06
  -> 16B-01..05
  -> 16C-01..10
  -> 16D-01..05

16B-01/02 可与 16A-01/02 并行；
16C-01 可在 16A 实施前完成；
其余性能优化必须等采样语义冻结后再做。
```

## 9. 当前唯一允许动作

```text
保留本文档和上游 PRD/DEV/DECISION/PREP/PROMPT；
等待 Stage 14 完成；
不运行 Stage 16 构建、不修改代码、不改默认配置。
```
