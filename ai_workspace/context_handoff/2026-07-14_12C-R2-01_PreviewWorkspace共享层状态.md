# 12C-R2-01 PreviewWorkspace 与共享层状态交接

> 日期：2026-07-14
> 状态：COMPLETE

## 1. 已完成

```text
新增 PreviewWorkspace，统一承载生产层检查、材料叠加和原始调试预览；
MainWindow 将三个预览顶级页签整合为单一“预览”入口；
三个既有 panel 提供真实层号读取、精确选择和 SigLayerIndexChanged；
PreviewWorkspace 使用真实 layerIndex 同步三个 panel；
PreviewPanel 不再把图片序号当层号；
raw/overlay 同层缺图时保持目标层并明确禁止跨层兜底；
新增 preview-workspace-shared-layer smoke。
```

## 2. 固定契约

```text
生产 LayerPreviewPanel 层范围优先作为规范范围；
模式切换不改变共享 layerIndex；
panel 滑块操作会回写并同步其他模式；
稀疏 preview 缺图不选择最近层；
overlay 仍只组合同层 RGB/W/S/V；
切片坐标到 Qt 显示坐标的镜像规则不变。
```

## 3. 未改变边界

```text
未修改 slicer_core、切片算法或 TIFF 读取协议；
未修改 p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print；
未默认启用 OpenVDB；
未实现 R2-02 图例/探针或 R2-03 DiagnosticsDock；
未实现 12D 材料闭环业务判断。
```

## 4. 下一任务

```text
12C-R2-02 图例与像素探针收口
```

R2-02 应在 `PreviewWorkspace` 上增量提供统一图例、生产值/显示值说明和六通道 probe 上下文，不得改变生产像素或伪彩生成规则。
