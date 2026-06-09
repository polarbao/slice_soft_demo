# DEMO_07B_UI自动化SmokeTest验证方案

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## 1. 验证命令

```powershell
cmake --build build --config Debug --target slicer_debug_ui

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case startup

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case save-as-config --config samples\configs\material_process\nail_rgb_white_varnish_top2.json --output output\ui_smoke_generated_top3.json --yes

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case chart-load --package output\NailRgbWhiteVarnishTop3

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load --package output\NailRgbWhiteVarnishTop3

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case compare-profiles --package-a output\NailRgbWhiteVarnishTop1 --package-b output\NailRgbWhiteVarnishTop3 --output output\MaterialProfileCompare_ui_smoke.json

.\scripts\run_regression.ps1 -Mode quick
```

## 2. 验收 Checklist

- [ ] self-test 通过。
- [ ] startup smoke test 通过。
- [ ] save-as-config 生成新 config。
- [ ] chart-load 能读取 material_process_report layers。
- [ ] overlay-load 能找到 preview 图像。
- [ ] compare-profiles 能生成 compare JSON。
- [ ] Save 覆盖确认在交互模式可用。
- [ ] PreviewReportIndex 能解析标准 schema。
- [ ] fallback 到旧 preview_report 字段仍可用。
- [ ] quick regression 通过。

## 3. 状态报告

完成后生成：

```text
docs/slicer/REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md
```
