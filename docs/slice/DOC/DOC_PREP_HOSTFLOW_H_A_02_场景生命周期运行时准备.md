# DOC_PREP HOSTFLOW H-A-02 场景生命周期运行时准备

> 状态：**IMPLEMENTED / VERIFIED**  
> 日期：2026-08-07  
> 任务：H-A-02 `SceneFacade` / `SceneCapabilityAdapter` 场景增删与隐式创建

## Goal

在不新增能力、不新增导出、不改变 SPI major 的前提下，实现：

```text
model.import → scene.apply_operation(addInstance/removeInstance)
```

并锁定原子批处理、实例 id 分配、`operationId` 幂等、revision 和负例 fail-closed。

## Current State

### A 级代码事实

| 模块 | 当前事实 | H-A-02 缺口 |
|---|---|---|
| `SceneCapabilityAdapter` | 已支持既有 handle、完整 inline scene 和受控隐式 scene 三条路径 | H-A-04 排版操作仍待独立实现 |
| `SceneFacadeService` | 已支持 add/remove、revision、原子候选和 operationId replay | 无 H-A-02 遗留缺口 |
| `ModelCapabilityAdapter` | import 后的 `SceneModel` 可经模块内注册表受控注入场景 | 无 H-A-02 遗留缺口 |
| `MultiModelScene` | instance 继续受 model/resource scope 与 Profile 一致性约束 | 隐式场景上下文由 DTO v1.6 `sceneContext` 提供 |
| Worker slice | scene Profile 继续与提交 Profile 一致 | 未引入模块猜测或硬编码默认值 |

### B 级目标

- add/remove 按请求顺序在候选 authority 上执行，任一失败不提交。
- `assignInstanceId` 缺省时模块生成唯一且稳定的实例 id。
- remove 只删除 instance，不释放 import model。
- replay 相同 operationId + 相同 payload 返回原结果；不同 payload fail-closed。
- 隐式 scene 必须由宿主提供 Profile/buildVolume 上下文，不能写死内部默认值。

## Preparation Gate

| 准备项 | 状态 | 说明 |
|---|---|---|
| H-A-01 add/remove DTO | PASS | v1.5 合同基础已完成 |
| Core/Adapter 代码落点 | PASS | 文件所有权和现有 replay/atomic 机制已核实 |
| 错误码集合 | PASS | 复用 INPUT-0001/0002、PROFILE-0030/0031、LAYOUT-0022 |
| Debug/Release 测试入口 | PASS | 扩展 `scene_facade_14b03_unit_tests` 并新增 Adapter/DLL 生命周期测试 |
| 隐式 scene Profile 输入 | PASS | HQ-07 已授权 DTO v1.6 `sceneContext.resolvedProfileId` |
| 隐式 scene buildVolume 输入 | PASS | HQ-07 已授权宿主权威 `sceneContext.buildVolume` |
| H-A-04 排版合同 | DEFERRED | 独立卡，不属于 H-A-02 |

**结论：HQ-07 已由用户授权，DTO v1.6、运行时和 Debug/Release 门禁均已完成。H-A-02
于 2026-08-07 收口；下一张独立原子卡为 H-A-04。**

## Proposed Implementation

### 1. Core DTO

- `SceneOperationType` 增加 `AddInstance`、`RemoveInstance`。
- `SceneOperation` 增加 add 所需的模型注册描述、可选指定实例 id 和 initial transform。
- 模型资源使用 `shared_ptr<const SceneModel>`，只在候选 authority 提交成功后进入 session。
- fingerprint 纳入 operation type、model identity、assigned id、initial transform 和 sceneContext identity。

### 2. SceneFacade

- 对 candidate scene 先执行 add/remove，再统一重算 effective transform、bbox、碰撞和越界。
- add 首次引用模型时一并注册 `ResourceScope`、`ModelSource`、`models_by_id`、`api_model_ids`。
- 同一场景重复 `assignInstanceId`、未知 modelId、删除未知 instance 均 fail-closed。
- 同批 `add → transform`、`add → remove` 按顺序生效；失败不增加 revision。

### 3. Adapter

- 区分三条输入路径：已有 handle、非空 inline scene、隐式 scene。
- 隐式路径先完整校验 sceneContext 和 revision，再分配 scene/session id。
- `modelId` 只从同一 `pm_module_t` 对应的 `ModelCapabilityAdapter` 查询。
- 仅成功隐式创建响应追加 `sceneHandle`；失败不得遗留可查询 session。

### 4. Tests

正向：

```text
empty + add(identity)
empty + add(initialTransform)
existing + add
add + transform in one batch
add + remove in one batch
remove existing
operationId exact replay
model remains available after remove
```

负向：

```text
unknown/released modelId
duplicate assignInstanceId
remove unknown instanceId
add contains forbidden instanceId
implicit create without/invalid sceneContext
existing handle plus sceneContext
non-empty inline scene plus sceneContext
stale revision
same operationId with changed model/transform/context
cancelled batch leaves no scene/session mutation
```

## File Ownership

预计修改：

```text
src/slicer_core/api/SceneDtos.h
src/slicer_core/api/scene/SceneFacadeOperation.cpp
src/slicer_core/api/scene/SceneFacadeService.*
src/slicer_module/SceneCapabilityAdapter.cpp
src/slicer_module/SceneLifecycleSupport.*（新）
tests/contracts/scene_facade_14b03/Main.cpp
tests/hostflow/SceneLifecycleAdapterTests.cpp（新）
CMakeLists.txt
```

不修改：

```text
contracts/print_module_spi.h
apps/slicer_debug_ui/**
p0.rgbwsv.2 / TIFF writer / RIP reader
H-A-04 GridLayoutPolicy 接线
```

## Validation

实际执行：

```powershell
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
cmake --build build-slicesoft/main --config Debug --target scene_facade_14b03_unit_tests hostflow_ha02_scene_lifecycle_tests
ctest --test-dir build-slicesoft/main -C Debug -R "scene_facade_14b03|hostflow_ha02" --output-on-failure
cmake --build build-slicesoft/main --config Release --target scene_facade_14b03_unit_tests hostflow_ha02_scene_lifecycle_tests
ctest --test-dir build-slicesoft/main -C Release -R "scene_facade_14b03|hostflow_ha02" --output-on-failure
git diff --check
```

结果：

- DTO v1.6 与三车道合同门禁：PASS。
- Debug `scene_facade_14b03` / `hostflow_ha02`：2/2 PASS。
- Release `scene_facade_14b03` / `hostflow_ha02`：2/2 PASS。
- Source-size guard：PASS（仅既存 warning）。
- `git diff --check`：PASS（仅 Windows 行尾提示）。

## Rollback

- 合同未获授权：保留 v1.5 H-A-01，H-A-02 不落地，不影响既有四种变换。
- 实现门禁失败：回退 H-A-02 运行时代码，保留已获授权的合同修订与明确状态，不修改 ABI 外壳。
