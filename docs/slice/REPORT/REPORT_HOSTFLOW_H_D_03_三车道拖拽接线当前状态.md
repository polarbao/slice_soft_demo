# REPORT HOSTFLOW H-D-03 三车道拖拽接线当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 分支：`feature/14-slicer-capability-package`

## 1. 任务结论

H-D-03 已把冻结的 Transient / Commit / Recovery 三车道合同接入参考宿主俯视画布。
鼠标移动只在宿主进程内完成实例拾取、矩阵预测和 QImage 重绘；松手时才通过公开 SPI
提交一次原子变换。实现未扩展 SPI v1、11 个导出或 15 项能力，也未读取切片核心内部对象。

## 2. 已实现内容

1. `TopViewCanvasWidget` 增加左键模型拖拽，保留中键平移、滚轮缩放和双击复位；
2. `TopViewRenderPolicy` 提供显示像素到 build-volume XY 的转换和本地实例拾取；
3. `MoveOptimizationPolicy` 以缓存 `TopViewFrame` 更新实例 `worldMatrix`，拖拽期不刷新 ViewData；
4. `SceneInteractionController` 可附着到宿主已持有的 scene identity，并暴露 Commit 的碰撞、越界摘要；
5. 松手只提交一次 `scene.apply_operation`，成功响应直接更新 revision 和 ViewData identity；
6. 正常 Commit 不追加 snapshot；Stale 才读取一次权威 snapshot，并丢弃本地瞬态帧；
7. 画布拾取会同步模型列表选择，Commit 后同步刷新 3D 视图和宿主状态摘要。

## 3. 机器证据

新增 `hostflow_hd03_drag_interaction`，使用真实带纹理甲片资产驱动可见 Qt 画布事件：

| 证据 | 结果 |
|---|---:|
| 连续 pointer-move | 12 次 |
| pointer-move 阶段 DLL 调用 | 0 次 |
| 松手 Commit | 1 次原子提交 |
| revision 变化 | 恰好 `+1` |
| 正常 Commit 后 snapshot | 0 次 |
| ViewData identity | 非空 |

既有 `slicer_stage14e03_interaction_contract_test` 同时覆盖 UI-M4：外部推进 revision 后，
本地旧版本 Commit 返回 Stale，控制器只恢复一次 snapshot，不重放旧操作。

## 4. 验证

Debug 与 Release 均完成以下 8 项联合门禁，结果分别为 8/8 PASS：

```text
slicer_source_size_guard_self_test
slicer_stage14e02_qt_host_boundary_test
slicer_stage14e03_interaction_contract_test
slicer_stage14e04_top_view_contract_test
slicer_stage14e04d_dual_view_contract_test
hostflow_hd01_top_canvas
hostflow_hd02_three_d_canvas
hostflow_hd03_drag_interaction
```

可见证据输出到：

```text
build/hostflow_hd03_evidence/<Debug|Release>/hostflow_hd03_drag_commit.png
```

`HostMainWindow.cpp` 当前 497 行，仍在 500 行守卫内；新增交互主体放在
`HostMainWindowView.cpp`，没有扩大主窗口单文件职责。

## 5. 边界与后续

- H-D-04 仍需统一导入、删除、变换、规则排版和 context 变更后的双视图刷新；
- H-D-06 仍须等待 H-D-04，并由人工走通七步流程和归档截图，自动化结果不能替代人工 PASS；
- 当前拖拽入口位于俯视画布；3D orbit/pan/zoom 仍保持相机操作，不与模型拖拽混用；
- 本任务未修改 `apps/slicer_debug_ui/**`、TIFF、RIP 或生产切片协议。
