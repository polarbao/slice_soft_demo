# DOC_PREP_14D-08-R3-01A 全场景权威预检服务准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R3-01A`
>
> 文档状态：`PREPARATION_GATE=PASS`

## 1. 准备结论

`14D-08-R3-01A` 已具备开发条件。任务只建立 Qt-free、Worker-free 的全场景权威预检服务，
不创建公开 Facade、不注册 Worker executor、不生成生产 Package。`R3-01B` 在本服务通过
Debug/Release 定向门禁后，才负责 API DTO 投影和 Worker 适配。

## 2. 冻结输入

内部请求固定包含：

| 字段 | 规则 |
|---|---|
| `scene` | 已按 `slicesoft.multimodel_scene.13b.1` 解码的 committed scene |
| `sceneHash` | `ComputeMultiModelSceneHash(scene)` 的小写 SHA-256；必须精确匹配调用方身份 |
| `expectedSceneRevision` | 必须等于 `scene.sceneRevision` |
| `options` | 完整模型预检选项；不得静默降低 self-intersection 或 topology 检查 |
| `targetMode` | 当前生产请求明确选择的 `Legacy` 或 `GlobalSurfaceShell`；禁止由 Profile 名称猜测 |
| `admissionContext` | Legacy/Global backend 可用性，仅用于既有模式准入投影 |
| `modelResolver` | 以 `ModelSource` 为键加载不可变 `SceneModel`，不得扫描 resource scope 外路径 |
| `cancellationRequested` | 可在资源、模型、实例和完整几何循环边界协作取消 |

file contract 的 `geometry.preflight.full` 传输映射留给 `R3-01B`。映射固定为
`input.mode=full`、`input.scene` 和可选 `input.buildVolume`；若单独携带 build volume，其规范化值
必须与 scene 内值一致。`file_contract_v1` 的 `input` 已允许对象，本任务不需要提升 minor 版本。

## 3. 冻结输出

内部结果必须完整承载下列字段，并按 `sceneId -> modelId -> instanceId -> issueCode` 稳定排序：

```text
sceneId / sceneRevision / sceneHash
authoritative / productionAdmitted / cancelled / complete
checkedModelCount / checkedInstanceCount / blockedInstanceCount / skippedInstanceCount
instances[]:
  modelId / instanceId / transformRevision / transformHash
  sourceStatus / transformedStatus / legacyAdmission / globalAdmission
  topology / bboxMm / outOfBounds / issues[]
sceneIssues[] / collisions[] / outOfBoundsInstances[]
```

每条 issue 必须包含稳定 `code`、`severity`、`count`、`detail`、`modelId`、`instanceId` 和
结构化 context。公开 `api::PreflightResult` 当前仅含 issue code，不能满足能力 DTO；因此
`R3-01B` 必须对该内部 API DTO做加法扩展，但不得改变 C SPI、能力数量或导出符号。

## 4. 执行顺序

1. 校验 scene schema、scene hash、revision、生产 build volume 和 scene Profile 一致性。
2. 按 `modelId` 排序校验 source path、resource scope containment、source/resource hash，并加载一次模型。
3. 按 `instanceId` 排序，仅处理 `visible=true` 实例；hidden 实例计入 skipped，但不得影响 PASS。
4. 对每个 visible 实例调用 `TransformedModelPreflightService`，同时保留 source 与 effective transform 证据。
5. 构造 `SceneCollisionRequest`，调用 `EvaluateSceneCollisionAdmission` 检查 transformed bounds、越界和碰撞。
6. 聚合完整性、取消、预算和 admission；任一 required check 未完成都禁止 authoritative PASS。

同一 model 的 source audit 可缓存；不同 instance 的 transformed audit 不得共享 transform identity。
不得通过改写 Profile `input.modelPath` 循环调用单模型生产入口。

## 5. Admission 唯一关系

- 普通 scene-wide 预检使用 `EvaluateModelPreflightAdmissions` 作为每实例 Legacy/Global 模式投影。
- scene 最终 `productionAdmitted` 只读取请求中显式 `targetMode` 对应的模式结果；warning 可继续
  进入 Legacy，Global topology blocker 仍保持阻断。缺少或无法解析目标模式必须在 `R3-01B`
  映射层 fail-closed，禁止根据 `resolvedProfileId` 字符串猜测。
- `EvaluateProductionAdmission` 是 OpenVDB 实验诊断策略，不得被 scene service 再次作为通用准入器；
  它只允许在既有 OpenVDB 诊断链内部调用一次，其 blocker 作为该实例 issue 输入聚合。
- scene 最终 `productionAdmitted` 是“所有 visible 实例目标模式 admitted + collision PASS + complete”
  的合取，不建立第二套会覆盖既有 blocker 的策略。

## 6. 完整性与失败规则

| 情况 | authoritative | productionAdmitted | 结果 |
|---|:--:|:--:|---|
| 全部 required checks 完成且无 blocker | true | true | passed 或 warning |
| topology/collision/out-of-bounds blocker | true | false | blocked |
| 资源缺失、hash 不符、scope escape | false | false | fail-closed |
| 预算不足、诊断降级、部分跳过 | false | false | audit incomplete |
| stale revision/hash | false | false | stale |
| 取消/超时 | false | false | cancelled |

warning 不自动阻断 Legacy；Global 的 topology blocker 继续由既有 admission policy 决定。

## 7. 稳定映射

| 内部失败 | Facade/Worker 稳定码 |
|---|---|
| scene hash/revision stale | `PM-SLICER-LAYOUT-0022` |
| source/resource 不存在、不可读或越界 | `PM-SLICER-INPUT-0001` |
| import/format 无效 | `PM-SLICER-INPUT-0002` |
| topology blocked | `PM-SLICER-TOPOLOGY-0010/0011`，保留 issue code |
| 取消 | `PM-SLICER-CANCELLED-0070` |
| 未分类异常 | `PM-SLICER-INTERNAL-0099` |

domain blocker 应返回成功执行的 authoritative blocked 业务结果，而不是把所有 blocker 都转换为进程失败；
只有请求、资源、完整性或执行失败才返回 `ApiResult` failure。

## 8. 文件所有权与 target

```text
src/slicer_core/preflight/SceneFullPreflightService.h/.cpp
src/slicer_core/preflight/SceneFullPreflightResourceResolver.cpp
tests/stage14d_08_r3/SceneFullPreflightServiceTests.cpp
```

文件进入 `slicer_engine`，可依赖现有 import、preflight、collision 服务；禁止依赖 Qt、Worker runtime、
TIFF writer、UI 或 `print_module_spi.h`。`R3-01B` 另建 factory/executor 文件，避免本任务触碰 registry。

## 9. 定向验收

最低用例：单模型 PASS、多模型同源多实例、hidden 跳过、transform 后越界、实例碰撞、资源缺失、
scope escape、source hash 不符、stale scene、完整 topology blocker、budget incomplete、取消、双运行稳定排序。

```powershell
cmake --build build-slicesoft/main --config Debug --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "^stage14d08_r3_scene_preflight_tests$"
cmake --build build-slicesoft/main --config Release --target stage14d08_r3_scene_preflight_tests
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R "^stage14d08_r3_scene_preflight_tests$"
```

## 10. 门禁

```text
14D_08_R3_01A_PREPARATION_GATE=PASS
14D_08_R3_01A_IMPLEMENTATION=READY
14D_08_R3_01B=BLOCKED_BY_R3_01A
```
