# HOSTFLOW H-D-02..06 视图后续任务准备审查

> 状态：**H-D-02/03/04 COMPLETE / H-D-06 WAIT HUMAN EVIDENCE**
> 日期：2026-08-10
> 基线：H-D-01、H-D-02、H-D-05 已完成；SPI v1、11 个导出、15 项能力保持冻结。

## 1. 审查结论

R-A-01 的原始 13.8k 口径已由 RB-P1 复核修正：参考宿主过去固定请求 `lod2`，使 35/36
真实 OBJ 在预算尚未耗尽时也进入 10k 跳采样。H-D-02 已先把请求改为 `lod=auto`，正常
预算内只接受完整 `lod0`；无法保留冻结纹理内容时显式返回 ViewData 错误并清空旧帧，
不显示灰模或破碎网格。RD-B 不再是 H-D-02 的硬前置，RB-P2/P3 和 R-A-02 仍作为后续
预算与性能治理保留。

| 任务 | 准备结论 | 当前动作 | 解锁条件 |
|---|---|---|---|
| H-D-02 3D 画布与相机 | **COMPLETE（2026-08-09）** | 维持 Debug/Release 与真实资产矩阵回归 | 无 |
| H-D-03 三车道拖拽 | **COMPLETE（2026-08-10）** | 维持 Debug/Release 与 UI-M1/UI-M4 回归 | 无 |
| H-D-04 刷新事件 | **COMPLETE（2026-08-10）** | 维持 22 实例与缓存失效回归 | 无 |
| H-D-05 包目录入口 | **COMPLETE** | 维持回归 | 无 |
| H-D-06 人工验收 | **PREPARED / WAIT HUMAN EVIDENCE** | 不生成虚假 PASS | 取得人工七步截图证据 |

H-D-03 已把拾取和拖拽接入真实俯视画布：pointer-move 只更新宿主缓存的
`worldMatrix` 和显示图像，松手时才提交一次 `scene.apply_operation`。自动化实测
12 次移动期间模块调用为 0，正常 Commit 仅推进一次 revision 且不追加 snapshot；
Stale 分支继续按冻结三车道合同读取一次权威快照并丢弃本地瞬态状态。

H-D-04 已把导入、删除、精确变换和规则排版统一到一对双视图刷新；删除最后实例时
清空画布而不是请求无实例 ViewData。22 实例规则排版和后续纯变换实测
`MeshUploadCount=0`、`TextureUploadCount=0` 增量，证明 `worldMatrix` 变化没有错误
失效不可变网格、纹理或 surface preview 缓存。

## 2. H-D-02 已实现合同

实现复用了 `SceneRenderPolicy`、`CpuRasterBackend` 与 `CameraController`，没有另造
ViewData 或读取内部场景 JSON。3D 视图满足：

1. 使用冻结 `three_d` ViewData 的 mesh、UV、submesh、materials、textures 与 appearances；
2. 纹理缺失、解码失败或外观绑定失败时显式失败，禁止成功灰模；
3. orbit、pan、zoom、正交/透视切换和七向预设均在宿主本地执行，跨 DLL 调用为 0；
4. 22 实例场景仍受 `maxBytes` 硬上限约束，使用 `auto` 选择而非强制 `lod0`；
5. 禁止 `outline_only` 伪装成 3D 成功结果。

下列临时方案均被否决：

- 强制 `lod0`：可能突破 ViewData 32 MiB 预算及实例上限；
- 保留三角形跳采样：真实甲片会出现破洞；
- 回退灰模或轮廓：违反冻结的纹理 fail-closed 与 `three_d` 合同；
- 静默换回俯视图：会把能力缺失伪装成 3D 成功。

## 3. H-D-04 刷新矩阵

| 事件 | 俯视刷新 | 3D 刷新 | 网格/纹理缓存 |
|---|:---:|:---:|---|
| import / add / remove | 是 | 是 | 仅新增或删除模型身份对应缓存 |
| transform Commit | 是 | 是 | `worldMatrix` 变化不得使 mesh/texture cache 失效 |
| applyGridLayout | 一次 | 一次 | 纯实例矩阵更新，`MeshUploadCount` 增量为 0 |
| sceneContext / buildVolume | 是 | 是 | 平台与网格失效；模型 mesh/texture 不失效 |
| Profile 参数变化 | 按显示语义决定 | 按显示语义决定 | 不得无条件清空几何缓存 |
| Stale Recovery | snapshot 后一次 | snapshot 后一次 | 使用恢复后的 scene/revision，丢弃旧帧 |

H-D-04 的代码可以在 H-D-02 完成后独立提交，但验收必须同时观察俯视和 3D，避免先形成
“俯视已刷新、3D 仍陈旧”的中间产品。

## 4. H-D-06 证据要求

H-D-06 必须人工走通导入、显示、选择、变换、排版、切片、打开生产包目录七步；记录
Debug/Release、scene/revision、Profile、模型资产、截图和失败路径。没有人工证据时只能写
`NOT RUN`，不得根据自动化测试推断人工可操作性 PASS。

## 5. 验证门禁

H-D-02 实际执行：

```powershell
cmake --build build-slicesoft/main --config Debug --target slicer_ui_host_sim hostflow_hd02_three_d_canvas_tests --parallel 8
ctest --test-dir build-slicesoft/main -C Debug -R "hostflow_hd02|hostflow_h[ab]|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test|slicer_stage14e04c_three_d_contract_test|slicer_stage14e04d_dual_view_contract_test" --output-on-failure
cmake --build build-slicesoft/main --config Release --target slicer_ui_host_sim hostflow_hd02_three_d_canvas_tests --parallel 8
ctest --test-dir build-slicesoft/main -C Release -R "hostflow_hd02|hostflow_h[ab]|slicer_stage14e02_qt_host_boundary_test|slicer_source_size_guard_self_test|slicer_stage14e04c_three_d_contract_test|slicer_stage14e04d_dual_view_contract_test" --output-on-failure
```

Debug/Release 3D 画布验证均得到 `lod=lod0 / cameraCalls=0`，1 字节预算负例得到
`PM-SLICER-VIEWDATA-BUDGET` 并保持空帧。Release 真实资产矩阵覆盖 36 个 OBJ：22 个完整
`lod0`，14 个因既有贴图/材质资产错误得到 `PM-SLICER-INPUT-0001/0002` 显式拒绝，
0 个以跳采样破碎网格显示。Debug 相关联合门禁 22/22 PASS，当前源码构建后追加 3D/边界
门禁 5/5 PASS；Release 联合门禁 24/24 PASS。资产错误修复与 RB-P2/P3 不在 H-D-02 范围内。
