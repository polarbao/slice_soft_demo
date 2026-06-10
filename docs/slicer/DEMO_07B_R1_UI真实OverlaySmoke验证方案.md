# DEMO_07B_R1_UI真实OverlaySmoke验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：07B-R1  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证 UI overlay smoke test 不再只覆盖 graceful-empty-preview，而是真正加载 preview 图像并完成 overlay 组合。

---

## 2. 验证命令

```powershell
cmake --build build --config Debug --target slicer_debug_ui

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test

.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv

.\scripts\run_regression.ps1 -Mode quick
```

---

## 3. 验收标准

- [ ] `ui_overlay_rgbwv_preview.json` 能生成 output package。
- [ ] package 中存在 preview 图像。
- [ ] package 中存在 `reports/preview_report.json`。
- [ ] `preview_report.schema = p0.preview_report.1`。
- [ ] `PreviewReportIndex` 可读取 channel/layer/kind。
- [ ] `overlay-load-real` 返回 0。
- [ ] `PreviewOverlayPanel.imageCount() > 0`。
- [ ] 至少一种 overlay 模式可 compose 非空图像。
- [ ] quick regression 通过。
- [ ] 生产 TIFF 协议不变。
