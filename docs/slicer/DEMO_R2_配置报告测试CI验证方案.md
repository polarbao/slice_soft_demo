# DEMO_R2_配置报告测试CI验证方案

> 文档版本：v0.1  
> 文档状态：Demo / 验证方案  
> 适用阶段：R2  
> 建议提交目录：`docs/slicer/`

## 1. 验证目标

```text
配置 schema 可识别
legacy config 可兼容
report schema 可检查
golden summary 可回归
CI quick guard 可一键运行
UI smoke test 不退化
输出协议不变
```

## 2. 必须验证命令

```powershell
cmake --build build --config Debug

.\scripts\run_regression.ps1 -Mode quick

.\scripts\run_schema_tests.ps1

.\scripts\run_golden_tests.ps1

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv

.\scripts\run_ci_quick.ps1
```

## 3. 样例要求

新增一个新 schema 配置：

```text
samples/configs/schema_v1/slicer_config_v1_basic.json
```

继续验证 legacy 配置：

```text
samples/configs/slice_config.json
samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json
samples/configs/material_process/nail_rgb_white_varnish_top2.json
```

## 4. 验收标准

```text
slicer.config.1 样例可运行
legacy config 仍可运行
schema tests 返回 0
golden tests 返回 0
run_ci_quick.ps1 返回 0
quick regression 通过
UI self-test 通过
overlay-load-real 通过
rip_reader_test 仍识别 p0.rgbwsv.2
report 兼容现有 UI
```
