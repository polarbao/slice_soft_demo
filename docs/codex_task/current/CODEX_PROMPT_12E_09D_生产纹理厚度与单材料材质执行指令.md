# CODEX_PROMPT_12E-09D 生产纹理厚度与单材料材质执行指令

请按顺序阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
docs/slice/DOC/DOC_DECISION_12E_09D_生产纹理厚度与单材料材质收口.md
docs/slice/PRD/PRD_12E_09D_生产纹理厚度与单材料材质控制.md
docs/slice/DEV/DEV_12E_09D_生产纹理厚度与单材料材质控制设计.md
docs/slice/DEMO/DEMO_12E_09D_生产纹理厚度与单材料材质验证方案.md
docs/slice/DOC/DOC_PREP_12E_09D_生产纹理厚度与单材料材质收口准备.md
docs/codex_task/current/TASKS_12E_09D_生产纹理厚度与单材料材质任务清单.md
```

规则：

```text
1. 等 03D 当前优先任务完成且用户授权后，从 09D-01 开始。
2. 每次只执行用户明确授权的原子任务。
3. 诊断和生产参数必须保持不同身份。
4. Legacy 层数、Global mm 宽度和 allTexture 不得混写。
5. 单材料 W/V 必须由 resolver 原子生成完整字段。
6. 不实现 12G-TCWS、LibTIFF、支撑或新 OpenVDB admission。
7. 每个任务运行定向测试、UI Smoke、git diff --check。
8. 完成后更新 TASKS、REPORT、总览和上下文；未经验证不得写 COMPLETE。
```
