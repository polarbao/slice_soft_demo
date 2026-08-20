# CODEX_PROMPT_MATVOL 多材质纵深体积 RGB 与按需补白根治执行指令

> 文档状态：**READY FOR MV-01 AUTHORIZATION**
> 版本：v1.0 ｜ 日期：2026-08-20
> 任务真源：`TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`

## 1. AI 开工读取顺序

```text
1. 根 AGENTS.md 与 .agents/AGENTS.md
2. .agents/docs/architecture-boundary.md、build-and-test.md、code-standards.md
3. DOC_DECISION_MATVOL
4. DOC_PREP_MATVOL
5. DEV_MATVOL
6. 本任务清单的状态表与当前卡
7. Stage 15 Decision/DEV 与 MEMFLOW Decision/DEV
8. 当前源码和 git diff；历史 DEV_06 仅作 C 级背景
```

不得只读本提示词就编码；任务清单状态为唯一真源。

## 2. 开工规则

```text
每次只执行用户明确授权的一张卡；
先运行 git status --short，识别 MEMFLOW/RIPFLOW/用户脏改；
先完成该卡 PREPARED 条件，再修改代码；
手工编辑使用 apply_patch，不回退、不覆盖不相关改动；
生产语义变化卡必须先给 Implementation Plan 并等待确认；
完成后更新任务状态、完成日期、实际验证和修订记录；
未运行的命令不得写 PASS。
```

## 3. 不得自行改写的裁决

```text
材质 02 是浅桃色 [255,220,198]，不是黄色；
结果组合预览中的亮绿色可能是 S 伪彩色；
旧全实体 RGB/按需补白 Profile 保持原语义；
MATVOL 必须显式 opt-in；
开放表面默认 reject；
surface_band 要求显式 thicknessMm；
重叠要求显式 priority，同级冲突失败；
white carrier 在最终 RGB 后执行，只写 W；
所有权逐层物化，禁止 dense 全层多材质栈；
生产接线依赖 MEMFLOW，不得旁路。
```

## 4. 证据分级

```text
A：当前代码、资产、配置、测试、构建脚本和实际命令输出；
B：MATVOL/Stage15/MEMFLOW 正式 Decision/DEV；
C：历史 DEV_06、聊天和临时诊断；
D：与当前代码冲突或已废弃的材料。
```

任何“已支持”“已完成”“性能提升”结论必须由 A 级证据支撑。

## 5. 每卡输出模板

```markdown
## Implementation Plan
### Problem Type
### Layer(s) Involved
### Official Documents
### Historical Documents
### AI Workspace Evidence
### Current Code Reality
### Current State
### Target State
### Historical State
### Pending Confirmation
### Risk Points
### Files To Change
### Verification Plan
```

实施后追加：变更、实际命令/结果、未运行 Gate、剩余风险、任务卡状态同步。

## 6. 验证分层

```text
L0 文档/配置/schema/diff check
L1 当前模块 Unit/Contract，MSVC /W4 /WX
L2 synthetic dense oracle 的逐层 owner/RGB/W diff
L3 旧 Profile Golden 与 Stage 15 回归
L4 Package/manifest/report/preview/RIP strict
L5 Host Profile/UI smoke
L6 Reality 03、取消/故障/恢复
L7 Release wall/CPU/Peak Working Set 与 RasterMemoryBudget
```

新实现不得与 expected 共用同一个 helper；否则不构成独立 oracle。

## 7. 停止并报告

命中以下任一项立即停止扩大范围，保留旧路径：

```text
需要修改 p0/SPI/Worker/RIP；
旧 Profile 像素或 hash 漂移；
当前卡依赖未完成或 INPUT OPEN；
open surface/overlap 被隐式猜测；
material owner 与 model mask 不闭合；
white carrier 修改 RGB/S/V/Z；
出现 dense 多材质层栈或预算溢出；
取消/失败后有半包；
无法与并行脏改安全合并。
```

## 8. 首张可执行卡

当前只有 **MV-01** 为 `PREPARED`。它只固化资产事实、fixtures、旧行为 baseline 和独立 oracle，
不新增生产 API、不修改 Profile、不接生产路径。执行 MV-02 及后续卡必须再次取得用户授权。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-20 | v1.0 | 创建可跨 AI 执行的读取顺序、红线、证据、模板、验证和停止规则。 |
