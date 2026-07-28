# CODEX PROMPT 13C-04 Preview IO 收口执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/SLICE_AI_SKILL_MASTER.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/REPORT/REPORT_13C_03_UnifiedProductionPreview当前状态.md
docs/slice/DOC/DOC_PREP_13C_04_PreviewIO收口准备.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
```

现在只执行 `13C-04 Preview IO 收口`。

硬性要求：

1. `preview.outputPolicy` 只允许 `tiff_native` 和 `tiff_native_with_diagnostics`。
2. 新配置、UI 默认 `tiff_native`，即不自动写逐通道诊断图。
3. 旧 `preview.enabled` 必须按准备文档迁移，显式新策略优先。
4. preview 配置不得控制 `layers/*.tiff`。
5. `p0.rgbwsv.2`、R G B W S V、uint8、black_is_print 不变。
6. 无 preview 目录时，生产预览必须从 TIFF 完整显示。
7. 诊断策略开启时，现有 RGB/W/S/V 图仍可写出。
8. preview report 保留 `p0.preview_report.1`，文件列表允许为空。
9. 增加 config/writer 单测和 `tiff-native-preview-no-png` UI smoke。
10. 记录同一 fixture 开关诊断图前后的 IO 文件数、字节数和 timing。
11. 不实现按需导出 UI，不删除旧 Overlay/Raw，不修改生产材料策略。
12. 完成后生成 `docs/slice/REPORT/REPORT_13C_04_PreviewIO收口当前状态.md`。
13. 运行准备文档规定的全量验证；未运行的命令不得写成 PASS。
14. 本任务独立提交，不夹带 13B-08、13C-05、13D 或其他并行工作树改动。

建议提交标题：

```text
feat(13C-04): 【预览IO】默认关闭重复诊断图写出
```
