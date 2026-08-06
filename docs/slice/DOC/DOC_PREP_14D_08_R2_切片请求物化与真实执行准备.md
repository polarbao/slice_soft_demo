# DOC_PREP_14D-08-R2 切片请求物化与真实执行准备

> 编制日期：2026-08-06
>
> 对应任务：`14D-08-R2`
>
> 前置状态：`14D-08-R1=COMPLETE`
>
> 文档状态：`PREPARATION_GATE=PASS_WITH_CONTROLLED_SPLIT`
>
> 父任务状态：`14D-08=BLOCKED`

## 1. 准备结论

`slice.rgbwsv` 的文件合同到现有生产 `SliceFacade` 之间可以建立唯一、可审计的映射，但不能把
“请求物化”和“生产成功”合并成一个原子任务。R2 受控拆分为：

| 子任务 | 内容 | 准备状态 |
|---|---|---|
| `14D-08-R2-01` | scene/profile/output 校验、hash 复核和 job 目录原子物化 | **READY** |
| `14D-08-R2-02` | 通过唯一 `CreateProductionSliceFacade()` 执行真实切片 | **BLOCKED_BY_R3_01** |
| `14D-08-R2-03` | package、RIP strict、独立入口正负例集成证据 | **BLOCKED_BY_R2_02_AND_14D_05** |

R2-01 只生成可供现有生产 Facade 消费的受信输入，不创建 TIFF、manifest 或成功
`result.json`。生产执行必须等待 R3-01 的权威 full preflight，并继续受 14D-05 安全发布约束。

## 2. 已读取依据

- `contracts/file_contract_v1.request.schema.json`、`file_contract_v1.md`；
- `contracts/slicer_capability_dtos.json/.md` v1.2；
- `contracts/slicer_three_lane_contract.json/.md` v1.1；
- `src/slicer_core/scene/MultiModelScene.*`；
- `src/slicer_core/scene/SceneEffectiveConfig.*`；
- `src/slicer_core/config.*`；
- `src/slicer_core/engine/ProductionSliceFacadeFactory.*`；
- `src/slicer_core/pipeline/MultiModelProductionService.*`；
- `apps/slicer_worker/runtime/*`；
- `DOC_PREP_14D_05_安全发布与清理双保险实施准备.md`；
- `DOC_PREP_14D_08_R1_共享Worker执行基础拆分准备.md`。

## 3. 当前代码事实

1. R1 已严格解析并保留原始 `scene/profile/output` 对象，作业目录、请求、结果和取消路径不可变。
2. `ComputeMultiModelSceneHash()` 对 canonical scene JSON 生成小写 64 位 SHA-256，不带前缀。
3. 外部能力 DTO 的 `sceneHash` 使用 `sha256:<64 lowercase hex>`。
4. `SceneEffectiveConfig` 与 `SliceFacade` 内部继续使用不带前缀的 64 位摘要。
5. `CreateProductionSliceFacade()` 是唯一现成生产切片 Facade；不得在 Worker 复制 CLI 或切片器实现。
6. `MultiModelProductionService` 要求 production scene、可加载 Profile、Profile 名称一致、DPI/层高一致。
7. `PreflightFullFacade` 当前只有接口，没有可构造实现；因此 R2-02 尚不能诚实满足生产顺序。

## 4. R2-01 规范映射

### 4.1 scene 身份

1. `request.scene` 必须可由 `DeserializeMultiModelScene()` 解码。
2. scene 必须通过 `SceneValidationPurpose::Production`，fixture/unresolved build volume 不得冒充生产。
3. 规范摘要为：

```text
plainSceneHash    = ComputeMultiModelSceneHash(scene)
externalSceneHash = "sha256:" + plainSceneHash
```

4. `request.sceneHash` 必须与 `externalSceneHash` 逐字相等；不接受大写、短摘要或调用方自报值。
5. 写入内部 effective config 时使用 `plainSceneHash`，保持现有 Facade 合同不变。

### 4.2 Profile 身份

`request.profile` 必须同时满足：

- 是完整、可由当前 `load_slice_config()` 加载的 Profile；
- 含非空字符串 `profileVersion`；
- 含规范 `profileHash=sha256:<64 lowercase hex>`；
- `materialProcessProfile.enabled=true`；
- `materialProcessProfile.name` 与 scene 的 `resolvedProfileId` 一致；
- 输出 DPI、层高、RGBWSV 协议继续由 Profile 提供，Worker 不注入隐藏默认值。

Profile hash 计算规则冻结为：复制顶层对象、移除 `profileHash`，保留 `profileVersion` 和其余字段，
使用项目 `Json::dump(0)` 的稳定键序列化后计算 SHA-256，再添加 `sha256:` 前缀。未知可选字段参与
hash，防止调用方在 hash 后修改配置。

`input.modelPath` 仍是当前单模型配置加载器要求的兼容字段。R2 不以它替代 scene 中的模型列表，
也不得静默改写它；真实多模型来源只来自 committed scene。

### 4.3 路径与资源边界

以下路径必须为绝对、词法规范化路径：

- request 自身及 job 目录；
- scene 的 resource scope root/package path；
- 每个 `ModelSource.sourcePath`；
- `output.packageDir`。

模型与 package 资源必须存在并匹配预期文件/目录类型。scene 的 scope containment 继续由
`ValidateMultiModelScene()` 负责；R2 不使用 Worker 当前目录解析相对路径，不扫描 job 外未知目录，
不跟随请求中的路径删除既有资产。符号链接、目标租约和最终 publish 冲突由 14D-05 再做第二层防护。

### 4.4 job 目录物化

R2-01 只能在不可变 `jobDirectory` 内原子生成：

```text
scene.snapshot.json
profile.effective.json
scene_config.effective.json
```

每个文件先写同目录 `.tmp`，关闭后再替换最终文件。取消或失败时只清理本步骤拥有的临时文件和
尚未交给执行器的物化文件，不触碰 `request.json`、`result.json`、外部模型资源、既有 package、
14D-05 staging/backup。

`scene_config.effective.json` 必须通过现有 `WriteSceneEffectiveConfig()` 生成；DPI、层高、pipeline、
Profile id/path、scene snapshot path 和 packageDir 必须来自已校验输入。时间使用 UTC ISO 8601。

### 4.5 output 映射

- `output.contract` 必须精确为 `p0.rgbwsv.2`；
- `output.packageDir` 必须是绝对规范路径；
- R2-01 不创建 packageDir，不占用最终发布权；
- R2-02 的 `api::SliceRequest.scene_config_path` 指向本 job 的 effective config；
- R2-02 的内部 `scene_hash` 使用 `plainSceneHash`；
- R2-02 不接收第二个 output 路径，不允许 Facade 输出到请求外位置。

## 5. 稳定失败映射

| 条件 | 稳定错误码 | 说明 |
|---|---|---|
| scene 解码/生产校验失败、sceneHash 不一致 | `PM-SLICER-LAYOUT-0022` | stale/身份不闭合，不执行 |
| Profile 缺字段、hash 不一致、加载失败 | `PM-SLICER-PROFILE-0030` | 不使用默认 Profile 兜底 |
| scene/Profile 身份冲突 | `PM-SLICER-PROFILE-0031` | 不改写 scene 或 Profile |
| 相对、逃逸、不存在或类型错误的资源路径 | `PM-SLICER-INPUT-0001` | 现有稳定码定义为模型文件不存在或不可读；不误用内存不足码 |
| 物化文件写入/替换失败 | `PM-SLICER-OUTPUT-0050` | 不报告成功 |
| 物化前后观察到取消 | `PM-SLICER-CANCELLED-0070` | 清理本步骤拥有文件 |
| 未分类异常 | `PM-SLICER-INTERNAL-0099` | 在 executor 边界转换 |

## 6. R2-01 文件所有权

建议建立 engine-linked Worker 私有切片层，避免把 config/engine 依赖反向拉入 R1 runtime：

```text
apps/slicer_worker/slice/WorkerSliceRequestMaterializer.h
apps/slicer_worker/slice/WorkerSliceRequestMaterializer.cpp
tests/stage14d_08_r2/WorkerSliceRequestMaterializerTests.cpp
```

CMake 采用单独 `slicer_worker_slice_runtime` target，依赖方向为：

```text
slicer_worker_runtime -> slicer_base
slicer_worker_slice_runtime -> slicer_worker_runtime + slicer_engine
slicer_worker -> slicer_worker_slice_runtime
```

R2-01 不修改 production registry；只有 R2-02 和 R3-01 同时完成后才能注册真实 slice executor。

## 7. R2-01 验收

### 7.1 正例

1. 合法 committed production scene、完整 Profile、绝对 packageDir 物化成功。
2. scene/profile hash 双重复核通过，生成文件可重新读取且身份一致。
3. effective config 的 sceneHash、Profile、DPI、层高、pipeline 和 packageDir 与请求一致。
4. Debug/Release 结果一致；不创建 package、manifest、TIFF 或成功 result。

### 7.2 负例

1. sceneHash/profileHash 改动一位必须拒绝。
2. fixture build volume、Profile 名称冲突、relative/escape/missing resource 必须拒绝。
3. 非 `p0.rgbwsv.2`、相对 packageDir、写入冲突必须拒绝。
4. 取消发生在写入前/中/后均不得留下 `.tmp` 或伪成功文件集。
5. 缺 full preflight 时不得注册生产 slice executor。

### 7.3 实施后验证命令

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  stage14d08_r2_slice_materializer_tests
cmake --build build-slicesoft/main --config Release --target `
  stage14d08_r2_slice_materializer_tests

ctest --test-dir build-slicesoft/main -C Debug --output-on-failure `
  -R "^stage14d08_r2_slice_materializer_tests$"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure `
  -R "^stage14d08_r2_slice_materializer_tests$"

python tests/architecture/ValidateTargetDependencies.py --repo-root .
git diff --check
```

## 8. R2-02 与 R2-03 后置门

R2-02 必须满足：

1. R2-01 物化测试通过；
2. R3-01 提供真实、scene-wide、`authoritative=true` 的 full preflight；
3. 顺序固定为 materialize -> full preflight -> production SliceFacade；
4. SliceFacade 失败/取消不得生成成功 output；
5. 只复用 `CreateProductionSliceFacade()`。

R2-03 还必须等待 14D-05 的 staging、租约、自检、原子 publish、崩溃恢复和双清理。没有
14D-05 时产生的真实 package 只能作为受控开发证据，不能进入 M-MVP、模块发布或 Worker 替换结论。

## 9. 并行边界

- R2-01 可与 R3 准备审计、14D-07-R0 合同冻结并行，文件所有权不重叠。
- R2-02 必须串行等待 R3-01，不得用单模型预检或测试 fake 代替。
- R2-03 必须串行等待 R2-02 与 14D-05。
- 根 CMake 由单一集成者修改；与其他会话并发时必须先审计工作树。
- 当前工作树 Stage 16/渲染文档和模型资产不属于本任务，不得暂存或改写。

## 10. 门禁结论

```text
14D_08_R2_PREPARATION_GATE=PASS_WITH_CONTROLLED_SPLIT
14D_08_R2_01_PREPARATION_GATE=PASS
14D_08_R2_02_PREPARATION_GATE=BLOCKED_BY_14D_08_R3_01
14D_08_R2_03_PREPARATION_GATE=BLOCKED_BY_R2_02_AND_14D_05
14D_08_PARENT_GATE=BLOCKED
```

下一张可执行原子卡是 `14D-08-R2-01`。完成它只解除输入物化缺口，不得把
`slice.rgbwsv` 标记为 Worker 生产可用。
