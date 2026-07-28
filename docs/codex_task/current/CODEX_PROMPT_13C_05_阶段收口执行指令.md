# CODEX PROMPT 13C-05 阶段收口执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/build-and-test.md
docs/slice/REPORT/REPORT_13C_03_UnifiedProductionPreview当前状态.md
docs/slice/REPORT/REPORT_13C_04_PreviewIO收口当前状态.md
docs/slice/DOC/DOC_PREP_13C_05_阶段收口准备.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
```

现在只执行 `13C-05 阶段收口`。

要求：

1. 不新增生产预览模式，不修改材料策略。
2. 完成 stripped/tiled、无 preview、635/600、全材料、探针和错误矩阵。
3. 生产/诊断入口、异步生命周期和 fail-closed 行为必须回归。
4. 所有验收 package 必须由共享 writer 生成并通过 RIP strict。
5. 保持 `p0.rgbwsv.2`、R G B W S V、uint8、black_is_print。
6. 生成 `REPORT_13C_TIFF原生统一预览阶段收口.md`。
7. 同步 Stage 13 TASKS、依赖矩阵、ROADMAP、README、12X/13 总览和用户手册。
8. 只记录实际运行的验证结果。
9. 本任务独立提交，不夹带 13B-08、13D、09A 或其他并行改动。

建议提交标题：

```text
docs(13C-05): 【阶段收口】固化TIFF原生统一预览证据链
```
