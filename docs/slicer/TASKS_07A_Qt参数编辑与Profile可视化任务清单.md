# TASKS_07A_Qt参数编辑与Profile可视化任务清单

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## Milestone 07A-0：阅读确认

- [ ] 阅读 `REPORT_07_Qt调试UI当前实现状态.md`
- [ ] 阅读 07A 文档
- [ ] 确认不修改 slicer_core 输出协议
- [ ] 确认不做生产 UI
- [ ] 确认不做设备通信

## Milestone 07A-1：ConfigDocument

- [ ] 加载 JSON config
- [ ] 保留未知字段
- [ ] get/set helper
- [ ] dirty 状态
- [ ] Save / Save As / Revert

## Milestone 07A-2：ConfigValidator

- [ ] input.modelPath 校验
- [ ] output.packageDir 校验
- [ ] storageMode 校验
- [ ] varnish.topLayers 校验
- [ ] material role 校验
- [ ] support.mode 校验

## Milestone 07A-3：MaterialProcessProfileEditor

- [ ] enabled / profileName / target
- [ ] rgb.enabled
- [ ] white.enabled / coverage / expandPx / shrinkPx
- [ ] varnish.enabled / topLayers
- [ ] support.expected
- [ ] validation.require*

## Milestone 07A-4：MaterialPolicyEditor

- [ ] enabled
- [ ] rgb
- [ ] white
- [ ] varnish
- [ ] conflictPolicy

## Milestone 07A-5：MaterialRoleMappingEditor

- [ ] rules table
- [ ] Add Rule
- [ ] Remove Rule
- [ ] Move Up / Down
- [ ] role combo

## Milestone 07A-6：ChannelChartPanel

- [ ] 读取 material_process_report
- [ ] 绘制 per-layer RGB/W/V/S
- [ ] channel checkbox
- [ ] hover layer index

## Milestone 07A-7：PreviewOverlayPanel

- [ ] single channel
- [ ] RGB + W overlay
- [ ] RGB + V overlay
- [ ] RGB + S overlay
- [ ] layer slider / zoom / fit

## Milestone 07A-8：验证

- [ ] self-test 通过
- [ ] UI demo 通过
- [ ] CLI quick regression 通过
- [ ] 生成 `REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md`
