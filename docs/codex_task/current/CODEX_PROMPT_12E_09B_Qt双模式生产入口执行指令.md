# CODEX_PROMPT 12E-09B Qt 双模式生产入口执行指令

> 状态：09B-02 COMPLETE / START FROM 09B-03
> 日期：2026-07-24

## 1. 必读

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/DOC/DOC_DECISION_12E_09A_09B_Qt任务顺序与职责边界.md
docs/slice/DOC/DOC_DECISION_12E_Legacy与GlobalSurfaceShell双切片模式.md
docs/slice/PRD/PRD_12E_09B_Qt双模式生产入口与能力锁定.md
docs/slice/DEV/DEV_12E_09B_Qt双模式ProductionProfile设计.md
docs/slice/DEMO/DEMO_12E_09B_Qt双模式生产入口验证方案.md
docs/slice/DOC/DOC_PREP_12E_09B_Qt双模式生产入口准备.md
docs/codex_task/current/TASKS_12E_09B_Qt双模式生产入口任务清单.md
```

## 2. 执行规则

```text
只执行用户明确指定的一个 09B 原子任务；
开始前检查 branch/status 并保护无关 dirty state；
先读目标源码和测试，再修改；
优先复用现有 ConfigDocument、EffectiveConfigGenerator、preflight、ProcessRunner 和 preview/report；
Qt 不复制 core admission 规则；
每个任务运行定向验证和 git diff --check；
不自动开始下一任务；
不提交，除非用户或任务明确要求。
```

## 3. 产品红线

```text
Legacy 默认；
Global 显式选择；
Global 只开放 admitted Profile 能力；
OpenVDB 不是第三种产品模式；
Global 失败不自动 fallback；
repair 默认关闭；
不修改 p0.rgbwsv.2、R G B W S V、uint8、black_is_print；
不硬编码固定性能倍数为本次测量值；
复杂自相交模型继续 fail-closed。
```

## 4. 当前入口

```text
当前原子任务：12E-09B-03；
09A-02..06 是独立诊断支线；
09C 等待 09B-06 后处理 X/Y DPI；
12E-10 等待 09B、09C 收口及对应 09A preview 依赖。
```
