# DOC_PREP_13 全阶段原子任务实施准备与文件所有权

> 文档状态：P0 ATOMIC PREPARATION COMPLETE / 13A-01..05、13B-01..07、13C-01、09A-02 COMPLETE / NEXT 13C-02
> 版本：v1.6
> 日期：2026-07-28
> 适用范围：13A-01..05、13B-01..07、13C-01..05

## 1. 目的

本文把 Stage 13 已批准的 PRD/DEV/DEMO 转换为 17 个近程原子任务的实施准备矩阵，冻结：

```text
任务输入和前置依赖；
建议模块及文件所有权；
计划测试 target；
任务内输出；
验收和停止条件；
外部产品/设备 Gate。
```

本文只表示“开发前准备完整”，不表示任何建议文件、测试 target 或功能已经存在。每个任务开始前仍须
读取当前代码，按实际目录和依赖做最小调整，不得仅根据本文文件名机械创建代码。

## 2. 固定架构和协议边界

```text
Qt 仅允许位于 apps/slicer_debug_ui；
scene/layout/pipeline 核心对象使用 STL/domain DTO；
现有 SourceTransform=modelTransform+autoOrient 保持不变；
InstanceTransform 在 SourceTransform 后应用，不新增 Z 平移或二次自动落台；
Legacy 保持默认，Global 仅显式 opt-in；
联合切片失败不得静默拆分为多个单模型成功；
生产 package 保持 p0.rgbwsv.2、R G B W S V、uint8、black_is_print；
生产预览以 manifest 列出的 TIFF 为权威；
诊断语义只能来自 report/mask，不能从 TIFF 猜测；
OpenVDB optional/OFF，不因 Stage 13 变成强制依赖。
```

## 3. 目录所有权

| 领域 | 建议目录 | 允许职责 | 禁止职责 |
|---|---|---|---|
| Scene DTO | `src/slicer_core/scene` | model/instance/transform/scene identity | Qt、文件对话框、直接写 TIFF |
| Geometry adapter | `src/slicer_core/geometry` | 变换后 mesh/bbox/view geometry | UI 状态、Profile 持久化 |
| Layout | `src/slicer_core/layout` | grid、build volume、collision | package 写入 |
| Pipeline | `src/slicer_core/pipeline` | 多实例编排、层映射和 fail-closed | Qt |
| Output/report | `src/slicer_core/output`、`src/slicer_core/reports` | 单 package 和旁路审计 | 业务策略决策 |
| Qt scene state | `apps/slicer_debug_ui/models`、`services` | session document、selection、worker adapter | core 内部临时对象 |
| Qt widgets/controllers | `apps/slicer_debug_ui/widgets`、`controllers` | 显示和用户命令 | 生产 TIFF 业务规则 |
| Tests | `tests/unit`、UI self-test/smoke | 数学、schema、负向、UI 和协议回归 | 以截图替代协议验证 |

所有新增 C++ 文件使用 PascalCase；Public API 使用 Doxygen；C++ 使用 Allman；Qt 自定义信号以
`Sig` 开头，槽以 `On` 开头，并使用函数指针 `connect`。

## 4. 13A 原子任务准备

### 4.1 13A-01 ModelTransform 与 ModelInstance

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13-00 文档准入 |
| 核心文件 | 新增 `scene/ModelTransform.*`、`scene/ModelInstance.*`；新增或扩展 `geometry/TransformedModelAdapter.*` |
| 现有接触面 | `scene/SceneModel.*`、`geometry/SceneModelTriangleMeshAdapter.*`、core CMake |
| 计划测试 | `model_transform_unit_tests` |
| 必测 | identity、pivot、scale、mirror、rotateZ、translateXY、winding/normal/UV、revision、SourceTransform minZ 不变 |
| 非目标 | Qt 画布、生产路由、联合切片 |
| 完成输出 | Public DTO、数学/adapter、单测、13A-01 状态报告 |

详细合同以 `DOC_PREP_13A_01_ModelTransform与ModelInstance合同准备.md` 为准。

### 4.2 13A-02 俯视渲染

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13A-01 |
| 核心文件 | `scene/SceneViewGeometry.*` 或等价只读 DTO |
| UI 文件 | 新增 `models/SceneDocument.*`、`models/SceneSelectionModel.*`、`widgets/ModelTopViewWidget.*` |
| 接入点 | `MainWindow.*`、`apps/slicer_debug_ui/CMakeLists.txt` |
| 计划测试 | `scene_view_geometry_unit_tests`、`--ui-smoke-test --case model-top-view` |
| 必测 | +Z 俯视、+X 右/+Y 上、毫米比例、bbox/轮廓、适应视图、选中隔离、blocked 只读显示 |
| 非目标 | 3D 相机、gizmo、自动 nesting |
| 完成输出 | 单模型俯视可见、选择状态稳定、无 Qt 反向依赖 |

详细合同以 `DOC_PREP_13A_02_模型俯视渲染准备.md` 为准。

### 4.3 13A-03 选择与精确变换

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13A-02、13B-01；建议先完成 scene-aware 09A-02 |
| UI 文件 | 新增 `widgets/ModelTransformPanel.*`、`controllers/SceneTransformController.*` |
| 状态文件 | 扩展 `SceneDocument` 和 Scene Effective Config adapter |
| 计划测试 | `scene_transform_controller_unit_tests`、`--ui-smoke-test --case model-top-view-transform` |
| 必测 | X/Y、rotateZ、uniformScale、重置、保存/回读、locked、stale revision、非法数值 fail-closed |
| 非目标 | Z 编辑、非均匀缩放、完整 undo/redo |
| 完成输出 | UI、session config 和几何 adapter 使用同一 effective transform |

详细合同以 `DOC_PREP_13A_03_选择与精确变换准备.md` 为准。

### 4.4 13A-04 镜像与 post-transform preflight

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13A-03 |
| 核心接入 | `TransformedModelAdapter`、`pipeline/ModelPreflightGate.*` 或等价 public adapter |
| UI 接入 | `ModelTransformPanel` 镜像控制和阻断状态 |
| 计划测试 | `transformed_model_preflight_unit_tests`、真实纹理模型回归 |
| 必测 | mirrorX/mirrorY/双镜像、winding、normal、UV、source/transformed admission、confirmed self-intersection 阻断 |
| 非目标 | 自动修复、绕过 strict admission |
| 完成输出 | 每次变换后的几何准入可追踪，blocked 状态不允许生产 |

详细合同以 `DOC_PREP_13A_04_镜像与变换后预检准备.md` 为准。

### 4.5 13A-05 阶段收口

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13A-04 COMPLETE |
| 主要修改 | UI self-test/smoke、用户手册、Stage 13A report、索引和上下文 |
| 计划验证 | Debug build、CTest、self-test、三窗口尺寸 smoke、Quick CI |
| 必测资产 | xiao_ma、yecan、Texture2D 3MF 正向；复杂浮雕 blocked 反向 |
| 完成输出 | M13-1 单模型俯视与变换候选 |

## 5. 13B 原子任务准备

### 5.1 13B-01 MultiModelScene 与 Scene Effective Config

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13A-01 Public DTO |
| 核心文件 | 新增 `scene/MultiModelScene.*`、`scene/ModelSource.*`、`scene/ResourceScope.*` |
| 配置接入 | `config/ConfigSchema.*`、`ConfigMigration.*`、`NormalizedConfig.*` 或新 Scene config reader |
| UI 接入 | 扩展 `EffectiveConfigGenerator.*`，但本任务不做模型列表 |
| 计划测试 | `multi_model_scene_config_unit_tests`、`scene_effective_config_unit_tests` |
| 必测 | single_model/scene、modelId/instanceId、resourceScope、revision、unknown buildVolume、scene_profile_only、save/readback/revert/stale |
| 完成输出 | scene identity 和原子 Effective Config；解锁 09A-02 |

详细合同以 `DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备.md` 为准。

### 5.2 13B-02 模型列表与实例操作

状态：`COMPLETE（2026-07-27）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-01、13A-05 COMPLETE |
| UI 文件 | 新增 `widgets/ModelListPanel.*`，扩展 `SceneDocument`、`SceneSelectionModel` |
| 计划测试 | `scene_document_unit_tests`、`--ui-smoke-test --case multi-model-list` |
| 必测 | 导入、复制、删除、显示、锁定、选择同步、稳定列表顺序、资源作用域隔离 |
| 非目标 | 自动排版和切片 |
| 完成输出 | 1..22 实例的可编辑场景草稿 |

详细合同以 `DOC_PREP_13B_02_模型列表与实例操作准备.md` 和
`CODEX_PROMPT_13B_02_模型列表与实例操作执行指令.md` 为准。

### 5.3 13B-03 11x2 规则排版

状态：`COMPLETE`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-02 COMPLETE |
| 核心文件 | 新增 `layout/BuildVolume.*`、`layout/GridLayoutPolicy.*` |
| UI 文件 | 新增 `widgets/SceneLayoutPanel.*` |
| 计划测试 | `grid_layout_policy_unit_tests`、layout UI smoke |
| 必测 | 1/11/12/22 实例、row_major、边到边 20/30 mm、不同尺寸 bbox、确定性、手工 override、超过 22 拒绝 |
| 外部 Gate | buildVolume 未知允许 fixture/draft，不允许 production ready |
| 完成输出 | 可序列化 requested/derived/effective layout |

详细合同以 `DOC_PREP_13B_03_11x2规则排版准备.md` 和
`CODEX_PROMPT_13B_03_11x2规则排版执行指令.md` 为准。

实际证据：`REPORT_13B_03_11x2规则排版当前状态.md`。

### 5.4 13B-04 幅面、碰撞和逐实例准入

状态：`COMPLETE（FUNCTIONAL FIXTURE）/ PRODUCTION GATE OPEN`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-03 |
| 核心文件 | 新增 `layout/SceneCollisionService.*`；复用 `ProductionAdmissionPolicy` 和 model preflight |
| 计划测试 | `scene_collision_admission_unit_tests` |
| 必测 | AABB 快筛、精确投影/mask、边界 epsilon、重叠、越界、buildVolume undefined、逐实例错误身份 |
| 外部 Gate | 正式设备 width/height/origin/axes 阻断 production acceptance |
| 完成输出 | fixture 级准入能力；外部输入关闭后才能完成 production Gate |

详细合同以 `DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md` 和
`CODEX_PROMPT_13B_04_幅面碰撞与逐实例准入执行指令.md` 为准。

实际证据：`REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md`。

### 5.5 13B-05 全局 Raster 与联合层合成

状态：`READY FOR FIXTURE DEVELOPMENT`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-04 功能 Gate |
| 核心文件 | 新增 `pipeline/MultiModelSliceOrchestrator.*`、`pipeline/SceneLayerComposer.*` |
| 复用边界 | 现有 Legacy/Global layer producer、RasterGrid、MaterialChannelComposer |
| 计划测试 | `multi_model_layer_composer_unit_tests` |
| 必测 | 全场景 bbox、共享 Z 层序、实例局部到全局映射、无跨实例串写、重叠 fail-closed、Model/OuterVarnish/Support/Empty 优先级 |
| 非目标 | 永久布尔合并、跨模型联合支撑、混合引擎 |
| 完成输出 | 每个 layerIndex 一个 writer-ready 全局 RGBWSV buffer |

详细合同以 `DOC_PREP_13B_05_全局Raster与联合层合成准备.md` 和
`CODEX_PROMPT_13B_05_全局Raster与联合层合成执行指令.md` 为准。

### 5.6 13B-06 单 package 与 scene report

状态：`FIXTURE COMPLETE / PRODUCTION INPUT OPEN`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-05 |
| 核心文件 | 新增 `reports/MultiModelSceneReport.*`；扩展共享 package writer 的可选 scene metadata |
| 计划测试 | `multi_model_package_writer_unit_tests`、`rip_reader_test --summary` |
| 必测 | 单 package、单层单 TIFF、manifest layer list、modelId/instanceId、原子发布、失败无伪成功 package、RIP strict |
| 协议边界 | 不修改 `p0.rgbwsv.2` 固定字段和六通道语义 |
| 完成输出 | 可审计的联合 package 候选 |

详细合同以 `DOC_PREP_13B_06_单Package与SceneReport准备.md` 和
`CODEX_PROMPT_13B_06_单Package与SceneReport执行指令.md` 为准。

### 5.7 13B-07 真实模型矩阵与收口

状态：`READY FOR FUNCTIONAL MATRIX DEVELOPMENT / PRODUCTION GO INPUTS OPEN`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13B-06 |
| 脚本/报告 | 新增 Stage 13B matrix script、结果 JSON/Markdown、`REPORT_13B` |
| 计划矩阵 | 1/11/12/22 实例、OBJ/3MF、纹理资源隔离、碰撞、越界、两引擎允许范围、RIP strict |
| 记录指标 | import/layout/preflight/slice/compose/TIFF/report、总时长、峰值内存、package 大小 |
| 外部 Gate | 设备幅面/轴方向和 22 实例正式预算未关闭时，只能输出工程实测，不能宣称 production GO |
| 完成输出 | M13-2/M13-3 的 GO/NO-GO 证据 |

详细合同以 `DOC_PREP_13B_07_真实模型矩阵与阶段收口准备.md` 和
`CODEX_PROMPT_13B_07_真实模型矩阵与阶段收口执行指令.md` 为准。

## 6. 13C 原子任务准备

### 6.1 13C-01 TiffLayerSource 与 LRU

状态：`COMPLETE（2026-07-28）`

| 项目 | 准备内容 |
|---|---|
| 前置 | 无代码依赖；单贡献者顺序要求 identity wave 后实施 |
| 核心文件 | 新增无 Qt 的 RGBWSV layer reader/source DTO，复用 `read_rgbwsv_tiff` |
| UI 文件 | 新增 `services/TiffLayerSource.*`、`services/TiffLayerCache.*` 或等价 adapter |
| 计划测试 | `tiff_layer_source_unit_tests`、`tiff_layer_cache_unit_tests` |
| 必测 | manifest-only layer、stripped/tiled、5 层/256 MiB LRU、cache identity、取消、stale generation、切包失效、稳定错误码 |
| 非目标 | 伪彩、删除旧 Panel、修改 TIFF |
| 完成输出 | 可复用的异步生产层数据源 |

详细合同以 `DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md` 为准。

### 6.2 13C-02 MaterialPreviewComposer

状态：`READY / 13C-01 COMPLETE`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13C-01 |
| 核心文件 | 新增无 Qt `MaterialPreviewComposer.*` 或等价 domain composer |
| UI adapter | QImage 转换和可配置伪彩留在 UI service |
| 计划测试 | `material_preview_composer_unit_tests` |
| 必测 | R/G/B/W/S/V、RGB、RGB+W/S/V、RGB+S+W+V、Empty、alpha、覆盖顺序、六通道探针 |
| 生产规则 | `<255` 表示对应通道打印；显示顺序不等于材料冲突优先级 |
| 完成输出 | 同一 RGBWSV buffer 的确定性显示结果 |

### 6.3 13C-03 统一生产预览

状态：`PREPARED / WAIT 13C-02`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13C-02 |
| UI 文件 | 新增 `widgets/UnifiedLayerPreviewWidget.*`，让旧 Layer/Overlay Panel 先通过 adapter 复用新 source |
| 计划测试 | `--ui-smoke-test --case tiff-native-preview` |
| 必测 | 真实 layerIndex/zMm/dpiX/dpiY、单层同源、快速滑层、RGB+S+W+V、探针、错误不跨层兜底 |
| 迁移规则 | wrap first、move later、rewrite last；诊断入口保持独立 |
| 完成输出 | 生产 Preview 单一入口；解锁 09A-05 和 12E-10A |

### 6.4 13C-04 Preview IO 收口

状态：`PREPARED / WAIT 13C-03`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13C-03 |
| 配置文件 | 扩展 preview config/migration，区分 production TIFF、diagnostic image 和 on-demand export |
| Pipeline 接入 | 默认关闭重复生产通道 PNG，但不得关闭 TIFF |
| 计划测试 | config migration、无 preview 目录、before/after IO 统计 |
| 兼容 | 旧 `preview.enabled` 必须有显式迁移语义 |
| 完成输出 | 生产检查不依赖重复 PNG，诊断图仍可按需生成 |

### 6.5 13C-05 阶段收口

状态：`PREPARED / WAIT 13C-04`

| 项目 | 准备内容 |
|---|---|
| 前置 | 13C-04 |
| 主要修改 | fixture、UI smoke、协议回归、用户手册、`REPORT_13C`、索引和上下文 |
| 计划验证 | stripped/tiled、无 preview、RGB+S+W+V、物理比例、RIP strict、Quick CI |
| 完成输出 | M13-4 TIFF 原生统一预览候选 |

## 7. 建议测试 target 汇总

以下 target 名称是规划值，开始任务时必须先检查 CMake 现状并遵循已有命名模式：

```text
model_transform_unit_tests；
scene_view_geometry_unit_tests；
scene_transform_controller_unit_tests；
transformed_model_preflight_unit_tests；
multi_model_scene_config_unit_tests；
scene_effective_config_unit_tests；
scene_document_unit_tests；
grid_layout_policy_unit_tests；
scene_collision_admission_unit_tests；
multi_model_layer_composer_unit_tests；
multi_model_package_writer_unit_tests；
tiff_layer_source_unit_tests；
tiff_layer_cache_unit_tests；
material_preview_composer_unit_tests。
```

每个 C++/Qt 任务至少执行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
```

UI 任务增加对应 `--self-test` 和 `--ui-smoke-test`；联合 package 增加
`rip_reader_test --summary`。未实际运行的命令不得记录为 PASS。

## 8. 外部输入与开发阻断

| 外部输入 | 可继续开发 | 不得完成的结论 |
|---|---|---|
| buildVolume/机器轴未知 | 13A、13B-01..06 fixture、13C | 13B-04 production、13B-07 GO |
| mixed Profile 未决 | P0 `scene_profile_only` | mixed-profile 能力 |
| 22 实例预算未决 | 功能和实测 | 13B-07 production GO |
| 3D backend 未决 | 13A-R1 Qt 2D | 13A-R2/R3 |

外部输入关闭规则以 `DOC_CHECKLIST_13_未决产品输入与阶段Gate.md` 为准。

## 9. 完整性结论

```text
Stage 13 P0 产品需求：完整；
Stage 13 P0 总体技术设计：完整；
Stage 13 P0 验证设计：完整；
17 个近程原子任务的依赖、文件所有权、测试和验收准备：完整；
Stage 13 实现：13A-01..05、13B-01..07 和跨阶段 09A-02 已完成；
Stage 13 production readiness：未完成；
13A-R2/R3、13B-R4 中长期详细设计：有意延后，当前只有 Epic。
```

因此，当前无需继续扩写 P0 通用文档；13B-06 已完成 fixture 开发和状态报告。13B-07 需要补齐
独立的真实模型矩阵执行合同后再进入功能开发，且必须把功能 PASS 与 production GO 分开。
中长期 3D 和自动 nesting 必须在 P0 证据完成后另立 PRD/DEV/DEMO/TASKS，不得夹带进入近程任务。
