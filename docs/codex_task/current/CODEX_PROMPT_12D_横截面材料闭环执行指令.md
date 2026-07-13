# CODEX_PROMPT_12D 横截面材料闭环执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
docs/slice/DOC/DOC_DECISION_12D_横截面材料无缝闭环专项.md
docs/slice/DOC/DOC_DECISION_12D_R0_R1_R2_R3_材料闭环阶段拆分.md
docs/slice/ROADMAP/ROADMAP_12D_材料闭环分阶段执行路线.md
docs/slice/PRD/PRD_12D_横截面材料无缝闭环验收与修复.md
docs/slice/DEV/DEV_12D_材料闭环诊断与修复设计.md
docs/slice/DOC/DOC_SCHEMA_12D_MaterialClosureReport.md
docs/slice/DEMO/DEMO_12D_横截面材料无缝闭环验证方案.md
docs/slice/DOC/DOC_MATRIX_12D_Fixture与验收矩阵.md
docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md
```

## 执行前提

```text
12C-R2-05 已完成并生成最终状态报告；
工作树现有用户修改已识别且不会被覆盖；
每次只执行用户明确指定的一个 12D 原子任务；
先写失败测试，再实现代码；
验证通过后单独提交并停止。
```

## 强制边界

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不默认启用 repair；
不默认启用 OpenVDB；
不从 preview PNG 判定生产闭环；
不允许 TIFF inferred candidate 显示 production pass；
不允许修复外部背景；
不允许自动修复 2px 及以上 gap。
```

## 原子任务顺序

```text
R1：12D-02 -> 12D-03 -> 12D-04；
R2：12D-05 -> 12D-06；
R3：12D-07 -> 12D-08 -> 12D-09 -> 12D-10。
```

## 验证要求

每个任务执行任务清单中的定向验证，并至少运行：

```powershell
cmake --build build --config Debug --target <affected-targets>
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

涉及 TIFF/package 时额外运行 `rip_reader_test`；涉及 Qt UI 时使用 12C fresh UI lane 和对应 smoke。

## 提交格式

```text
type(12D): 中文摘要

- 【模块】本任务实际修改
- 【验证】本次实际通过命令
- 【边界】RGBWSV、repair 默认关闭、OpenVDB 默认关闭情况
```

不得把计划命令写成已通过证据，不得把多个 12D 原子任务合并为一次提交。
