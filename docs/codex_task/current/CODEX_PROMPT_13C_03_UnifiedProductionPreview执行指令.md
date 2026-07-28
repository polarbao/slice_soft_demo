# CODEX PROMPT 13C-03 Unified Production Preview 执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/project-profile.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
.agents/docs/code-standards.md
docs/slice/PRD/PRD_13C_RGBWSV_TIFF原生统一预览.md
docs/slice/DEV/DEV_13C_TIFFLayerSource与统一材料合成设计.md
docs/slice/DEMO/DEMO_13C_TIFF原生统一预览验证方案.md
docs/slice/DOC/DOC_PREP_13C_03_UnifiedProductionPreview准备.md
docs/slice/REPORT/REPORT_13C_01_TIFFLayerSource与Cache当前状态.md
docs/slice/REPORT/REPORT_13C_02_MaterialPreviewComposer当前状态.md
```

现在只执行 `13C-03 Unified Production Preview`。

要求：

1. 复用现有 `PreviewWorkspace` 和 `LayerPreviewPanel`，不要新增重复的第四个预览 Panel。
2. 生产预览只读取 manifest 列出的同层 RGBWSV TIFF。
3. 通过 `TiffLayerLoadWorker` 异步切层；禁止 UI 主线程同步解码 TIFF。
4. 使用 `MaterialPreviewComposer` 提供 R/G/B/W/S/V、RGB 组合、RGB+S+W+V、Occupancy 和 Empty。
5. 模式切换复用当前六通道 buffer，不重复读 TIFF。
6. 一级入口收敛为“生产预览/诊断预览”；旧 Overlay/Raw 保留在诊断入口。
7. 保持真实 layerIndex、zMm、dpiX/dpiY 和低 Z 到高 Z。
8. QImage 只在显示边界执行一次 Y 翻转；像素探针必须映射回 TIFF raw 坐标并调用 core Probe。
9. 快速滑层必须拒绝 stale generation；失败不得跨层兜底或保留旧图伪装成功。
10. 新增 `tiff-native-preview-all-materials` UI smoke，并保留现有 preview/overlay smoke。
11. 不关闭 preview PNG；13C-04 才处理 IO。
12. 不修改 `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print`、Legacy 默认或 OpenVDB 边界。
13. 遵循项目 C++/Qt 命名、Allman、函数指针 connect 和 Doxygen 规则。
14. 完成后生成 `docs/slice/REPORT/REPORT_13C_03_UnifiedProductionPreview当前状态.md`。
15. 实际运行准备文档列出的定向、UI、全量和 Quick CI Gate；未运行不得声称通过。
16. 通过后按仓库中文 `【模块】` 风格原子提交，不提交 `docs/claude`。
