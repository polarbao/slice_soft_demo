# REPORT 14B-03 SceneFacade 当前状态

> 日期：2026-08-05  
> 范围：Stage 14B-03 正式 target 集成与合同验证  
> 结论：**SCENEFACADE_GATE=PASS；TARGET_INTEGRATION=PASS**

## 1. 任务边界

本任务实现 Qt-free 的 `SceneFacade` 权威场景服务，并接入 `slicer_base` 与 CTest。实现遵守：

- `slicer_three_lane_contract` v1.1；
- capability DTO v1.2 与 `SceneViewData` v1.2；
- Transient 期间零模块调用；
- Commit 批次全成或全败，成功后 revision 恰好加一；
- 正常 Commit 一次响应即可取得 canonical state、碰撞、越界、build volume、warning、preflight delta 与 ViewData identity，不追加 snapshot；
- `GetViewData` 不以轮廓或灰模伪造纹理成功。

真实双视图纹理数据仍由硬前置任务 14B-03A 提供，本任务只建立 Provider 边界。

## 2. 已实现能力

### 2.1 权威状态与 Commit

- 由 `SceneFacadeSeed` 注入 `MultiModelScene`、源模型、API identity 与已解析 build volume；
- 创建时复用 `ValidateMultiModelScene`，结构错误和未解析 build volume 均 fail-closed；
- `SceneOperationRequest` 独立承载并校验 `currentSceneRevision` 与 `expectedSceneRevision`；
- 在锁保护下对候选副本执行批次，任一未知/锁定实例、非法参数、stale 或取消均不修改权威状态；
- 成功后 revision 只增加一次，并返回完整 `SceneCommitResult`；
- 同 `operationId` 同规范化请求返回首次结果，不重复提交；不同 payload 返回 `PM-SLICER-PROFILE-0031`。

### 2.2 碰撞、越界与响应充分性

- 复用 `AdaptTransformedModel`、`EvaluateSceneCollisionAdmission` 与 `ComputeMultiModelSceneHash`；
- 碰撞和 XY 越界作为可提交的权威结果返回，结构/几何错误继续 fail-closed；
- Commit 响应包含 snapshot、canonical matrix、有效包围盒、build volume、碰撞、越界实例、warning、触及实例的结构化 preflight delta 与 `viewdataIdentity`；
- `zLimitMm` 继续沿用既有非阻断 warning 语义；
- `CheckCollision` 仅接受与当前 scene ID、revision、scene hash 一致的 snapshot。

### 2.3 ViewData Provider 边界

- 校验 scene、revision 以及 `three_d + outline_only` 非法组合；
- Provider 缺失时明确指出 14B-03A 前置未完成；
- Provider 返回后再次校验 revision 与 view mode；
- 未调用既有灰模 fallback，真实 top `surfacePreview` 与 three_d mesh/UV/submesh/material/texture 留给 14B-03A。

## 3. 正式验证

Debug 与 Release 均执行：

```powershell
cmake --build build-slicesoft/main --config <Debug|Release> --target scene_facade_14b03_unit_tests --parallel
ctest --test-dir build-slicesoft/main -C <Debug|Release> --output-on-failure -R "scene_facade_14b03_unit_tests|slicer_capability_dto_contract_test|slicer_three_lane_contract_test|slicer_stage14b_facade_dto_contract_test|slicer_stage14b_target_graph_test|slicer_stage14b_layering_feasibility_test"
```

实际结果：

```text
Debug build: PASS
Release build: PASS
Debug targeted CTest: 6/6 PASS
Release targeted CTest: 6/6 PASS
source-size guard self-test: PASS
```

专属测试覆盖 revision 单次递增、两 revision 字段不一致、成功重放、ID 冲突、stale、批次回滚、取消、权威碰撞/越界、完整 Commit 响应、旧 snapshot 拒绝、Provider 缺失及禁止 `three_d + outline_only`。

`ValidateSourceSizeGuard.py --base-ref HEAD` 当前仍被并行开发中的 14B-02 新文件超限阻断；14B-03 新增实现文件均小于 500 行，该跨任务阻断必须在 14B-02 提交前关闭。

## 4. 主要文件

- `src/slicer_core/api/CommonDtos.h`
- `src/slicer_core/api/SceneDtos.h`
- `src/slicer_core/api/SceneFacade.h`
- `src/slicer_core/api/scene/SceneFacadeService.*`
- `src/slicer_core/api/scene/SceneFacadeAuthority.*`
- `src/slicer_core/api/scene/SceneFacadeOperation.cpp`
- `tests/contracts/scene_facade_14b03/Main.cpp`
- `CMakeLists.txt`

## 5. 后续边界

1. 14B-03A 必须实现真实双视图纹理 Provider，才能解锁 14C-04、14E-04 与 14E-04c。
2. 14C wire adapter 只能映射本次完整 Commit 响应，正常成功不得追加 `scene.get_snapshot`。
3. Commit 级 `viewdataIdentity` 标识当前场景响应快照；网格、外观、纹理与预览缓存仍必须使用 14B-03A 的独立 identity，不能混用。
4. 本任务不修改 SPI v1、11 个导出、15 项能力或 RGBWSV 生产协议。

## 6. 阶段判断

14B-03 已完成实现、正式 target 接线及 Debug/Release 门禁。下一条关键路径是先完成 14B-02，再执行 14B-03A；在 14B-03A 通过前，不得宣称 ViewData v1.2 真实纹理链路完成。
