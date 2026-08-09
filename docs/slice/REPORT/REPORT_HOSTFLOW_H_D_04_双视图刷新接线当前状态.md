# REPORT HOSTFLOW H-D-04 双视图刷新接线当前状态

> 状态：**COMPLETE**
> 日期：2026-08-10
> 分支：`feature/14-slicer-capability-package`

## 1. 任务结论

H-D-04 已统一参考宿主的场景变更刷新入口。导入、删除、精确变换和规则排版成功后，
宿主只执行一对俯视/3D 权威刷新；删除最后实例时直接清空双视图。刷新继续使用公开
`scene.get_snapshot` 和 `scene.get_viewdata`，没有读取模块内部场景或绕过 revision。

## 2. 刷新矩阵

| 场景事件 | 俯视 | 3D | 不可变资源行为 |
|---|:---:|:---:|---|
| import / add | 刷新一次 | 刷新一次 | 新身份按需加载 |
| remove | 刷新一次 | 刷新一次 | 最后实例删除后清空画布 |
| transform Commit | 刷新一次 | 刷新一次 | 网格、纹理、surface preview 不失效 |
| applyGridLayout | 刷新一次 | 刷新一次 | 仅更新实例矩阵 |
| H-D-03 拖拽 Commit | 本地权威帧直接采用 | 刷新一次 | 正常 Commit 不追加 snapshot |
| Stale Recovery | 恢复后刷新 | 恢复后刷新 | 丢弃旧瞬态帧 |

Profile 与 buildVolume 在首个实例入场前属于宿主草稿，此时没有可刷新的场景；场景创建后
上下文保持冻结，非法修改 fail-closed，不制造伪刷新。

## 3. 22 实例缓存证据

新增 `hostflow_hd04_scene_refresh`，重复导入真实纹理 fixture 形成最大 11×2 场景，先加载
双视图，再执行规则排版和单实例纯平移：

```text
instances=22
layoutRevision=23
transformRevision=24
meshUploadDelta=0
textureUploadDelta=0
```

俯视帧和 3D 帧都在每次 Commit 后采用当前 revision；surface preview 缓存数量不变。
这证明实例 `worldMatrix` 变化不会使不可变 mesh、texture 或 preview identity 失效。

## 4. 验证

Debug 与 Release 均完成以下 8 项联合门禁，结果分别为 8/8 PASS：

```text
slicer_source_size_guard_self_test
slicer_stage14e02_qt_host_boundary_test
slicer_stage14e03_interaction_contract_test
slicer_stage14e04d_dual_view_contract_test
hostflow_hd01_top_canvas
hostflow_hd02_three_d_canvas
hostflow_hd03_drag_interaction
hostflow_hd04_scene_refresh
```

Debug 的 22 实例刷新门禁耗时约 35 秒，Release 约 4.4 秒；差异来自 Debug 构建的模型
导入与 ViewData 解码开销，不改变缓存增量结论。`git diff --check` 通过。

## 5. 边界与后续

- H-D-06 的代码前置现已完成，但仍须人工走通七步流程并归档截图，当前不能写 PASS；
- 本任务不改变 ViewData DTO、SPI v1、TIFF、RIP 或生产切片协议；
- 本任务没有修改 `apps/slicer_debug_ui/**`；
- RB-P2/P3 仍负责跨实例网格 DTO 去重与 UV 缝安全顶点共享，不能混入刷新任务。
