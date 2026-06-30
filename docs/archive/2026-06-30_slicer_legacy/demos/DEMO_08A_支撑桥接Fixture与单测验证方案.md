# DEMO_08A_支撑桥接Fixture与单测验证方案

> 文档版本：v0.1
> 适用阶段：08A
> 建议提交目录：`docs/slicer/`

## 1. 验证目标

验证 08 中已有 bridge gap 能力能够被专用 fixture 和 golden test 稳定覆盖。

## 2. 必须执行命令

```powershell
cmake --build build --config Debug

.\build\Debug\support_shape_unit_tests.exe

.\build\Debug\slicer_cli.exe --config samples\configs\support\support_bridge_gap_smoke.json

.\build\Debug\rip_reader_test.exe --package output\SupportBridgeGapSmoke --summary

.\scripts\run_support_shape_tests.ps1

.\scripts\run_schema_tests.ps1

.\scripts\run_golden_tests.ps1

.\scripts\run_ci_quick.ps1
```

## 3. 验收 Checklist

- [ ] `support_shape_unit_tests` 返回 0。
- [ ] `support_bridge_gap_smoke` package 生成成功。
- [ ] `rip_reader_test` PASS。
- [ ] `support_shape_report.schema = p0.support_shape_report.1`。
- [ ] `bridgedGaps.length > 0`。
- [ ] `addedSupportPixels > 0`。
- [ ] support pixels 不覆盖 model pixels。
- [ ] golden test 包含 bridge fixture。
- [ ] `run_ci_quick.ps1` 通过。
- [ ] 原 support_shape_smoke 仍通过。
