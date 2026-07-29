# DOC PREP 12E-09A-04 异步分析 Worker 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-29
> 前置：12E-09A-01..03、13D-01..04 COMPLETE

## 1. 原子目标

本任务把当前实例的拓扑、全局距离、纹理/填充分区、纹理转移、栅格映射和闭环诊断放入后台
Worker。Qt 主线程只负责生成不可变请求、显示状态和接收结果，不执行几何重计算。

任务完成后应具备：

```text
开始诊断、取消诊断；
成功、失败、取消、重复运行和窗口关闭生命周期；
session/scene/instance/revision/configHash 完整身份；
切换模型、变换或参数后丢弃旧结果；
诊断结果只驻留内存，不写生产 package/TIFF；
宽度上限、全纹理阈值、耗时和分区统计可回显。
```

## 2. 固定边界

```text
不开放 Global 生产写包；
不修改生产 Profile、ConfigDocument、scene draft 或生产 effective config；
不复用 preview PNG；
不把诊断成功显示为 production PASS；
不在 09A-04 实现同层语义预览，该工作属于 09A-05；
OpenVDB 仍是候选后端能力，不是第三种产品模式。
```

## 3. 请求身份合同

每次启动生成独立诊断 session，并先写入：

```text
output/ui_sessions/<diagnostic-session>/
  slice_config.diagnostic.effective.json
```

Worker 请求必须冻结：

```text
sessionId；
sceneId / sceneRevision；
modelId / instanceId / transformRevision；
diagnostic configHash；
source model shared ownership；
instance transform；
texture sample options；
textureSurfaceWidthMm / modelFillMaterial；
classificationResolutionMm。
```

任一身份字段变化时，正在执行的请求进入逻辑取消；即使底层单个几何阶段稍后返回，也只能作为
stale 结果丢弃，不能更新当前 UI。

## 4. 生命周期

冻结状态：

```text
idle -> running -> succeeded
                -> failed
                -> cancelled
                -> stale
```

规则：

```text
重复启动：先取消旧 generation，再启动新 generation；
参数编辑：取消当前 generation，并把旧结果标记为不可复用；
模型/实例/scene revision/transform revision 变化：同上；
窗口关闭：只设置 cancellation flag 并解除 QObject 回调，不等待后台线程访问已销毁窗口；
完成回调：先核对 generation，再核对完整 identity；
同一 generation 只能产生一次终态回调。
```

当前核心 Release benchmark 是一个同步核心调用，09A-04 的取消为阶段边界协作取消：变换和
网格适配前后检查取消；进入一次 benchmark 后不能强制终止其内部函数，但返回后必须立即丢弃
已取消结果。后续若需要毫秒级中断，应为 partition/texture/raster 核心增加显式 cancellation
callback，不能通过终止线程实现。

## 5. 线程和所有权

采用现有 `TransformedModelPreflightLoader` 已验证的模式：

```text
QThreadPool + QRunnable；
std::shared_ptr<std::atomic_bool> cancellation；
QPointer + QMutex 保护回调目标；
QMetaObject::invokeMethod(..., Qt::QueuedConnection) 回到 UI 线程；
源模型使用 shared_ptr<const SceneModel>；
重型结果使用 shared_ptr，避免重复复制三维 mask。
```

Worker 不直接持有 `MainWindow`、`ContextInspector` 或生产输出目录。

## 6. 核心执行链

```text
AdaptTransformedModel
  -> AdaptSceneModelToTriangleMesh
  -> RunTextureFillPartitionReleaseBenchmark
     -> Legacy CPU topology/occupancy/distance/partition
     -> texture transfer
     -> raster mapping
     -> full closure diagnostic linkage
```

09A-04 先接通当前已验证的 Legacy CPU diagnostic backend。OpenVDB 可用性只展示，不在本任务
中静默替换后端。

## 7. UI 合同

“切片设置/纹理与填充诊断”增加：

```text
开始诊断；
取消诊断；
运行状态；
模型最大宽度 = allTextureThresholdMm；
全纹理阈值；
Texture Surface / Model Fill voxel 统计；
核心耗时；
失败或取消原因。
```

运行中禁用开始按钮，启用取消按钮；终态恢复参数编辑。诊断结果不改变顶部生产切片按钮状态。

## 8. 测试矩阵

### 单元测试

```text
成功：异步返回且只回调一次；
失败：异常转换为稳定失败，不传播到 Qt 事件循环；
取消：协作取消后不发布成功；
重入：第二次请求胜出，第一次结果被丢弃；
stale：scene/instance/revision/configHash 不匹配时拒绝结果；
销毁：Worker 销毁后后台完成不访问悬挂 QObject。
```

### UI Smoke

```text
中文开始/取消按钮和 tooltip 存在；
无模型时开始禁用；
运行状态、取消状态和结果边界可显示；
1280x720、1440x900、1920x1080 不遮挡。
```

### 回归

```text
diagnostic_effective_config_unit_tests；
production_effective_config_unit_tests；
multimodel_scene_contract_unit_tests；
slicer_debug_ui build；
--self-test；
git diff --check。
```

## 9. 完成判据

只有同时满足以下条件才能把 09A-04 标为 COMPLETE：

```text
后台 Worker 不在 UI 线程执行重任务；
诊断 effective config 写入、回读和 configHash 成功；
完整 identity 与 generation 双重防 stale；
成功/失败/取消/重入/销毁测试 PASS；
没有生产 package/TIFF 写入；
09A-05 被明确解锁。
```
