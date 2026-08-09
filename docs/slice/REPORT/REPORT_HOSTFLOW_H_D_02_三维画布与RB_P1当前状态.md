# REPORT HOSTFLOW H-D-02 三维画布与 RB-P1 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-09
> 分支：`feature/14-slicer-capability-package`

## 1. 任务结论

H-D-02 已把参考宿主的 3D 占位画布替换为真实纹理 ViewData 渲染链路，并优先完成
RB-P1。宿主仍只经公开 SPI 请求 `three_d` ViewData；CPU 光栅、相机、投影切换和画布
刷新均位于宿主进程内，不读取切片核心内部对象，也未修改 ABI、TIFF 或生产切片协议。

## 2. 已实现内容

1. `SceneRenderPolicy` 从固定 `lod2` 改为 `lod=auto`，默认预算保持 128 MiB；
2. `PM-SLICER-VIEWDATA-BUDGET` 显式显示错误码和预算，并清空旧帧，禁止破碎网格兜底；
3. 新增真实 `ThreeDCanvasWidget`，显示 `CpuRasterBackend` 生成的 RGBA 帧；
4. 支持左键 orbit、中/右键 pan、滚轮光标中心 zoom、适配场景；
5. 提供顶、底、前、后、左、右、等轴七向预设和正交/透视切换；
6. 导入成功后同时刷新俯视和 3D ViewData，纹理、UV、材质绑定错误继续 fail-closed；
7. `ThreeDFrame` 记录模块实际返回的 `lod0/lod1/lod2`，测试可审计自动 LOD 结果。

## 3. RB-P1 结果

RB-P1 消除了“预算未耗尽却固定进入 10k 跳采样”的宿主侧根因。真实资产矩阵逐个导入
`model/obj` 下全部 36 个 OBJ，并通过 `SceneRenderPolicy` 请求 3D 数据：

| 结果 | 数量 | 含义 |
|---|---:|---|
| 完整 `lod0` | 22 | 在 128 MiB 预算内保留完整网格和真实外观 |
| 显式预算拒绝 | 0 | 本轮单模型没有耗尽预算 |
| 显式资产拒绝 | 14 | 既有 OBJ/MTL 的贴图缺失或材质引用错误，返回 `PM-SLICER-INPUT-0001/0002` |
| 破碎降级显示 | 0 | 未把 `lod1/lod2` 跳采样结果当作本轮成功证据 |

14 个资产拒绝不是 RB-P1 回归。冻结合同要求缺少贴图、材质绑定或有效 UV 时显式失败，
不能成功显示灰模。对应资产应由后续 RENDER 资产治理任务修复。

## 4. UI-M7 证据

自动化验证在 ViewData 首次刷新完成后重置模块调用计数，再连续执行 orbit、pan、光标中心
zoom、七向预设和两种投影切换。13 次本地重绘期间 `cameraCalls=0`，证明相机交互没有
跨 DLL 调用。1 字节预算负例返回 `PM-SLICER-VIEWDATA-BUDGET`，拒绝帧不含任何实例。

## 5. 验证

```powershell
cmake --preset slicesoft-main
cmake --build build-slicesoft/main --config Debug --target slicer_ui_host_sim hostflow_hd02_three_d_canvas_tests --parallel 8
ctest --test-dir build-slicesoft/main -C Debug -R "hostflow_hd02|hostflow_h[ab]|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test|slicer_stage14e04c_three_d_contract_test|slicer_stage14e04d_dual_view_contract_test" --output-on-failure
cmake --build build-slicesoft/main --config Release --target slicer_ui_host_sim hostflow_hd02_three_d_canvas_tests --parallel 8
ctest --test-dir build-slicesoft/main -C Release -R "hostflow_hd02|hostflow_h[ab]|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test|slicer_stage14e04c_three_d_contract_test|slicer_stage14e04d_dual_view_contract_test" --output-on-failure
```

结果：Debug 相关联合门禁 22/22 PASS，追加当前源码构建和五项 3D/边界门禁 5/5 PASS；
Release 联合门禁 24/24 PASS。源码尺寸门禁确认 `HostMainWindow.cpp` 为 496 行，未增加白名单。

可见证据输出到：

```text
build-slicesoft/main/hostflow_hd02_evidence/<Debug|Release>/hostflow_hd02_textured_three_d.png
```

## 6. 边界与后续

- RB-P2（同模型多实例网格去重）、RB-P3（UV 缝安全的顶点共享）和 R-A-02 仍未执行；
- H-D-03 三车道拖拽与 H-D-04 场景变更自动刷新已解除 H-D-02 前置，可独立开工；
- H-D-06 仍须等待 H-D-03/04，并取得人工七步操作截图，不可由自动化测试推断 PASS；
- 本任务没有修改 `apps/slicer_debug_ui/**`，也没有扩展 SPI v1 的 11 个导出或 15 项能力。
