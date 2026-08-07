# CODEX_PROMPT_16 切片几何采样、甲片接触姿态与性能专项执行指令

> 状态：PREPARED / DO NOT EXECUTE BEFORE STAGE 14 CLOSURE
> 日期：2026-08-06
> 对应任务：`TASKS_16_切片几何采样甲片接触姿态与性能专项任务清单.md`

## 1. 执行前硬门

只有同时满足以下条件，才能开始 Stage 16：

```text
1. Stage 14 已有明确收口报告；
2. 用户明确要求启动 Stage 16 准入复核；
3. 从 16-00 而不是 16A/16B/16C 代码卡开始；
4. 当前 git status --short 已记录，用户变更不被覆盖；
5. Stage 14 后的 Facade/SPI/Worker/telemetry 真源已读取；
6. 当前 Stage 15 和 Legacy 基线可复现。
```

任一条不满足，只允许更新文档的 `BLOCKED/DEFERRED` 原因，不得修改代码。

## 2. 必读文档

```text
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/architecture-boundary.md
docs/slice/DOC/DOC_DECISION_16_*.md
docs/slice/PRD/PRD_16_*.md
docs/slice/DEV/DEV_16_*.md
docs/slice/DOC/DOC_PREP_16_*.md
docs/codex_task/current/TASKS_16_*.md
docs/slice/DOC/DOC_DECISION_13E_*.md
docs/slice/DOC/DOC_AUDIT_13G_*.md
docs/slice/DOC/DOC_DECISION_12F_*.md
docs/codex_task/current/TASKS_12F_*.md
docs/slice/DOC/DOC_DECISION_13F_*.md
docs/codex_task/current/TASKS_13F_*.md
docs/slice/REPORT/REPORT_12B_R1_*.md
docs/slice/REPORT/REPORT_12E_10C_*.md
docs/slice/REPORT/REPORT_13B_07_*.md
docs/slice/REPORT/REPORT_15_*.md
Stage 14 最终收口真源
```

## 3. 执行规则

```text
每次只执行用户明确指定的一张任务卡；
任务前先读源码和最新报告，不以本文档行号猜当前实现；
先冻结 before 身份和输出，再编辑代码；
语义变更与性能存储重写不得在同一张卡实施；
候选默认关闭，禁止 silent fallback；
无法获得的计时/内存字段写 null，不使用估算数冒充实测；
任务完成后运行该卡定向验证、git diff --check，更新状态后停止。
```

## 4. 采样不变量

```text
LegacyCenterSample 缺省语义和 golden 不变；
层体积使用统一半开区间；
2x2 子样本位置和顺序确定；
不使用随机 jitter；
不使用第一层形态学膨胀伪装几何修复；
不把超采样中间高分辨率体完整常驻内存；
新策略首先限定在 relief_heightfield admission。
```

## 5. 姿态不变量

```text
保留正面 +Z 和尖端 +Y；
不缩放；
不使用文件名特判；
不用两个最低顶点直接决定角度；
首个任务只 diagnostic-only；
autoOrient=false 不得被覆盖；
候选无法满足高度/占地/准入时 fail-closed。
```

## 6. 性能不变量

```text
只使用 Release 数据形成性能结论；
core-only 和 end-to-end 分开；
cold/warm 分开；
每项优化前后必须在同一采样/姿态语义下比较；
先单线程数据结构优化，最后才评估有限并行；
并行数由显式内存预算限制；
输出语义改变的候选不能使用逐字节等价作 Gate，
必须使用已冻结的几何/材料/diff Gate。
```

## 7. 协议和生产红线

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不改 Model > Support；
不改 Stage 15 按需补白语义；
不默认启用 OpenVDB；
不修改 Legacy/Global 默认层级；
不以项目内 RIP strict 代替外部设备/RIP 证据。
```

## 8. 16-00 输出格式

16-00 必须首先输出：

```markdown
## Current State
## Stage 14 Closure Evidence
## Stage 15 Regression Baseline
## 12F/13F Carry-In Audit
## Asset And External Evidence Status
## Pending Product Inputs
## GO / DEFER / NO-GO
## Authorized First Task
## Verification Plan
```

若结论不是 GO，不得在回答后继续修改代码。

## 9. 通用验证层级

```text
L1 单元/配置/数学合同；
L2 fixture/golden/per-layer-channel diff；
L3 Quick CI/full regression；
L4 Package/RIP strict；
L5 Qt/Facade/Worker/cancel；
L6 外部 RIP/设备/实物，如缺失必须明确披露。
```

## 10. 停止条件

遇到任一条立即停止：

```text
Stage 14 尚未收口；
用户未授权当前任务卡；
需要修改生产协议；
无法保证 Legacy 默认零漂移；
无法证明姿态候选保持 +Z/+Y；
无法区分策略差异与性能优化差异；
需要无界内存/并行；
外部语义未确认却要求宣布生产默认；
工作树中的用户变更与当前卡冲突且无法安全隔离。
```
