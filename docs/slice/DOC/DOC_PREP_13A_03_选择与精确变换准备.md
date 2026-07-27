# DOC_PREP 13A-03 选择与精确变换准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13A-01、13A-02、13B-01、12E-09A-02 COMPLETE
> 后续：13A-04 WAIT 13A-03

## 1. 任务目标

在 13A-02 只读 +Z 俯视工作区上增加单模型精确实例变换，使 UI、`ModelInstance`、
`SceneViewGeometry` 和 session scene/effective config 使用同一份可审计变换。

本任务只处理：

```text
translateXmm；
translateYmm；
rotateZdeg；
uniformScale；
XY 居中；
重置；
locked、revision、stale、保存和回读。
```

不处理 mirror、Z 编辑、非均匀缩放、多模型列表、自动排版或 post-transform preflight。

## 2. 已有事实与缺口

已有：

```text
ModelTransform/ModelInstance/UpdateModelInstanceTransform；
TransformedModelAdapter 和稳定 transform hash；
MultiModelScene/sceneRevision/Scene Effective Config 原子事务；
SceneViewGeometry 和 generation-aware ModelTopViewLoader；
SceneDocument/SceneSelectionModel/ModelTopViewWidget；
single_model/scene Diagnostic Effective Config identity。
```

13A-02 的 `SceneDocument` 目前只保存一个投影快照，`ModelTopViewLoader` 在 Worker 完成后释放源
`SceneModel`。若直接在 UI 每次改值时重新读取 OBJ/3MF，会造成重复 I/O、资源解析和状态漂移。因此
13A-03 必须先建立只读源模型缓存和可编辑实例状态，不允许从当前屏幕坐标反推变换。

## 3. 冻结状态模型

建议新增或扩展：

```text
apps/slicer_debug_ui/models/SceneDocument.*
apps/slicer_debug_ui/services/SceneModelRepository.*
apps/slicer_debug_ui/controllers/SceneTransformController.*
apps/slicer_debug_ui/widgets/ModelTransformPanel.*
```

### 3.1 SceneDocument

单模型阶段至少持有：

```text
sceneId/sceneRevision；
modelId/instanceId；
ModelInstance；
只读 SceneViewGeometry；
source model cache key；
source/resource/transform hash；
dirty/loading/stale/error；
session scene config path。
```

Qt 文档不得复制并修改源三角网格。源 `SceneModel` 由 repository 以
`shared_ptr<const SceneModel>` 保存，key 至少包含：

```text
absolute model path；
source/resource hash 或文件身份；
source transform/config identity。
```

### 3.2 Revision

```text
有效 ModelTransform 改变 -> transformRevision +1；
实例集合或身份改变 -> sceneRevision +1；
等价输入不增加 revision；
UI 命令必须携带 expected transformRevision/sceneRevision；
Worker 完成时 revision 不一致 -> 丢弃结果，不覆盖新状态；
config 保存前再次比较 document 和 snapshot revision。
```

13A-03 只有一个实例，变换修改同时更新 transformRevision 和 sceneRevision。该规则与后续
13B 多实例保持一致。

## 4. 变换语义

继续使用已冻结次序：

```text
SourceTransform/autoOrient；
pivot = source bbox XY center + minZ；
uniformScale；
mirror（13A-03 固定 false）；
rotateZ；
translateX/Y；
不二次落台，不开放 translateZ。
```

数值规范：

| 字段 | UI 范围 | 小数/步长 | 校验 |
|---|---:|---:|---|
| X/Y mm | -10000.00..10000.00 | 2 / 0.10，可键入 0.01 | finite |
| rotateZ deg | -180.00..180.00 | 2 / 1.00 | 保存前归一化到 [-180,180) |
| uniformScale | 0.0100..100.0000 | 4 / 0.0100 | finite 且 >0 |

UI 范围是防御边界，不代表正式设备 buildVolume。13B-04 再根据正式幅面判定越界。

操作定义：

```text
XY 居中：把当前 effective bbox XY 中心移动到软件场景原点 (0,0)；
重置：恢复 identity InstanceTransform，不撤销 SourceTransform/autoOrient；
应用：使用当前字段形成一个原子命令；
编辑中：不得把半输入文本写入 SceneDocument；
locked：控件只读，命令返回稳定错误，不静默解锁。
```

打印幅面中心属于 13B-03；在 buildVolume 未解析时不得把“场景原点居中”写成“设备居中”。

## 5. 命令和异步重投影

建议命令：

```text
SetInstanceTransformCommand；
CenterInstanceAtSceneOriginCommand；
ResetInstanceTransformCommand。
```

控制器执行顺序：

```text
1. 读取 selected instance 和 expected revisions；
2. 校验 locked、身份和数值；
3. 调用 UpdateModelInstanceTransform；
4. 生成新的 sceneRevision/transformRevision；
5. 使用 repository 中的同一只读 SceneModel 异步重建 SceneViewGeometry；
6. 只有最新 generation/revision 可提交几何；
7. 更新 dirty 状态；
8. 用户保存时生成单实例 MultiModelScene/effective config；
9. 写后回读并核对 identity/revision/hash；
10. 失败时保留上一个完整快照，显示稳定错误。
```

拖动交互不属于本任务；QDoubleSpinBox 可在 `editingFinished` 或短防抖后提交，避免每个按键触发完整
投影。

## 6. Session 配置合同

不得覆盖源 Profile、`samples/configs` 或模型文件。首版单模型编辑保存为一个仅含单实例的
`MultiModelScene`：

```text
subjectType=scene；
sceneId/sceneRevision；
一个 ModelSource/ResourceScope；
一个 SceneModelInstance；
requestedTransform=用户实例变换；
derivedLayoutTransform=identity；
effectiveTransform=requestedTransform；
transformRevision；
source/resource/scene hash；
source Profile；
buildVolume=unresolved 或显式 fixture。
```

保存路径位于当前 UI session。保存、另存、回读、取消和 stale 继续复用 13B-01 原子事务，不新增
另一套 JSON 拼接逻辑。

切片按钮在 document dirty 或 effective config revision 落后时不得使用旧配置；13A-03 只建立
session 同步和 stale 提示，正式 post-transform production admission 在 13A-04 完成。

## 7. UI 设计

`ModelTransformPanel` 放在模型工作区侧栏或现有右侧“参数”区域，不挤压画布。使用：

```text
QDoubleSpinBox：X、Y、绕 Z、统一缩放；
图标按钮：居中、重置；
文本状态：scene/instance 简短身份、revision、已保存/未保存、locked；
工具提示：单位、范围、pivot、重置不影响自动姿态。
```

13A-03 不增加自由拖拽 gizmo，不用鼠标拖动画布修改模型。选中模型与控件绑定必须通过
`SceneSelectionModel`，禁止 MainWindow 手工复制选中状态。

## 8. 稳定错误

至少覆盖：

```text
SCENE_TRANSFORM_NO_SELECTION；
SCENE_TRANSFORM_INSTANCE_LOCKED；
MODEL_TRANSFORM_NON_FINITE；
MODEL_TRANSFORM_SCALE_NON_POSITIVE；
MODEL_TRANSFORM_REVISION_STALE；
SCENE_VIEW_REVISION_STALE；
SCENE_TRANSFORM_SOURCE_CACHE_MISSING；
SCENE_TRANSFORM_EFFECTIVE_CONFIG_STALE；
SCENE_TRANSFORM_SAVE_FAILED。
```

优先复用现有 `ModelTransformErrorCode`。只为 UI/controller 新增现有合同无法表达的错误，禁止把中文
提示当机器错误码。

## 9. 测试与验证

计划 target：

```text
scene_transform_controller_unit_tests
```

必测：

```text
X/Y、rotateZ、uniformScale；
中心到软件原点；
identity reset；
等价值不增加 revision；
locked 拒绝；
NaN/Inf/零或负 scale 拒绝；
stale scene/transform revision；
快速连续编辑仅最新投影生效；
源 SceneModel 不变；
保存/回读/取消/写失败回滚；
UI、SceneViewGeometry 和 effective config bbox/transform/hash 一致。
```

UI Smoke：

```text
--ui-smoke-test --case model-top-view-transform
```

覆盖三窗口尺寸、长中文路径、控件单位、选择隔离、locked、dirty/stale 和保存状态。

验证命令：

```powershell
cmake --build build --config Debug --target scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(model_transform_unit_tests|scene_view_geometry_unit_tests|multimodel_scene_contract_unit_tests|diagnostic_effective_config_unit_tests|scene_transform_controller_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\scripts\run_ci_quick.ps1
git diff --check
```

## 10. 准入结论

依赖和合同已完整，13A-03 可在用户授权后开发。停止条件：

```text
不得实现 mirror 或 post-transform preflight；
不得把 scene origin 称为设备中心；
不得在 UI 主线程重导入模型；
不得覆盖源 Profile/配置/模型；
不得让 stale revision 进入切片配置；
完成 REPORT_13A_03 后停止。
```
