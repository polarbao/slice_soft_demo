# CODEX_PROMPT_07B_R1_UI真实OverlaySmoke修正执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md
docs/slicer/DOC_DECISION_07B_R1_UI真实OverlaySmoke与预览Fixture修正.md
docs/slicer/TASKS_07B_R1_UI真实OverlaySmoke修正任务清单.md
docs/slicer/DEMO_07B_R1_UI真实OverlaySmoke验证方案.md
```

当前任务：

```text
07B-R1：UI 真实 Overlay Smoke Fixture 与 ConfigDiff 小收口
```

目标：

```text
1. 新增 preview.enabled=true 的 UI smoke fixture；
2. 新增或增强 overlay-load-real smoke test；
3. 自动验证真实 preview 图像与 RGB+W/V/S overlay；
4. 确认 preview_report schema = p0.preview_report.1；
5. 可选增强 ConfigDiff copy/export/filter；
6. 生成 REPORT_07B_R1。
```

必须保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
MaterialPolicy 语义不变
MaterialRoleMapping 语义不变
MaterialProcessProfile 语义不变
Support pipeline 语义不变
```

不要做：

```text
支撑形态算法修改
设备通信
RIP 半色调
ICC / CMYK
OpenVDB
新的切片算法
生产级任务系统
完整 3D viewport
```

必须执行验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
.\scripts\run_regression.ps1 -Mode quick
```

完成后生成：

```text
docs/slicer/REPORT_07B_R1_UI真实OverlaySmoke与配置编辑器小收口当前实现状态.md
```

报告最后明确是否建议进入：

```text
08：支撑形态与工艺优化
```
