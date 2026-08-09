# HOSTFLOW H-D-02..06 视图后续任务准备审查

> 状态：**PARTIAL READY / H-D-02 BLOCKED BY R-B DECISION**
> 日期：2026-08-09
> 基线：H-D-01、H-D-05 已完成；SPI v1、11 个导出、15 项能力保持冻结。

## 1. 审查结论

H-D 后续任务不能按编号直接连续开发。R-A-01 已确认 36 个真实 OBJ 中有 17 个超过
当前约 13,800 三角面/实例的 ViewData 预算阈值，现有 `SceneViewMeshBuilder` 会用三角形
跳采样降级，可能产生破洞。因此 H-D-02 若直接把现有 3D mesh 接到可见画布，会把已知
P1 缺陷变成生产可见行为。

| 任务 | 准备结论 | 当前动作 | 解锁条件 |
|---|---|---|---|
| H-D-02 3D 画布与相机 | **BLOCKED** | 不进入源码开发 | R-B-02 完成简化方案裁决与门禁；R-B-01 同批修正多材质预算 |
| H-D-03 三车道拖拽 | **BLOCKED** | 只保留既有合同 | H-D-02 完成并通过相机零 DLL 调用门禁 |
| H-D-04 刷新事件 | **PREPARED / WAIT H-D-02** | 冻结事件和缓存规则，不提前提交半套实现 | H-D-02 完成；可同时验证 3D `MeshUploadCount` |
| H-D-05 包目录入口 | **COMPLETE** | 维持回归 | 无 |
| H-D-06 人工验收 | **BLOCKED** | 不生成虚假 PASS | H-D-01..05 全部完成并取得人工截图证据 |

## 2. H-D-02 开工合同

开工后必须复用 `SceneRenderPolicy`、`CpuRasterBackend` 与 `CameraController`，不得另造
ViewData 或直接读取内部场景 JSON。3D 视图必须满足：

1. 使用冻结 `three_d` ViewData 的 mesh、UV、submesh、materials、textures 与 appearances；
2. 纹理缺失、解码失败或外观绑定失败时显式失败，禁止成功灰模；
3. orbit、pan、zoom、正交/透视切换和七向预设均在宿主本地执行，跨 DLL 调用为 0；
4. 22 实例场景仍受 `maxBytes` 硬上限约束，禁止用 `lod0` 强行绕开预算；
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

R-B 解锁后，H-D-02 至少执行：

```powershell
cmake --build --preset debug --target slicer_ui_host_sim
ctest --preset debug -R "hostflow_hd02|hostflow_h[ab]|qt_ui_host_boundary" --output-on-failure
cmake --build --preset release --target slicer_ui_host_sim
ctest --preset release -R "hostflow_hd02|hostflow_h[ab]|qt_ui_host_boundary" --output-on-failure
```

实际 target/preset 以当时仓库已有名称为准；不得为让命令通过而弱化既有门禁。
