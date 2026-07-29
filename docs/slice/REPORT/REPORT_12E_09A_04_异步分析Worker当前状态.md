# REPORT 12E-09A-04 异步分析 Worker 当前状态

> 状态：COMPLETE / 12E-09A-05 READY
> 日期：2026-07-29

## 1. 完成内容

本任务已把当前场景实例的纹理/填充诊断移入 `QThreadPool`，主线程只生成不可变请求、显示状态并
接收终态结果。实际诊断链路为：

```text
AdaptTransformedModel
  -> AdaptSceneModelToTriangleMesh
  -> RunTextureFillPartitionReleaseBenchmark
  -> Legacy CPU occupancy/distance/partition
  -> texture transfer
  -> raster mapping
  -> full closure diagnostic linkage
```

UI 的“切片设置/纹理与填充诊断”现提供“开始诊断”和“取消诊断”。运行结果回显模型最大宽度、
全纹理阈值、Texture Surface/Model Fill voxel 数及核心耗时。

## 2. 身份与事务

每次运行先写入并回读：

```text
output/ui_sessions/<diagnostic-session>/
  slice_config.diagnostic.effective.json
```

Worker 请求冻结：

```text
sessionId；
sceneId / sceneRevision；
modelId / instanceId / transformRevision；
configHash；
源模型 shared ownership；
实例变换、纹理采样、宽度、填充材料和分类分辨率。
```

重复运行使用 generation 防旧结果覆盖；场景、实例、变换或诊断参数变化会协作取消当前请求。
完整 identity 不匹配的结果按 stale 拒绝。

## 3. 线程与关闭安全

实现复用了已验证的 Qt 后台模式：

```text
QThreadPool + QRunnable；
shared_ptr<atomic_bool> 协作取消；
QPointer + QMutex 保护回调目标；
QueuedConnection 返回 UI 线程；
析构时解除 QObject 回调，不等待后台强制退出。
```

当前同步核心 benchmark 不支持内部毫秒级中断，因此取消在变换、适配和 benchmark 前后检查；
若取消发生在 benchmark 内部，返回结果只会被丢弃，不会写入 UI 或生产输出。

## 4. 生产边界

本任务不写生产 Package/TIFF，不修改生产 Profile、`ConfigDocument` 或 scene effective config，
也不把诊断成功显示为 production PASS。默认后端固定为已验证的 Legacy CPU diagnostic；
OpenVDB 不在本任务中静默替换。

## 5. 验证

实际通过：

```text
diagnostic_analysis_worker_unit_tests；
scene_transform_controller_unit_tests；
diagnostic_effective_config_unit_tests；
production_effective_config_unit_tests；
multimodel_scene_contract_unit_tests；
slicer_debug_ui Debug build；
diagnostic-settings-controls UI smoke。
```

`diagnostic_analysis_worker_unit_tests` 覆盖成功、失败、取消、重入 stale、销毁安全和真实闭合 cube
诊断执行链。真实执行证据只驻留内存，`productionAdmitted=false`。

## 6. 下一任务

`12E-09A-05` 已解除 09A-04 前置等待；13C-03 的 TIFF 原生数据源也已完成。下一步可在同一真实
`layerIndex/zMm` 上叠加 Texture Surface、Model Fill 和 Partition 诊断语义，不得回退到旧
preview PNG 序号。

