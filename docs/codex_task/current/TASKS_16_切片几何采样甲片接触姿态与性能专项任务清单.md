# TASKS_16 切片几何采样、甲片接触姿态与性能专项任务清单

> 阶段：Stage 16
> 状态：**16A-06 / 16B-02 / 16C-01 COMPLETE；16B-03、16C-02、受限 16D-01 READY**
> 版本：v1.3
> 日期：2026-08-12
> 规则：Stage 14 收口前不得执行任何 Stage 16 代码卡；收口后仍必须从 16-00 开始

## 0. 🔴 2026-08-11 用户裁定：准入 Gate 口径与开工条件

权威决策：`docs/slice/DOC/DOC_DECISION_16_00_Stage16准入Gate口径与R_F线排期裁定.md`

```text
① 本清单的「Stage 14 收口」取【读法甲】= Stage 14【切片侧】收口
   14F-05 已 SLICER-SIDE COMPLETE，收口报告已产出 ⇒ 16-00 的阻塞条件已满足
   【不等】14A_EXTERNAL_ACK 外部书面回签

② 开工条件：RENDER 专项的 R-F 线（R-F-01 平滑顶点法线 → R-F-02 基线重固化）已完成，
   16-00-01..04 已解锁

③ 用户于 2026-08-11/12 明确要求 R-F 和准备工作完成后开启 Stage 16；
   16-00-04 已形成 PARTIAL GO，第一张代码卡 16A-01 转 READY
```

### 0.1 ⚠️ Stage 14 Worker/Facade 边界复核结果

当前 `REPORT_14_切片能力包封装与打印软件集成准备状态.md` v3.61 已确认：
`14D-05/06/07/08` 与 `14C-06B` 全部 `COMPLETE`。因此早期任务表中残留的
`PREPARATION_GATE BLOCKED` 行属于历史状态，不能继续作为 `16C-08` 的阻塞依据。

须在 `16-00-02` 中逐项显式登记：

| 阻塞项 | 性质 | 影响 |
|---|---|---|
| Stage 14 Worker/Facade 最终边界 | **已满足**：真实 Worker、安全发布、取消和 SPI 生命周期均已完成 | `16C-08` 不再被 Stage 14 阻塞，但实现本身仍为 `still_required` |
| `13B` 的 `OPEN INPUT`：设备 buildVolume/origin/axes 与 22 实例生产预算未定 | **产品输入**，切片侧定不了 | `16C-10` 出口本就是「只输出实测，状态仍 `INPUT_OPEN`」——**不是能关掉的卡** |

⇒ **`16C-01..09` 均可进入本阶段排期；`16C-10` 只能形成实测，不能关闭生产 Gate。**

### 0.2 🔴 Stage 14 之前的性能基线已整体作废（A）

`T-A-03` 已把默认 Writer 切为 LibTIFF 4.7.1，同机 Release Writer-only p50 相对 handwritten
变化区间 **+1.086% ~ +48.775%**。

```text
13F-R1-06 的「完整写包 6516.322 ms」是 handwritten 时代的数字
12F / 13F 的全部 Release 性能数字同理
⇒ 均不可与 Stage 16 新基线直接比较
⇒ 16C-02 必须整套重跑，【不得复用旧数做 before/after】
```

### 0.3 已知的一处 carry-in 重叠（须在 16-00-02 判定）

H-F-05（2026-08-11）已把 Worker 核心 `SliceRunProfile` 细分耗时接入参考宿主，
覆盖配置/模型/网格/切片/逐层计算/场景合成/TIFF/预览/报告/输出与 Worker 总耗时。
这与 `16C-01` 的 carry-in（13F-R1-01/02 分项计时）高度重叠。

⚠️ **不得直接判 `already_satisfied`**：`16C-01` 要的是**单实例** core/compose 与
**import 的 parse/texture/preview/hash** 分项，H-F-05 提供的是**作业级**阶段耗时。
粒度对不上的部分仍是 `still_required`。

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

**状态：COMPLETE（2026-08-12）**

> R-F 线已按任务独立提交。Stage 16 治理文档为本卡开始前的已知改动，须先单独提交，
> 再固化 `baseline_identity.json`，不得把治理改动误记为代码基线漂移。

```text
读取 Stage 14 最终报告、当前 commit、Facade/SPI/Worker 合同；
记录 git status --short；
区分用户未提交变更和 Stage 16 可用基线；
不把 2026-08-06 Runtime 二进制当作 Stage 14 后代码身份。
```

**出口：** `baseline_identity.json` 包含 commit/build/compiler/Profile/asset/config hash。

**实际结果：** 已生成 `output/benchmarks/stage16/baseline_identity.json`；当前 Release
Legacy core-only 基线 PASS，工作树身份、LibTIFF/OpenVDB 开关、编译器、Profile 与资产 hash 已冻结。

### 16-00-02 任务重叠审计

**状态：COMPLETE（2026-08-12）**

检查 Stage 14 是否已实现或更改 12F/13F 的 telemetry、取消、Worker I/O、buffer 和缓存能力。
**须一并登记 §0.1 的 Worker/Facade 已满足事实、`16C-10` 产品输入和 §0.3 的 H-F-05 telemetry 重叠。**

**出口：** carry-in 矩阵的每项状态为 `already_satisfied|still_required|superseded|blocked_external`。

**实际结果：** 12F/13F/13B/Stage 14 carry-in 已逐项分类，详见
`REPORT_16_00_Stage16准入复核当前状态.md`。

### 16-00-03 资产和外部语义审计

**状态：COMPLETE（2026-08-12）**

确认 `segment_101`、参考 TIFF 堆栈的版本化/授权/通道/层高/像素尺寸。不能确认时，保留为本地对照，不得升级为生产 Golden。

**实际结果：** OBJ/参考 TIFF 均已记录 SHA-256；参考 TIFF 的通道、像素尺寸、来源授权和重分发边界未获权威确认，只保留为本地对照。

### 16-00-04 GO/DEFER/NO-GO 评审

**状态：COMPLETE / PARTIAL GO（2026-08-12）**

```text
GO：明确可启动的 16A/16B/16C 子范围；
DEFER：记录未决输入和不重复进入条件；
NO-GO：记录新证据为何否定本专项。
```

**出口：** 用户明确确认后才可将第一张代码卡改为 READY。

**实际结果：** 用户本轮启动要求视为首卡授权；16A-01 READY。默认采样、实际调平、
production Gate 和统一收口仍按报告中的 DEFER 条件控制。

## 4. 16A 几何采样

### 16A-01 合成 fixture 和差异 schema

**状态：COMPLETE（2026-08-12）**

**依赖：** 16-00 GO
**内容：** 建立平底、上升/下降斜楔、圆弧边、亚像素薄片和多区间负向 fixture；冻结 layer/channel/connected-component/dimension diff schema。
**出口：** fixture 不依赖 Reality 文件名，手算期望可复核。

**实际结果：** 六组 fixture、diff schema、示例报告和 C++ 合同测试已落地；Debug 构建与定向 CTest 1/1 PASS。生产采样路径未修改。

### 16A-02 GeometryOccupancyPolicy 和 Provider 合同

**状态：COMPLETE（2026-08-12）**

**依赖：** 16A-01
**内容：** 新增策略 DTO/provider wrapper，Legacy 实现仍调用现有路径。
**出口：** 默认 Legacy package/golden 逐字节不变；公共核心 API 为 STL-only。

**实际结果：** 新增 STL-only 策略 DTO、列占用 DTO 与 Provider；既有 `relief_heightfield` 路径显式构造并校验 Legacy 策略，仍沿用原有 mask 循环。Stage 16 定向 CTest 2/2 PASS，Golden fixture 的 25 个 TIFF layer 逐文件 SHA-256 差异为 0；Layer Slab 与 2x2 继续 fail-closed。

### 16A-03 Layer Slab Candidate

**状态：COMPLETE（2026-08-12）**

**依赖：** 16A-02
**内容：** 实现半开 layer slab 相交，首版只支持已准入 `relief_heightfield`。
**出口：** 上升/下降斜楔无边界重复或丢层；非 heightfield 配置 fail-closed。

**实际结果：** 已实现 `LayerSlabCoverage + PixelCenter` 半开层体积候选，固定边界容差为
`1e-9 mm`，零厚度列不占据；只允许显式 `relief_heightfield` 配置，通用 mesh 和 2x2
继续 fail-closed。定向 CTest 2/2 PASS，候选 Package 与 RIP strict PASS，Legacy Golden
TIFF layer 逐文件 SHA-256 差异 0/25。

### 16A-04 边界自适应 2x2 Candidate

**状态：COMPLETE（2026-08-12）**

**依赖：** 16A-03
**内容：** 仅对边界计算固定 2x2 子样本；同时支持 `>=2/4` 和 `>=1/4` 候选。
**出口：** 确定性；无随机 jitter；无完整高分辨率 3D 体常驻。

**实际结果：** 已实现固定四点 `(0.25/0.75, 0.25/0.75)` 的边界自适应 2x2 Heightfield
候选，S3 按同层 `>=2/4`、S4 按同层 `>=1/4` 判定。明确内部复用中心列，明确外部跳过，
三角形投影候选边界才执行四点求交；仅保存二维单区间样本列。两个候选 Package 与 RIP strict
PASS，Stage 16 CTest 2/2 PASS，Legacy Golden TIFF 差异 0/25；默认仍为 Legacy。

### 16A-05 机器可读候选矩阵

**依赖：** 16A-04
**内容：** 对 S0/S2/S3/S4 运行合成 fixture、Reality 5/5、Stage 15 和 Package/RIP 矩阵。
**出口：** `sampling_matrix.json`，包含逐层/通道 diff、尺寸偏差、连通分量、时间和内存。

**实际结果：** 新增 Release `stage16_sampling_matrix`。合成 fixture 与 Stage 15 白区载体
S0/S2/S3/S4 Package/RIP strict 8/8 PASS；Reality 5/5 使用真实下表面支撑完成四策略矩阵。
S3 在 Reality 5/5 上比 S4 更克制地扩张模型/支撑占用；候选计时和进程内存只作诊断，
16C-02 仍须独立重跑 p50/p95。默认继续为 S0。

### 16A-06 采样候选决策刷新

**依赖：** 16A-05
**内容：** 选择 `S3|S4|DEFER|NO-GO`，明确尺寸忠实与薄特征保存偏好。
**出口：** 独立 Decision 刷新；未授权时不切换默认。

**实际结果：** 选择 S3 作为后续姿态 A/B、Release 基线和 Facade 显式 opt-in 的首选诊断候选；
S4 保留为薄特征上限对照，S2 只保留回归。生产默认继续为 S0，默认切换 `DEFER`。权威结论见
`DOC_DECISION_16A_06_采样候选决策刷新.md`。

## 5. 16B 甲片接触姿态

### 16B-01 边界带和接触指标基线

**依赖：** 16-00 GO；可与 16A-01 并行
**内容：** 冻结两侧边界带、前 1/2 slab 接触面积、角度和方向约束。
**出口：** Reality 5/5 和标准甲片的 `posture_baseline.json`。

**状态：COMPLETE（2026-08-12）**

**实际结果：** 新增只读 `ContactPostureMetrics` 与确定性基线工具，冻结两侧各 `12.5%`
边界带、`0.5 * 0.038 mm` 首半 slab 接触面积代理、`12 deg` 角度边界及 +Z/+Y 约束；
Reality 5/5 与标准 `nai_you` 共 6/6 PASS，基线重复生成 SHA-256 一致，不修改顶点和默认策略。

### 16B-02 ContactLevelingAnalyzer diagnostic-only

**依赖：** 16B-01
**内容：** 在长轴限制下生成候选角和报告，不改变实际顶点。
**出口：** 无 Qt 核心；无文件名特判；报告含回退原因。

**状态：COMPLETE（2026-08-12）**

**实际结果：** 新增无 Qt `ContactLevelingAnalyzer`，按固定 `0.5 deg` 粗搜和 `0.1 deg`
精化生成 diagnostic-only 候选；每个候选独立归地并检查 +Z/+Y、`12 deg` 角度、
`0.5 mm` 高度增量和 `0.5 mm` X 占地增量预算。Reality 5/5 与标准甲片 1/1 共
6/6 PASS，真实报告可重复生成；不修改输入顶点、实例变换、配置、autoOrient 或默认姿态。

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

**状态：COMPLETE（2026-08-12）**

**实际结果：** `SliceRunProfile` 新增 unique model import 与 visible instance 加法诊断；
`load_model_report`、resource hash、单实例 core/compose/total 均在真实边界计时，当前生产链
没有独立执行的 texture decode 与 surface preview 明确输出 `null`。Worker JSON 与场景生产
定向 CTest 2/2 PASS，既有作业级字段、SPI 和 Package 协议不变。

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

## 9. 当前唯一允许动作（2026-08-12 更新）

```text
保留本文档和上游 PRD/DEV/DECISION/PREP/PROMPT；
R-F 线与 16-00-01..04 已完成；
16A-01/02/03/04/05 已完成并通过定向验证、候选 Package/RIP、真实矩阵与 Golden 零漂移检查；
16B-01/02 已完成，Reality 5/5 与标准甲片的基线、只读调平候选均为 6/6 PASS；
16A-06 已选择 S3 作为显式诊断候选，生产默认继续为 S0；
用户已授权同步推进，16B-03 与 16C-02 的前置均已满足；
16D-01 仅可先接入获批的 S3 显式 opt-in；姿态接入仍等待 16B-03/04；
候选和默认配置继续关闭。
```

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-12 | v1.3 | 完成 16A-06：依据 16A-05 矩阵选择 S3 为首选诊断候选、S4 为薄特征上限对照；S3 获准进入 16B-03、16C-02 和受限 16D-01，生产默认继续为 S0，默认切换 DEFER。 |
| 2026-08-12 | v1.2 | 完成 16A-05：新增机器可读 S0/S2/S3/S4 候选矩阵，合成 fixture、Reality 5/5、Stage 15 和 Package/RIP 全部执行；归档逐层/通道、尺寸、连通分量、时间和内存诊断，16A-06、16B-03、16C-02 依赖转为满足。 |
| 2026-08-12 | v1.1 | 完成 16B-02：实现无 Qt、无文件名特判的确定性只读调平分析器，Reality 5/5 与标准甲片 1/1 共 6/6 PASS；候选不改变顶点和默认姿态，16B-03 继续等待 16A-05。 |
| 2026-08-12 | v1.0 | 完成 16C-01：新增 import parse/hash 与 visible instance core/compose/total 真实计时，未独立执行的 texture/preview 输出 null；Worker/scene CTest 2/2 PASS，16C-02 继续等待 16A-05。 |
| 2026-08-12 | v0.9 | 完成 16B-01：冻结两侧边界带、首半 slab 接触面积代理、候选角及 +Z/+Y 约束；新增无 Qt 只读测量器、单测和确定性 `posture_baseline.json`，Reality 5/5 与标准甲片 6/6 PASS；16B-02 依赖满足，16D 仍等待 16A-06。 |
| 2026-08-12 | v0.8 | 完成 16A-04：新增 S3 `>=2/4` 与 S4 `>=1/4` 两个固定 2x2 边界自适应候选；Stage 16 CTest 2/2、两个 Package/RIP strict PASS，Legacy TIFF 0/25 差异；用户授权同步推进 16B/16C/16D，当前仅 16B-01、16C-01 前置满足，16D 继续等待 16A-06。 |
| 2026-08-12 | v0.7 | 完成 16A-03：实现仅限 `relief_heightfield` 的半开 Layer Slab + Pixel Center 候选，冻结正测度相交、固定 `1e-9 mm` 边界容差和零厚度不占据；定向 CTest 2/2、候选 Package/RIP strict PASS，Legacy TIFF 0/25 差异；16A-04 转为下一张依赖已满足的卡。 |
| 2026-08-12 | v0.6 | 完成 16A-02：新增 STL-only GeometryOccupancyPolicy、列占用 DTO 与 Provider；Legacy 生产路径显式校验策略并保持原有循环，定向 CTest 2/2 PASS，Golden TIFF layer 0/25 字节差异；16A-03 转为下一张依赖已满足但尚未开始的卡。 |
| 2026-08-12 | v0.5 | 完成 16A-01：新增六组可手算合成 fixture、逐层/通道/连通分量/尺寸差异合同和定向 C++ 测试；16A-02 依赖满足但尚未开始。 |
| 2026-08-12 | v0.4 | 完成 16-00-01..04：固化 Release/资产/Profile 身份，完成 carry-in 与外部语义审计，形成 PARTIAL GO；依据用户启动授权将 16A-01 转 READY。 |
| 2026-08-11 | v0.3 | R-F-01/02 完成后启动 16-00；依据 `REPORT_14` v3.61 修正早期状态漂移：14D-05/06/07/08 与 14C-06B 均已完成，`16C-08` 不再被 Stage 14 内部边界阻塞；`16C-10` 继续保持产品输入开放。 |
| 2026-08-11 | v0.2 | 用户裁定准入 Gate 取读法甲（Stage 14 切片侧收口），16-00-01..04 由 `BLOCKED` 转 `READY / 等 R-F 线完成`；新增 §0 记录裁定、§0.1 两项残留阻塞（`16C-08` 被 14D 内部阻塞、`16C-10` 保持 `INPUT_OPEN`，16C 按 8 张可推进卡计）、§0.2 历史性能基线因 LibTIFF 切换整体作废、§0.3 H-F-05 telemetry 与 `16C-01` carry-in 的重叠须逐项判定；§9 改写为当前允许动作。权威决策见 `DOC_DECISION_16_00_Stage16准入Gate口径与R_F线排期裁定.md`。 |
| 2026-08-06 | v0.1 | 首版。 |
