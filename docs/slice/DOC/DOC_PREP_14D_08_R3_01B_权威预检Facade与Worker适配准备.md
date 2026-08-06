# DOC_PREP_14D-08-R3-01B 权威预检 Facade 与 Worker 适配准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R3-01B`
>
> 文档状态：`PREPARATION_GATE=PASS / IMPLEMENTATION=COMPLETE`

## 1. 审计结论

`14D-08-R3-01A` 的 engine 内部全场景权威预检服务已经完成。`14A-04-R2` 已获用户授权并将
能力 DTO 升至 v1.3，跨进程请求现在具备重建 committed scene 几何所需的完整身份；本任务的
准备门禁已经通过，可以实现 Facade 与 Worker executor。

实现仍不得退回到只依据 `input.scene` 和默认 `SliceConfig` 重新导入模型；这会对另一份几何执行
“权威预检”，属于静默错误，必须 fail-closed。

## 2. 代码事实

1. `MultiModelScene::ModelSource` 仅包含模型路径、格式、source/resource hash 和显示名，不包含
   `modelTransform`、`autoOrient` 或其他模型加载配置。
2. `ComputeSceneResourceHash` 覆盖路径、格式、材质和纹理资源身份，但不覆盖加载后的顶点坐标或
   模型加载配置，不能证明 Worker 重建出的几何与提交场景相同。
3. `MultiModelProductionService::LoadSceneModels` 以物化后的完整 Profile 为基线，仅替换每个模型的
   `input.modelPath` 和格式后调用生产 importer；这才是当前生产几何重建规则。
4. `SceneFullPreflightService` 要求调用方提供 `modelResolver`，且要求显式 `targetMode`；当前
   `geometry.preflight` DTO 没有 Profile 身份和 `targetMode` 字段。
5. 当前 `api::PreflightRequest/Result` 仍是窄占位 DTO，不能承载 scene/profile/hash、完整性、实例证据
   和稳定 structured issue。

## 3. 必须补齐的传输身份

受控合同修订后，`geometry.preflight.full` 的 Worker `input` 至少应冻结为：

```text
mode                  = full
scene                 = committed canonical scene
sceneHash             = sha256(canonical scene)
expectedSceneRevision = scene.sceneRevision
profile               = canonical effective Profile
profileHash            = sha256(canonical effective Profile)
targetMode             = legacy | global_surface_shell
buildVolume            = optional exact-match copy
```

规则：

1. `profile.resolvedProfileId` 必须与 `scene.resolvedProfileId` 一致。
2. `profileHash`、`sceneHash` 和 revision 必须在执行前、模型加载后各校验一次。
3. 单独携带 `buildVolume` 时必须与 scene 内规范化值完全一致，不允许覆盖 scene。
4. `targetMode` 必须显式传输，禁止根据 Profile 名称、路径或 capability 猜测。
5. Worker 只允许从本 job 的物化文件读取 scene/Profile；不扫描仓库配置目录，也不回读 UI 状态。

## 4. Facade DTO 与 Worker 映射

### 4.1 `api::PreflightRequest` 加法字段

```text
sceneConfigPath
profileConfigPath
sceneHash
profileHash
expectedSceneRevision
targetMode
authoritative=true
```

### 4.2 `api::PreflightResult` 加法字段

结果必须保留 `R3-01A` 的 scene/revision/hash、complete/cancelled、模型与实例计数、每实例证据、
碰撞、越界和 structured issues。`issue_codes` 可作为兼容摘要保留，但不得成为唯一证据。

### 4.3 Worker 固定顺序

```text
parse envelope
  -> validate and canonicalize input identities
  -> materialize scene/profile into job directory
  -> build production-equivalent model resolver from effective Profile
  -> invoke ProductionPreflightFullFacade
  -> map domain admission result
  -> atomically publish result.json
```

几何 blocker 是成功执行后的业务结果；资源不完整、身份 stale、取消、路径越界或 importer 失败是
Worker failure。预检不得生成 TIFF、manifest 或 Package。

## 5. 受控合同修订要求

该补充已通过 `DOC_DECISION_14A-04-R2` 受控修订进入能力 DTO v1.3，并须持续通过合同门禁。
修订保持：

```text
PM_SPI_VERSION = 1
pm_* exports   = 11
capabilities   = 15
file contract major = 1
```

修订只允许为既有 `geometry.preflight` full 模式增加条件必需字段，不新增能力或导出。

## 6. 验收矩阵

最低覆盖：

- 默认与非默认 `modelTransform`；
- `autoOrient` 开启、关闭和不同最大高度；
- scene/Profile id 不一致、hash 不一致、revision stale；
- Legacy 与 Global 显式 target mode；
- 同一模型多 instance、hidden instance、碰撞和越界；
- Worker 与直接 Facade 结果逐字段一致；
- domain blocked 返回业务结果，resource incomplete 返回稳定失败；
- 不生成生产 Package。

## 7. 文件所有权

```text
contracts/slicer_capability_dtos.json/.md       仅 14A-04-R2 修改
src/slicer_core/api/SliceDtos.h                 01B 加法扩展
src/slicer_core/engine/ProductionPreflightFullFacadeFactory.h/.cpp
apps/slicer_worker/preflight/WorkerPreflightExecutor.h/.cpp
tests/stage14d_08_r3/WorkerPreflightExecutorTests.cpp
```

共享合同、根 CMake 和 Worker production registry 必须串行集成；Facade 实现和测试 fixture 可在合同
确认后并行开发。

## 8. 实施结果

`14D-08-R3-01B` 已完成以下闭环：

1. `api::PreflightRequest/Result` 已扩展为 scene/Profile/hash/revision/targetMode 与完整实例证据 DTO；
2. `ProductionPreflightFullFacade` 使用物化后的完整 Profile 重建模型，并复用生产 importer 规则；
3. Worker executor 严格校验输入身份、物化 job-owned scene/Profile，并把 blocker 保留为业务结果；
4. 资源缺失、hash/revision stale、Profile 不一致与取消均返回稳定失败；
5. Debug/Release 定向测试验证直接 Facade 与 Worker 的 scene 身份、实例计数和 admission 一致；
6. 预检未生成 TIFF、manifest 或生产 Package。

## 9. 门禁

```text
14D_08_R3_01A_IMPLEMENTATION=COMPLETE
14A_04_R2_DECISION=ACCEPTED
14D_08_R3_01B_PREPARATION_GATE=PASS
14D_08_R3_01B_IMPLEMENTATION=COMPLETE
NEXT_TASK=14D_08_R2_02
```
