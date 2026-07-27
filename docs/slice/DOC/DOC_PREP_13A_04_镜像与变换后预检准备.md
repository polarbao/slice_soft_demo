# DOC_PREP 13A-04 镜像与变换后预检准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13A-03 COMPLETE（`22f4a46`）
> 后续：13A-05 WAIT 13A-04

## 1. 任务目标

在 13A-03 精确变换闭环后开放 `mirrorX/mirrorY`，并对实例有效几何重新执行模型预检。画布必须同时
显示 source admission 和 transformed admission；任一 required 模式 blocked 时，生产按钮保持阻断。

## 2. 固定边界

```text
镜像是 InstanceTransform，不修改模型源文件；
奇数轴镜像反转 winding 并同步 UV 顶点；
双轴镜像行列式为正；
镜像后重新计算有效 bbox、hash、revision 和预检；
不自动修复 confirmed self-intersection；
不把 Legacy PASS 推导为 Global PASS；
blocked 允许只读查看，不允许生产写包；
不修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print。
```

## 3. 当前能力与必须补齐项

13A-01 的 `TransformedModelAdapter` 已实现 mirror winding/UV 调换和 determinant metadata；
13A-02 已能显示 blocked；13A-03 已提供 repository、command/revision、异步重投影和 session config。

13A-04 应直接复用以下实际 API：

```text
SceneModelRepository::Find -> shared_ptr<const SceneModel>；
SceneDocument::Instance/SceneRevision/SourceCacheKey/CommitInstance；
SceneTransformController::SetTransform；
ModelTopViewLoader::RequestProjection；
SceneProjectionRequest；
ModelTransformPanel；
SceneTransformSaveRequest/SaveSceneEffectiveConfig。
```

当前 `ModelPreflightService` 主要从单模型 SliceConfig 重新导入模型，不能直接保证使用
`ModelInstance::effectiveTransform`。13A-04 必须新增或抽取一个无 Qt 的 transformed preflight 入口：

```text
输入：shared_ptr<const SceneModel>、ModelInstance、preflight options、generation/cancel；
处理：AdaptTransformedModel -> topology/finite/full audit -> mode admission；
输出：source/transformed result、identity/revision/hash；
禁止：从 UI 屏幕投影或旧 SliceConfig 推断实例几何。
```

优先复用现有诊断服务和 `EvaluateModelPreflightAdmissions`，不得复制拓扑算法。

## 4. 状态和并发

每次有效镜像：

```text
transformRevision +1；
sceneRevision +1；
SceneViewGeometry 进入 Loading/Stale；
transformed preflight 进入 Pending/Running；
切片动作暂时阻断；
只有最新 generation + sceneRevision + transformRevision 可发布；
完成后同时更新 view geometry、source/transformed admission 和 effective config dirty 状态。
```

用户在预检运行中再次修改时取消旧任务。取消或失败不允许继续使用上一个 transform 的 PASS 作为新
transform 的准入。

## 5. UI

在 `ModelTransformPanel` 增加 X/Y 镜像图标按钮和工具提示。状态区域显示：

```text
源模型：PASS/WARNING/BLOCKED；
变换后：PENDING/PASS/WARNING/BLOCKED；
Legacy admission；
Global admission；
稳定 blocker code；
scene/instance/revision。
```

生产动作 Gate 读取 transformed result，不从颜色或中文标签猜测。

## 6. 测试

计划 target：

```text
transformed_model_preflight_unit_tests
```

必测：

```text
mirrorX、mirrorY、双镜像；
winding、UV 和 bbox；
source mesh 不变；
transform/hash/revision；
镜像后 finite/topology/full self-intersection；
Legacy/Global 分别准入；
快速连续镜像只接受最新结果；
取消、失败、窗口关闭；
blocked 画布可见但生产按钮禁用；
保存/回读后 mirror 与 admission identity 一致。
```

真实模型：

```text
strict-PASS xiao_ma/yecan 作为正向；
confirmed self-intersection 浮雕资产作为 blocked 反向；
Texture2D 3MF 正向控制 UV/资源。
```

UI Smoke：

```text
--ui-smoke-test --case model-transform-preflight
```

## 7. 准备结论

需求、服务边界、实际 13A-03 Public API、执行指令和测试口径均已准备。13A-04 可在用户授权后开发；
仍不得并行实现 13A-05、多模型排版、自动修复或生产 TIFF 改造。
