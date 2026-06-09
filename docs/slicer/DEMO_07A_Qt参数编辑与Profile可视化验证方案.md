# DEMO_07A_Qt参数编辑与Profile可视化验证方案

> 文档版本：v0.1  
> 建议目录：`docs/slicer/`

## 1. Demo 目标

验证 UI 能完成：

```text
编辑 materialProcessProfile
编辑 materialPolicy
编辑 materialRoleMapping
保存新 config
运行 slicer
查看 per-layer chart
查看 preview overlay
比较 profile
```

## 2. 启动方式

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe
```

self-test：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

## 3. 样例验收

### TopLayers 编辑

输入：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
```

操作：

```text
varnish.topLayers: 2 -> 3
Save As ui_generated_top3.json
Run Slicer
Load Package
```

验收：

```text
V activeLayerIndices 数量变为 3
V printPixels 增加
validation.pass = true
```

### MaterialRoleMapping 编辑

添加：

```text
matchNameContains = clear
role = varnish
```

验收：

```text
material_role_mapping_report.rules 包含 clear
mappedVarnish > 0
```

### Channel Chart

打开：

```text
output/NailRgbWhiteVarnishTop3
```

验收：

```text
RGB/W/V/S 曲线可见
```

### Preview Overlay

选择：

```text
RGB + V overlay
RGB + S overlay
```

验收：

```text
overlay 可显示
zoom / fit / layer slider 可用
```

## 4. 回归 Checklist

- [ ] UI 可启动。
- [ ] self-test 通过。
- [ ] Save As 可生成新配置。
- [ ] 非法 JSON 不保存。
- [ ] MaterialProcessProfileEditor 可用。
- [ ] MaterialPolicyEditor 可用。
- [ ] MaterialRoleMappingEditor 可用。
- [ ] ChannelChartPanel 可用。
- [ ] PreviewOverlayPanel 可用。
- [ ] Run Slicer 可用。
- [ ] Run RIP Summary 可用。
- [ ] Compare Profiles 可用。
- [ ] quick regression 通过。
