# DOC_DECISION_07B_REPORT07A后进入UI自动化SmokeTest与配置编辑器收口

> 文档版本：v0.1  
> 阶段：REPORT_07A 之后  
> 建议目录：`docs/slicer/`

## 1. 阶段判断

07A 已完成 Qt 参数编辑与 Profile 可视化第一版：`ConfigDocument / ConfigValidator`、配置编辑面板、MaterialProcessProfile/MaterialPolicy/MaterialRoleMapping/Support 编辑器、ChannelChartPanel、PreviewOverlayPanel，并通过 build、self-test、quick regression 与 `slicer_cli + rip_reader_test` 验证。

07A 保持不变：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
MaterialPolicy / MaterialRoleMapping / MaterialProcessProfile 执行语义不变
```

因此 07A 主功能可以收口。

## 2. 下一阶段建议

建议进入：

```text
07B：UI 自动化 Smoke Test 与配置编辑器收口
```

原因：07A 仍有工程收口点：

```text
Save 覆盖前二次确认未实现
Save As / Overlay / Chart 未做脚本化 UI smoke test
PreviewOverlayPanel 的 preview_report schema 仍是基础兼容实现
Config 编辑器缺少配置差异预览和更完整的枚举下拉
```

## 3. 07B 定位

07B 是 Qt 调试 UI 工程质量收口阶段，不是新切片算法阶段。

07B 目标：

```text
让 slicer_debug_ui 的关键交互可脚本化验证；
让配置编辑器具备覆盖保护、字段枚举、差异预览；
让 PreviewOverlayPanel 的数据源 schema 更稳定；
为 08 支撑形态优化提供稳定 UI 工具。
```

## 4. 非目标

不做设备通信、RIP 半色调、ICC/CMYK、OpenVDB、新切片算法、生产级任务系统、完整 3D viewport、支撑形态算法修改。
