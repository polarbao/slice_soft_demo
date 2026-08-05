# REPORT_14B-03A 双视图纹理 ViewData 当前状态

> 更新时间：2026-08-05
> 当前验证基线：`d7f36ce`
> 任务：Stage 14B-03A
> 状态：`IMPLEMENTATION_COMPLETE / DEBUG_RELEASE_GATES_PASS / HOST_INTEGRATION_PENDING`
> 打印侧 ACK：`NOT CONFIRMED`

## 1. 本次范围

本次在不修改 SPI v1、11 个 `pm_*` 导出、15 项能力、冻结 public DTO、
`cmake/SliceSoftCoreLayering.cmake`、现有 `api/implementation` 以及 model/importer/scene 共享实现的约束下，
完成 `ITexturedSceneViewDataProvider` 的正式真实纹理闭环：

- `top` 返回基于模型局部坐标 `+Z` 表面的真实 RGBA8 `surfacePreview`；
- `three_d` 返回 position、normal、UV、index、submesh、materials 与 textures；
- 已使用纹理缺失、解码失败、UV 无效或材质绑定无效时 fail-closed；
- 每个实例的 `appearanceIdentity` 必须闭合到 `appearances[]`；
- 建立 `localBoundsMm + worldMatrix` 的局部资源缓存与场景身份语义；
- 保留强制纹理资源，不使用灰模、轮廓或无纹理成功结果代替失败；
- 通过构造时注入只读模型仓库和纹理源，避免修改 `ModelRepository` 或 importer 共享文件；
- Provider 与单元/真实资产测试已接入根 CMake 和 CTest，并保持只链接 `slicer_base`。

本任务没有执行或取得打印侧回签，也没有接入 14C SPI blob 适配或 14E Qt 宿主显示。

## 2. 实现结构

### 2.1 Provider 与只读资源边界

- `TexturedSceneViewDataProvider` 实现现有 `ITexturedSceneViewDataProvider`，不新增 SPI 或能力。
- `ISceneViewModelRepository` 只按 `ModelId` 返回不可变 `SceneModel`。
- `ISceneViewTextureSource` 只负责将声明纹理解码为 straight-alpha RGBA8。
- 提供 map-backed 模型仓库和文件纹理源工厂；测试可注入内存纹理源，不依赖 Qt。

### 2.2 纹理与 appearance 闭合

- 只解析三角形实际引用的材质；未使用材质声明的缺失纹理不会导致误拒绝。
- 已使用材质声明纹理时，纹理文件缺失、解码失败、尺寸/像素无效均返回稳定错误。
- 已使用纹理三角形必须具有有限 UV，并且材质名必须解析到唯一有效材质。
- 相同解码纹理按内容身份去重；纹理身份不依赖源路径和实例世界变换。
- appearance 身份由实际使用的材质及纹理内容闭合计算，不以文件名伪造。

### 2.3 `top` 双视图纹理数据

- 使用局部 XY 投影和 `+Z` 深度选择生成真实表面预览。
- 纹理颜色通过三角形 UV 重心插值采样，保留纯白、纯黑及 alpha 数据。
- 背景为透明像素；成功结果必须至少包含一个可见表面像素。
- 预览身份由局部几何、局部边界、appearance 和 RGBA 内容决定，不包含 `worldMatrix`。
- 预算不足时依次尝试 768、512、384、256、128、64、32 像素级预算；无法保留必需纹理时返回
  `PM-SLICER-VIEWDATA-BUDGET`，不降级成灰模。

### 2.4 `three_d` 双视图纹理数据

- 返回位置、生成法线、UV、索引、按材质分组的 submesh、materials 和 textures。
- 使用 seam-safe 展开顶点，避免共享位置但 UV 不同的纹理接缝被错误合并。
- `three_d + outline_only` 被明确拒绝。
- `Auto` LOD 依次尝试 `Lod0`、`Lod1`、`Lod2`；LOD 只降低三角形负载，不删除必需纹理。
- local mesh identity 不包含实例世界变换；请求 world mesh 时使用不同身份。

## 3. Identity 与缓存语义

| 对象 | 身份输入 | 实例移动后的预期 |
|---|---|---|
| Texture | 解码 RGBA、宽高及纹理语义 | 不失效 |
| Appearance | 实际使用的 material/texture identity | 不失效 |
| Local surface preview | 局部几何、局部边界、appearance、RGBA | 不失效 |
| Local three_d mesh | 局部网格、UV、submesh、appearance、LOD | 不失效 |
| World three_d mesh | local mesh 与 `worldMatrix` | 失效并产生新身份 |
| Scene ViewData | scene revision、实例、`worldMatrix` 与资源引用 | 失效并产生新身份 |

该语义保证实例平移、旋转或缩放时可以复用局部 preview/texture/mesh 缓存，同时 Scene ViewData 仍能反映
最新权威场景变换。实现没有在 mouse-move 路径引入 DLL 调用，也没有改变 three-lane v1.1 的
Transient/Commit/Production 边界。

## 4. 新增实现与测试文件

实现位于 `src/slicer_core/api/viewdata/`：

- `SceneViewResources.h/.cpp`
- `TexturedSceneViewDataProvider.h/.cpp`
- `SceneViewResolvedAsset.h`
- `SceneViewAssetResolver.cpp`
- `SceneSurfacePreviewBuilder.h/.cpp`
- `SceneViewMeshBuilder.h/.cpp`
- `SceneViewIdentity.h/.cpp`
- `SceneViewBudget.h/.cpp`
- `SceneViewClosureValidator.h/.cpp`

独立测试位于 `tests/stage14b_03a/`：

- `Main.cpp`
- `PositiveCases.cpp`
- `FailureCases.cpp`
- `RealFixtureMain.cpp`
- `TestSupport.h`
- `RunIndependent.ps1`
- `ValidateRealFixtures.py`

所有本任务新增 C++ 源文件均不超过 500 行。仓库现有 `.gitignore` 的 `tests/*` 会忽略该新测试目录，
后续提交所有者必须显式纳入这些测试文件，不能只提交 Provider 实现。

## 5. 验证结果

### 5.1 已实际执行并通过

| 验证项 | 状态 | 准确边界 |
|---|---|---|
| 14B-03A 独立 C++20 编译与行为测试 | `PASS` | 使用独立脚本编译 Provider 和测试替身 |
| checker 3MF 语义测试 | `PASS` | 验证真实黑/红纹理、top 与 three_d 闭合 |
| 纯白/近白保真 | `PASS` | 专用合成纹理逐像素验证 opaque white 与 near-white 不被当作背景 |
| shengdanjie OBJ 使用材质闭合 | `PASS` | 真实 importer、文件纹理解码、top/three_d 纹理变化及未使用缺失纹理不被加载 |
| 双模型 appearances | `PASS` | 验证两个实例分别闭合到不同 appearance |
| fail-closed 负向测试 | `PASS` | 覆盖缺纹理、解码失败、无 UV、错误材质、预算、取消 |
| local/world identity 测试 | `PASS` | 验证实例移动不使局部资源身份失效 |
| 真实 fixture 端到端 | `PASS` | `load_model_report` 导入 checker 3MF 与 shengdanjie OBJ，随后生成 top/three_d ViewData |
| Capability DTO v1.2 合同门禁 | `PASS` | 15 项能力及双视图纹理/网格合同通过 |
| Three-lane v1.1 合同门禁 | `PASS` | Transient/Commit/Production 调用规则通过 |
| Debug/Release 正式工程 | `PASS` | 两个正式 CTest target 均完成编译和运行 |
| 文件纹理源真实运行 | `PASS` | 真实 OBJ/3MF 资产经文件纹理源解码并形成 ViewData |
| 新文件行数、尾随空白和禁止依赖扫描 | `PASS` | 新增源文件均不超过 500 行，未引入 Qt/OpenVDB 依赖 |

实际执行的主要命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\stage14b_03a\RunIndependent.ps1
python tests/contracts/ValidateCapabilityDtos.py
python tests/contracts/ValidateThreeLaneContract.py
cmake --build build-slicesoft/main --config Debug --target textured_scene_viewdata_14b03a_unit_tests textured_scene_viewdata_14b03a_real_fixture_tests --parallel
cmake --build build-slicesoft/main --config Release --target textured_scene_viewdata_14b03a_unit_tests textured_scene_viewdata_14b03a_real_fixture_tests slicer_base --parallel
ctest --test-dir build-slicesoft/main -C Debug --output-on-failure -R "(model_import_layering_probe|facade_contract_unit_tests|scene_facade_14b03_unit_tests|textured_scene_viewdata_14b03a_unit_tests|textured_scene_viewdata_14b03a_real_fixture_tests)"
ctest --test-dir build-slicesoft/main -C Release --output-on-failure -R "(model_import_layering_probe|facade_contract_unit_tests|scene_facade_14b03_unit_tests|textured_scene_viewdata_14b03a_unit_tests|textured_scene_viewdata_14b03a_real_fixture_tests)"
python tests/contracts/ValidateStage14BPreparation.py
python tests/contracts/ValidateStage14BLayeringFeasibility.py
python tests/contracts/ValidateStage14BFacadeDtos.py
python scripts/ValidateSourceSizeGuard.py --base-ref HEAD
```

独立脚本的实际输出为：

```text
Stage 14B-03A textured ViewData tests: PASS
Stage 14B-03A real fixture guard: PASS
```

合同门禁的实际输出为：

```text
15 capability DTOs plus dual-view texture/grid contract: PASS
Transient/Commit/Production interaction contract: PASS
```

正式 CTest 的 Debug 与 Release 结果均为 `5/5 PASS`；源码行数门禁为 `PASS`，只报告仓库既有
G4/G5 警告。

### 5.2 明确未运行或未确认

| 项目 | 状态 | 原因 |
|---|---|---|
| Qt/打印软件 top/three_d GPU 显示 | `NOT RUN` | 不属于 14B-03A 独立写入范围 |
| 真实模型 LOD、预算和性能基线 | `NOT RUN` | 当前只验证确定性预算行为和负向错误 |
| 14C SPI wire/blob 适配 | `NOT RUN` | 属于 14C-04，必须复用本 Provider，不得另造 DTO |
| 打印侧合同 ACK/回签 | `NOT CONFIRMED` | 本任务未收到、未执行且不声明打印侧 ACK |

## 6. 保持不变的边界

- 未修改 SPI v1、11 个导出函数、15 项能力和 capability DTO v1.2。
- 未修改 three-lane v1.1，不允许 mouse-move、orbit、pan、zoom 或视图切换调用模块。
- 未修改 CLI、Qt UI、生产 RGBWSV TIFF、manifest、RIP、位深、通道顺序或极性。
- 未修改 `ModelRepository`、model/importer/scene 共享实现，也未复制其业务规则。
- 根 CMake 只登记 Provider 源文件及两个正式测试 target；未修改分层映射或现有 `api/implementation`。
- 未提供纹理失败后的灰模成功路径，`three_d` 不允许 `outline_only` 伪装完成。

## 7. 未决集成问题

1. 14C-04 仍需把 capability DTO v1.2 的 wire/blob 适配接到本 Provider，禁止绕过或另造 ViewData DTO。
2. 现有 importer 没有在 `SceneModel` 中保留源顶点法线；当前 Provider 生成确定性几何法线，复杂浮雕的视觉质量仍需 14E 验收。
3. 当前 LOD 为确定性三角形降采样，不是质量保持的网格简化器；真实模型视觉质量和预算阈值仍需后续验证。
4. Qt 宿主显示和打印侧 ACK 属于后续 14E/外部协作，不得在本报告中标记为完成。
5. `tests/stage14b_03a/` 被仓库 `.gitignore` 的 `tests/*` 规则忽略，提交时必须显式纳入。

## 8. 结论

14B-03A 的切片侧实现已经完成：`top` 和 `three_d` 均具备真实纹理数据闭环，已使用纹理及其
UV/材质绑定采用 fail-closed，appearance 与 local/world identity 语义已通过独立测试和冻结合同门禁。

正式 Debug/Release target 与真实 importer 资产端到端均已通过。当前仍不得将 14B-03A 描述为
打印软件宿主集成完成，也不得宣称打印侧 ACK；这两项分别由 14C/14E 和外部回签关闭。
