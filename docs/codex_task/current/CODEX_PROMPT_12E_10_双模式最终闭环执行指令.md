# CODEX PROMPT 12E-10 双模式最终闭环执行指令

## 1. 必读

```text
AGENTS.md
.agents/AGENTS.md
docs/slice/PRD/PRD_12E_10_双模式最终闭环.md
docs/slice/DEV/DEV_12E_10_双模式最终闭环设计.md
docs/slice/DEMO/DEMO_12E_10_双模式最终闭环验证方案.md
docs/slice/DOC/DOC_PREP_12E_10_双模式最终闭环准备.md
docs/slice/DOC/DOC_SCHEMA_12E_FinalClosureMatrix.md
docs/codex_task/current/TASKS_12E_10_双模式最终闭环任务清单.md
```

## 2. 执行规则

```text
只执行用户明确指定的一个 10A/10B/10C/10D；
开始前核对前置状态和工作树；
Legacy 与 Global 必须显式分开；
禁止 silent fallback；
每个生产成功 case 必须有 TIFF、manifest、report 和 RIP strict；
blocked case 不生成假 package；
实际未运行的验证不得写 PASS；
完成一个原子任务后停止。
```

## 3. 固定边界

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 或 black_is_print；
不把 OpenVDB 设为默认；
不吸收 Stage 13 多模型生产 Gate；
不实施 12G-TCWS；
不关闭未提供的设备输入。
```

## 4. 当前入口

```text
12E-09A-05/06、09D 和 10A 已完成；
10A 的生产 TIFF / 09A 语义 / WSV / 精确闭环同层 Gate 已通过；
10B 已完成 14 行生产 PASS、3 行 BLOCKED_EXPECTED 和固定矩阵；
当前下一任务为 10C，技术和文档前置已具备；
10D 等待 10A/10B/10C。
```
