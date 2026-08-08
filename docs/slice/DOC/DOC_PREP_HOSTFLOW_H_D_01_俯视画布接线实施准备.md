# H-D-01 俯视画布接线实施准备

> 状态：**PREPARATION_GATE = PASS**
> 日期：2026-08-09
> 任务：HOSTFLOW H-D-01
> 权威决策：`DOC_DECISION_HOSTFLOW_H_D_R1_视图接线归属与14E_04d延期作废.md`

## 1. 目标与边界

H-D-01 只负责把参考宿主的 `topCanvas` 从静态占位控件替换为真实俯视画布：

- 数据来自当前 `sceneHandle/sceneRevision` 的冻结 `top` ViewData；
- 复用既有 `TopViewRenderPolicy` 生成 `QImage`；
- 展示纹理、buildVolume、1 mm/10 mm 网格；
- 缩放和平移只操作宿主持有的图像，不产生 DLL 调用；
- 纹理、ViewData 或渲染失败时清空旧帧并显式报错。

本任务不实现 3D、拾取拖拽、自动刷新矩阵，也不修改 SPI、能力数量、RGBWSV/TIFF 协议。

## 2. 当前事实

| 层级 | 当前状态 | H-D-01 处理 |
|---|---|---|
| ABI | SPI v1、11 导出、15 能力已冻结 | 不改 |
| ViewData | `top` 已提供真实 `surfacePreview`、实例矩阵和平台数据 | 直接消费 |
| 渲染策略 | `TopViewRenderPolicy` 已有刷新、纹理 fail-closed、缓存和 QImage 合成 | 直接复用 |
| 宿主 UI | `topCanvas` 仍为静态 `QLabel` | 替换为专用画布 |
| 宿主生命周期 | 导入成功后已有权威 scene handle/revision | 在导入完成点刷新 |
| 行数门禁 | `HostMainWindow.cpp` 不允许继续承载大段接线逻辑 | 新建 `HostMainWindowView.cpp` |

## 3. 调用与所有权

```text
导入成功
  -> HostMainWindow::RefreshTopView
  -> TopViewRenderPolicy::Refresh(sceneHandle, sceneRevision)
  -> TopViewRenderPolicy::Render(frame, renderSize)
  -> ViewWorkspaceWidget::SetTopImage
  -> TopViewCanvasWidget 本地 paint/pan/zoom
```

- `HostMainWindow` 独占 `TopViewRenderPolicy` 生命周期；
- `ViewWorkspaceWidget` 拥有 `TopViewCanvasWidget`；
- 画布只保留完整 `QImage`，不持有模块对象或裸 ViewData 指针；
- wheel、pan、reset 均不得调用 `ModuleClient`；
- 失败分支先 `ClearTopImage()`，禁止继续显示上一场景图像。

## 4. 变更范围

| 文件 | 目的 |
|---|---|
| `TopViewCanvasWidget.h/.cpp` | 图像显示、本地缩放和平移 |
| `ViewWorkspaceWidget.h/.cpp` | 用真实画布替换俯视占位控件 |
| `HostMainWindowView.cpp` | 承载 ViewData 刷新和渲染接线 |
| `HostMainWindow.h/.cpp/.State.cpp` | 生命周期、导入后刷新及完整类型闭合 |
| `CMakeLists.txt` | 注册源码和 H-D-01 门禁 |
| `HostTopViewCanvasTests.cpp` | 真实纹理模型、可见截图和零 DLL 导航证据 |

明确不修改 `apps/slicer_debug_ui/**`，也不把宿主目录加入源码尺寸白名单。

## 5. 风险与处置

1. **LOD 跳采样风险**：由同批 R-A-01 实测。该缺陷影响后续 3D mesh，俯视图使用
   `surfacePreview`，因此不阻断 H-D-01。
2. **纹理失败误显示灰模**：沿用 `TopViewRenderPolicy` 的 fail-closed，并在宿主层清空旧图。
3. **交互误跨模块**：单测在缩放/平移前清零调用计数，操作后必须保持 0。
4. **Debug/Release 行为差异**：两种配置均构建并运行相同门禁。

## 6. 验证门禁

```text
cmake --build build-slicesoft/main --config <Debug|Release> \
  --target slicer_ui_host_sim stage14e04d_view_switch_tests \
           hostflow_hd01_top_canvas_tests --parallel 8

ctest --test-dir build-slicesoft/main -C <Debug|Release> \
  -R "hostflow_hd01_top_canvas|slicer_stage14e04d_dual_view_contract_test" \
  --output-on-failure

python tests/stage14e_02/ValidateQtHostBoundary.py --repo-root . \
  --binary <slicer_ui_host_sim.exe>
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
```

准备结论：需求、契约、代码插入点、所有权、失败语义和验证命令均已闭合，允许进入开发。
