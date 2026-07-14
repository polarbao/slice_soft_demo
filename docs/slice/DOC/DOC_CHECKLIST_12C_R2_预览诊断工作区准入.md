# DOC_CHECKLIST_12C-R2 预览与诊断工作区准入

> 文档状态：Readiness Checklist / Stage 12C-R2
> 日期：2026-07-14

## 1. 准入结论

```text
12C-R0 fresh Qt build / smoke：COMPLETE；
12C-R1 Profile / Settings / generated config / help metadata：COMPLETE；
12C-R2 产品默认值与组件复用边界：FROZEN；
12C-R2-01 PreviewWorkspace：COMPLETE；
12C-R2-02 图例与像素探针：READY TO START。
```

R2 不需要修改 `slicer_core` 公共 API、Qt 版本、第三方依赖或生产 RGBWSV 协议，可以在现有 `apps/slicer_debug_ui` 边界内实施。

## 2. 当前 A 级代码事实

| 组件 | 当前数据源 | 当前层状态 | R2 复用方式 |
|---|---|---|---|
| `LayerPreviewPanel` | manifest、reports、生产 TIFF、preview | `LayerPreviewPackage.layerindices` | 继续作为生产层检查视图和共享层主范围 |
| `PreviewOverlayPanel` | `preview_report` 或 preview 文件 | 自有真实 `m_layerIndices` | 继续同层组合 RGB/W/S/V，不允许跨层查找 |
| `PreviewPanel` | `preview_report` 或 preview 文件 | 当前按可见图片序号 | 改为真实层号选择，缺图时显示同层缺失 |
| `MainWindow` | 三个独立顶级页签 | 无共享状态 | 改为单一 `PreviewWorkspace` 顶级入口 |

已确认 `output/UiSmokeOverlayRgbwv` 可作为 R2-01 稀疏层夹具：生产 TIFF 包含 layer 0 起的连续层，preview 从后续层开始。它能够验证“生产层存在但当前 preview 同层缺失”时不得跳到其他层。

## 3. 共享层契约

### 3.1 唯一共享标识

```text
共享状态只保存真实 layerIndex；
禁止把 slider position、图片序号或文件排序号当作共享层号；
生产层检查优先提供规范层范围；
生产层不可用时，才使用 overlay/raw 层号并集作为后备范围。
```

### 3.2 稀疏 preview 规则

```text
目标 layerIndex 有图：显示该层；
目标 layerIndex 无图：保持目标层号，显示“当前模式同层无图”；
禁止选择最近层；
禁止跨层寻找 RGB/W/S/V；
禁止因切换通道或模式重置到第 0 张图片。
```

### 3.3 信号与同步

```text
三个既有 panel 暴露 CurrentLayerIndex / LayerIndices / SelectLayer；
用户移动某个 panel 的层滑块时发送 SigLayerIndexChanged；
PreviewWorkspace 保存共享 layerIndex，并同步另外两个 panel；
工作区同步使用重入保护，避免信号循环；
模式切换只切换承载视图，不改变共享 layerIndex。
```

### 3.4 坐标与渲染

```text
继续使用现有“切片坐标 -> Qt 显示坐标”的垂直镜像；
不改 TIFF 读取、伪彩、缩放、像素探针和 overlay 合成算法；
R2-01 不实现 R2-02 图例/探针收口。
```

## 4. R2 原子任务边界

| 任务 | 准入 | 本阶段约束 |
|---|---|---|
| R2-01 PreviewWorkspace | COMPLETE | 统一入口、真实层共享、同层缺失，不做诊断 Dock |
| R2-02 图例/探针 | READY | 不改变生产像素语义 |
| R2-03 DiagnosticsDock | WAIT R2-02 | 只调整承载位置，不移动业务判断 |
| R2-04 OpenVDB 摘要 | WAIT R2-03 | 只读 utility report，固定非生产 |
| R2-05 Smoke/手册/报告 | WAIT R2-04 | 完成多尺寸布局和阶段验收 |

## 5. R2-01 文件影响面

```text
新增 apps/slicer_debug_ui/widgets/PreviewWorkspace.*；
增量扩展 LayerPreviewPanel / PreviewOverlayPanel / PreviewPanel 的共享层 API；
MainWindow 用 PreviewWorkspace 替代三个独立预览顶级页签；
UiSmokeTestRunner 新增 preview-workspace-shared-layer；
apps/slicer_debug_ui/CMakeLists.txt 注册新 widget；
更新 DEV、TASKS、REPORT 和上下文交接。
```

## 6. R2-01 验证门禁

```powershell
cmake --build build-12c-ui --config Debug --target slicer_debug_ui
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case preview-workspace-shared-layer --package output\UiSmokeOverlayRgbwv
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
.\build-12c-ui\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
ctest --test-dir build-12c-ui -C Debug --output-on-failure
git diff --check
```

专项 smoke 必须至少证明：

```text
三个模式切换后 CurrentLayerIndex 相同；
preview 存在层显示同层数据；
preview 缺失层仍保留目标 layerIndex；
缺失层状态明确包含“不跨层兜底”；
模式切换不改变共享 layerIndex。
```

## 7. 安全边界

```text
不修改 p0.rgbwsv.2；
不修改 R G B W S V / uint8 / black_is_print；
不新增或调整切片算法；
不默认启用 OpenVDB；
不把 utility/candidate 提升为 production，保持 productionReplacementAllowed=false；
不删除回归 fixture；
不提前实现 12D 材料闭环判断。
```

## 8. 最终判断

12C-R2 的需求、组件边界、共享层契约、稀疏 preview 行为、文件影响面和验证夹具均已明确。`12C-R2-01 PreviewWorkspace 与共享层状态` 已完成，下一任务为 `12C-R2-02 图例与像素探针收口`。
