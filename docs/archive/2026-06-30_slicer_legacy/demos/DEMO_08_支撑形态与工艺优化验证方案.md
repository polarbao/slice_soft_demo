# DEMO_08_支撑形态与工艺优化验证方案

> 文档版本：v0.1  
> 阶段：08  
> 建议目录：`docs/slicer/`

## 1. 验证目标

验证支撑形态优化不会破坏 RGBWSV 协议，并能改善支撑组件统计。

## 2. 建议样例

新增：

```text
samples/configs/support/support_shape_smoke.json
```

输出：

```text
output/SupportShapeSmoke
```

配置启用：

```json
{
  "support": {
    "enabled": true,
    "mode": "bottom_plus_unsupported",
    "shape": {
      "enabled": true,
      "minComponentAreaPx": 16,
      "xyDilationPx": 1,
      "closingRadiusPx": 1,
      "bridgeGapPx": 2,
      "preserveModelPriority": true
    }
  },
  "preview": {
    "enabled": true,
    "channels": ["rgb", "support"]
  }
}
```

## 3. 必须执行命令

```powershell
cmake --build build --config Debug
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_shape_smoke.json
.\build\Debug\rip_reader_test.exe --package output\SupportShapeSmoke --summary
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

## 4. 验收 Checklist

- [ ] package 生成成功。
- [ ] `rip_reader_test` PASS。
- [ ] manifest 仍为 `p0.rgbwsv.2`。
- [ ] `support_shape_report.schema = p0.support_shape_report.1`。
- [ ] report 中有 pre/post component stats。
- [ ] 小组件过滤结果可见。
- [ ] bridge 或 dilation 结果可见。
- [ ] support pixels 不覆盖 model pixels。
- [ ] preview RGB+S 可打开。
- [ ] golden test 比较支撑摘要。
- [ ] run_ci_quick.ps1 通过。
