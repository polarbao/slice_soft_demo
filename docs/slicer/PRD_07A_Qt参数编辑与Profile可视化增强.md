# PRD_07A_Qt参数编辑与Profile可视化增强

> 文档版本：v0.1  
> 技术栈：C++20 / Qt 5.15 Widgets / CMake  
> 建议目录：`docs/slicer/`

## 1. 产品目标

07A 将 `slicer_debug_ui` 从“命令包装与报告查看器”增强为：

```text
参数编辑 + 工艺 profile 可视化 + 通道/层诊断工具
```

## 2. 必须支持

### 2.1 Config Form Editor

第一版必须覆盖：

- `materialProcessProfile`
- `materialPolicy`
- `materialRoleMapping`
- `support`
- `preview`

### 2.2 Config Save

支持：

- Save
- Save As
- Duplicate from current
- Revert
- Validate JSON

### 2.3 MaterialProcessProfile Panel

支持编辑：

```text
enabled
name
target
rgb.enabled
white.enabled / coverage / expandPx / shrinkPx
varnish.enabled / topLayers
support.expected
validation.require*
```

### 2.4 MaterialPolicy Panel

支持编辑：

```text
enabled
rgb
white
varnish
conflictPolicy
```

### 2.5 MaterialRoleMapping Panel

规则表格：

```text
matchNameContains | role | enabled
```

支持 Add / Remove / Move Up / Move Down。

### 2.6 Chart Panel

基于 `material_process_report.json` 显示：

```text
per-layer RGB / W / V / S printPixels
```

第一版用 QPainter 自绘，不强制 Qt Charts。

### 2.7 Preview Overlay

支持：

```text
single
RGB + W
RGB + V
RGB + S
validation overlay
```

## 3. 验收标准

1. UI 可编辑 `materialProcessProfile`。
2. UI 可编辑 `materialPolicy`。
3. UI 可编辑 `materialRoleMapping.rules`。
4. UI 可保存新 config。
5. UI 可基于新 config 运行 slicer。
6. UI 可显示 per-layer RGB/W/V/S 曲线。
7. UI 可显示 profile validation failures。
8. UI 可比较 Top1 / Top3 profile。
9. UI 可显示 preview overlay。
10. `run_regression.ps1 -Mode quick` 仍通过。
11. 不改变 slicer_core 输出协议。
12. 不破坏 07 已有功能。

## 4. 非目标

不做生产级任务系统、多设备调度、设备通信、RIP 半色调、ICC/CMYK、OpenVDB、新切片算法、完整 3D viewport。
