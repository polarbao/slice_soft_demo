# DOC_DECISION_07A_REPORT07后进入Qt参数编辑与Profile可视化增强

> 文档版本：v0.1  
> 阶段：REPORT_07 之后  
> 建议目录：`docs/slicer/`

## 1. 阶段判断

`REPORT_07_Qt调试UI当前实现状态.md` 显示 07 已完成基础调试 UI：

- `slicer_debug_ui` 基础应用。
- config / package 选择。
- `slicer_cli` 执行。
- `rip_reader_test --summary` 执行。
- `run_regression.ps1 -Mode quick` 执行。
- `compare_material_profiles.ps1` 执行。
- manifest / reports 查看。
- preview PNG / PPM 查看。
- `material_process_report.json` summary 查看。
- 日志、stderr、exit code、耗时、`E_*` 错误码查看。
- `--self-test` 通过。
- CLI quick regression 通过。

因此 07 主功能可以收口。

## 2. 是否继续修改 07

不建议继续修改 07 主功能。当前未实现项属于增强能力，应拆为 07A：

- JSON 表单化参数编辑。
- `MaterialProcessProfile` 可视化编辑。
- white / varnish 参数面板。
- topLayers slider。
- per-layer 图表。
- preview overlay。
- channel 对比。

## 3. 下一阶段

建议进入：

```text
07A：Qt 参数编辑与 Profile 可视化增强
```

目标是从“运行工具 + 查看报告”升级为“编辑参数 + 可视化 profile + 快速对比”。

## 4. 冻结项

07A 不改变：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
MaterialRoleMapping 语义
MaterialPolicy 语义
MaterialProcessProfile 语义
Support pipeline 语义
```

## 5. 非目标

07A 不做设备通信、喷头 bitstream、RIP 半色调、ICC/CMYK、OpenVDB、新切片算法、生产级任务系统、完整 3D viewport。
