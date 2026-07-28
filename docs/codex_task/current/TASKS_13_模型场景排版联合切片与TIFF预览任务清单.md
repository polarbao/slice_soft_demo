# TASKS 13 模型场景、排版联合切片与 TIFF 原生预览任务清单

> 状态：原始 17 任务全部 COMPLETE；13B-08 COMPLETE；NEXT 13D-01
> 日期：2026-07-28
> 执行原则：每次只执行用户明确授权的原子任务

## 1. 固定边界

```text
Qt 只在 slicer_debug_ui；
scene/layout/pipeline 使用 STL/domain DTO；
Z 落台沿用当前生产逻辑；
短期只开放 XY、rotateZ、uniformScale、mirrorX/mirrorY；
多模型联合切片输出一个 package 和每层一个 TIFF；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
不做自动 nesting 和跨模型联合支撑；
生产预览以 TIFF 为权威，诊断语义不从 TIFF 猜测；
不允许 silent fallback。
```

## 2. 13-00 文档与准入

状态：COMPLETE

成果：

```text
DOC_DECISION；
依赖/Gate 矩阵；
ROADMAP；
13A/13B/13C PRD、DEV、DEMO；
TASKS/CODEX_PROMPT；
准备状态报告和上下文同步；
Stage 12/13 跨阶段执行看板。
```

执行级准备：

```text
13A-01：COMPLETE，Public DTO、变换数学、adapter 和单测已落地；
13B-01：COMPLETE，MultiModelScene、ResourceScope、Scene Effective Config 和单测已落地；
13C-01：DOC_PREP_13C_01，READY，但按单贡献者顺序排在 identity wave 后；
13A-01..05、13B-01..07、13C-01..05：DOC_PREP_13 全阶段实施准备已覆盖；
13A-03/04 已实现，13A-05 已完成 M13-1 候选收口，13B-02 已实现 1..22 实例列表与场景草稿；
13B-03 已完成 11x2 规则排版、SceneDocument 原子恢复、配置回读和 Qt 排版页；
13B-04 已完成 SceneCollisionService、稳定错误和 fixture 功能 Gate；
13B-05 已完成 Legacy/Global scene adapter、共享 Grid、联合内存层、测试和状态报告；
13B-06 已完成单 package、typed scene report、原子发布和 RIP strict；
13B-07 已完成真实 OBJ/3MF 的 1/11/12/22 Debug/Release 功能矩阵；
Stage 13 未决产品输入：DOC_CHECKLIST_13，按具体 Gate 阻断，不虚构设备值。
```

## 3. 13A-01 ModelTransform 与 ModelInstance 合同

状态：COMPLETE（2026-07-27）

目标：

```text
建立无 Qt 的 ModelTransform/ModelInstance DTO；
固定 pivot、变换次序、revision 和 Z 落台边界；
提供变换后的 bbox/triangle adapter；
保持单模型未配置变换时行为不变。
```

验收：

```text
矩阵数学；
uniformScale；
rotateZ；
XY translate；
identity 不变性；
落台不变性；
Public API Doxygen；
targeted unit test。
```

实际证据：

```text
src/slicer_core/scene/ModelTransform.*；
src/slicer_core/scene/ModelInstance.*；
src/slicer_core/geometry/TransformedModelAdapter.*；
tests/unit/model_transform/Main.cpp；
model_transform_unit_tests PASS；
model_preflight_service_unit_tests / slice_pipeline_router_unit_tests PASS。
```

## 4. 13B-01 MultiModelScene 与 Scene Effective Config

状态：COMPLETE（2026-07-27）

目标：

```text
MultiModelScene/ModelSource/ResourceScope；
modelId/instanceId；
sceneRevision/transformRevision；
layout/buildVolume schema；
单模型配置兼容；
requested/derived/effective transaction；
不覆盖 fixture。
```

实际验收：

```text
schema round-trip 与稳定 hash；
save/readback/cancel/stale 与源文件保护；
路径资源隔离、duplicate/missing reference 和 mixed Profile 负向测试；
unresolved draft / fixture functional / device production buildVolume Gate；
等价浮点和镜像变换组合；
revision JSON 精确范围；
Debug 全量构建、CTest 60/60、Qt self-test、Quick CI PASS。
```

状态报告：

```text
docs/slice/REPORT/REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md
```

完成后 Gate：允许执行 scene-aware `12E-09A-02`。

## 5. 12E-09A-02 Scene-aware Diagnostic Effective Config

状态：COMPLETE（2026-07-27）

要求：

```text
保留现有 current-model 诊断边界；
subjectType 支持 single_model/scene；
结果绑定 sceneId/instanceId/revision；
不实现排版或联合切片；
完成后回到本任务清单继续 13A/13B。
```

状态报告：

```text
docs/slice/REPORT/REPORT_12E_09A_02_SceneAwareDiagnosticEffectiveConfig当前状态.md
```

## 6. 13A-R1 俯视和变换

### 13A-02 俯视渲染

状态：COMPLETE（2026-07-27）

目标：+Z 俯视、XY 轴、毫米网格、轮廓、包围盒、选择和适应视图。

实际证据：

```text
SceneViewGeometry、SceneDocument、SceneSelectionModel；
generation-aware ModelTopViewLoader；
QPainter ModelTopViewWidget 和独立导入模型预览入口；
scene_view_geometry_unit_tests、model-top-view UI Smoke；
REPORT_13A_02_模型俯视渲染当前状态.md。
```

### 13A-03 选择与精确变换

状态：COMPLETE（2026-07-27）

目标：X/Y、rotateZ、uniformScale、居中、重置、session config 同步。

独立准备：`DOC_PREP_13A_03_选择与精确变换准备.md` 和
`CODEX_PROMPT_13A_03_选择与精确变换执行指令.md`。

实际证据：`REPORT_13A_03_选择与精确变换当前状态.md`。

### 13A-04 镜像与 post-transform preflight

状态：COMPLETE（2026-07-27）

目标：mirrorX/mirrorY、winding/normal/UV 修正、重新准入、blocked UI。

实际证据：`REPORT_13A_04_镜像与变换后预检当前状态.md`。

### 13A-05 阶段收口

状态：COMPLETE（2026-07-27）

目标：UI self-test、三窗口 smoke、用户说明、REPORT_13A 和回归。

独立准备：`DOC_PREP_13A_05_模型俯视与变换阶段收口准备.md` 和
`CODEX_PROMPT_13A_05_模型俯视与变换阶段收口执行指令.md`。

实际证据：`REPORT_13A_模型俯视工作区与实例变换当前状态.md`。

## 7. 13B 多模型与规则排版

### 13B-02 模型列表与实例操作

状态：COMPLETE（2026-07-27）

目标：添加、复制、删除、选择、隐藏、锁定，显示 modelId/instanceId/transform/admission。

专项准备：`DOC_PREP_13B_02_模型列表与实例操作准备.md` 和
`CODEX_PROMPT_13B_02_模型列表与实例操作执行指令.md`。

实际证据：

```text
SceneDocument 1..22 有序实例和原子命令；
ModelListPanel 与多实例 ModelTopViewWidget；
同源只读资源身份共享和多源 ResourceScope；
多实例 Scene Effective Config 保存/回读；
scene_document_unit_tests、multi-model-list UI Smoke、Quick CI PASS；
REPORT_13B_02_模型列表与实例操作当前状态.md。
```

### 13B-03 11x2 规则排版

状态：COMPLETE（2026-07-27）

目标：

```text
maxColumns=11；
maxRows=2；
columnGapMm=20.00；
rowGapMm=30.00；
edge_clearance；
row_major；
UI 步长 0.01 mm；
稳定 derived transform。
```

专项准备：`DOC_PREP_13B_03_11x2规则排版准备.md` 和
`CODEX_PROMPT_13B_03_11x2规则排版执行指令.md`。

实际证据：

```text
GridLayoutPolicy 无 Qt 核心；
1/11/12/22、容量、不同 bbox、隐藏、锁定、stale 和确定性单测；
SceneDocument 原子 apply/restore；
requested/derived/effective transform 保存与回读；
中文 SceneLayoutPanel；
scene-grid-layout 三窗口 UI Smoke 和 Quick CI PASS；
REPORT_13B_03_11x2规则排版当前状态.md。
```

### 13B-04 幅面、碰撞和逐实例准入

状态：COMPLETE（FUNCTIONAL FIXTURE）/ PRODUCTION INPUT OPEN

目标：buildVolume、AABB+精确投影碰撞、post-transform admission、fail-closed。

专项准备：`DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md` 和
`CODEX_PROMPT_13B_04_幅面碰撞与逐实例准入执行指令.md`。

实际证据：

```text
SceneCollisionService 无 Qt 核心；
显式 lower-left/center fixture buildVolume；
四向越界、逐实例 admission、scene/transform revision 和 geometry identity；
AABB 快筛与投影三角形正面积精确碰撞；
隐藏实例跳过、稳定错误和确定性结果；
scene_collision_admission_unit_tests、scene-grid-layout UI Smoke、Quick CI PASS；
REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md。
```

### 13B-05 全局 Raster 与联合层合成

状态：FIXTURE COMPLETE

目标：共享 Z 层序、全局 XY grid、逐实例局部层映射、场景 closure、无重叠合成。

专项准备：`DOC_PREP_13B_05_全局Raster与联合层合成准备.md` 和
`CODEX_PROMPT_13B_05_全局Raster与联合层合成执行指令.md`。

实际证据：`REPORT_13B_05_全局Raster与联合层合成当前状态.md`。

### 13B-06 单一 package 与 scene report

状态：FIXTURE COMPLETE / PRODUCTION INPUT OPEN

目标：共享 writer、每层一个 TIFF、manifest 可选 scene 摘要、per-instance report、原子发布、RIP strict。

专项准备：`DOC_PREP_13B_06_单Package与SceneReport准备.md` 和
`CODEX_PROMPT_13B_06_单Package与SceneReport执行指令.md`。

实际证据：`REPORT_13B_06_单Package与SceneReport当前状态.md`。

### 13B-07 真实模型矩阵与收口

状态：FUNCTIONAL MATRIX COMPLETE / PRODUCTION INPUT OPEN

目标：1/11/12/22 实例、OBJ/3MF、资源隔离、碰撞/越界、性能和 REPORT_13B。

专项准备：`DOC_PREP_13B_07_真实模型矩阵与阶段收口准备.md` 和
`CODEX_PROMPT_13B_07_真实模型矩阵与阶段收口执行指令.md`。

实际证据：

```text
multi_model_scene_matrix 与 run_13b_07_real_model_matrix.ps1；
1/11/12/22 实例和 OBJ+Texture2D 3MF 正向矩阵；
纯 XY 平移实例本地层复用，22 实例仅调用两个唯一模型生产器；
每 case 一个 package、每全局 layerIndex 一个 TIFF、RIP strict PASS；
23/overlap/out-of-bounds/missing-volume/stale-revision 按预期阻断；
Debug/Release 矩阵和 Quick CI PASS；
REPORT_13B_07_真实模型矩阵与阶段收口当前状态.md。
```

生产 Gate 继续等待正式设备 buildVolume、原点/X/Y 轴向和 22 实例预算。

## 8. 13B-08 场景作业流收口插入专项

状态：`COMPLETE / PRODUCTION INPUT OPEN`

触发原因：Stage 13B 核心已经能完成多模型联合内存层和单 Package 功能矩阵，但 Qt 工作台尚未把
当前 `SceneDocument` 接入产品场景切片入口；批量导入也仍是单文件对话框。因此三模型排版后旧
`运行切片` 被主动禁用，属于主流程功能断点。

任务：

```text
13B-08-01：批量导入队列、容量、部分失败和单次自动排版；
13B-08-02：无 Qt 场景生产服务和显式 --scene-config CLI；
13B-08-03：Qt“切片当前场景”主动作、预检、进程和结果回载；
13B-08-04：1/3/11/12/22、OBJ/3MF、RIP strict 和阶段收口。
```

正式入口：`TASKS_13B_08_场景作业流收口任务清单.md`。13B-08 已完成，阶段报告和
Debug/Release 真实作业流证据已落地。

## 9. 13C TIFF 原生统一预览

### 13C-01 TiffLayerSource 与 LRU

状态：COMPLETE（2026-07-28）

完成：manifest layer index、stripped/tiled 解码、5 层/256 MiB 默认 LRU、Qt 异步 Worker、
取消/stale、防跨层、稳定错误和定向单测。

### 13C-02 MaterialPreviewComposer

状态：COMPLETE（2026-07-28）

完成：无 Qt 的 R/G/B/W/S/V、RGB、RGB+W/S/V、RGB+S+W+V、Empty/Occupancy、
生产统计、稳定错误和六通道像素探针。

### 13C-03 统一生产预览

状态：COMPLETE（2026-07-28）

目标：合并生产层和材料叠加控制，诊断入口独立，真实 layerIndex/zMm/dpiX/dpiY。

完成后 Gate：允许执行 `12E-09A-05` 的同层语义 Preview。

执行合同：`DOC_PREP_13C_03_UnifiedProductionPreview准备.md` 和
`CODEX_PROMPT_13C_03_UnifiedProductionPreview执行指令.md`。

### 13C-04 Preview IO 收口

状态：COMPLETE（2026-07-28）

完成：`tiff_native` 默认无 preview 目录、显式诊断图、旧 enabled 迁移、Qt 开关、逐层 TIFF
hash 一致和 IO 耗时对比。

### 13C-05 阶段收口

状态：COMPLETE（2026-07-28）

完成：无 preview 目录共享 writer package、stripped/tiled、635/600、RGB+S+W+V、探针、
异步/cache/fail-closed、RIP strict 和阶段报告已闭环。

## 10. 中长期任务

### 13D Qt 工作台信息架构与布局收口

状态：`13D-01 READY / 13D-02..04 WAIT PREVIOUS`

目标：建立顶部作业栏、单一 Context Inspector、可折叠项目区和统一 DiagnosticsDock，解决两个右侧
常驻区域挤压画布及诊断入口重复。13C 最终预览导航已经冻结，当前按 13D 原子顺序实施。

任务：`13D-01..04`，详见 `TASKS_13D_Qt工作台布局收口任务清单.md`。

### 13A-R2 3D Viewport Spike

状态：PLANNED / WAIT 13A-R1

比较 VTK、Qt3D 和 QOpenGLWidget。不得在没有 Spike 证据前引入大型依赖。

### 13A-R3 完整 3D 交互

状态：PLANNED / WAIT R2 DECISION

三轴 gizmo、撤销/重做、对齐、吸附、多选和标准视图。

### 13B-R4 自动排版

状态：PLANNED / WAIT 13B-R3

真正 nesting、自动朝向、跨模型支撑和增量重切片另立 PRD/DEV。

## 11. 数量与当前入口

```text
13A-01..05：5 个近程原子任务；
13B-01..07：7 个近程原子任务；
13C-01..05：5 个近程原子任务；
原始合计：17 个近程原子任务，当前完成 17；
本轮插入：13B-08 四个任务和 13D 四个任务，共 8 个；
13B-08-01..04 已完成；13C-05 Gate 已解除，13D-01 可进入开发；
13A-R2、13A-R3、13B-R4 是未拆分的中长期 Epic。
```

当前推荐入口为执行 `13D-01 顶部作业栏`；`13C-01..05` 和 `13B-08` 已完成；
13C-01 已完成
TIFF-native source、LRU、异步 generation 和稳定错误；13C-02 已完成确定性材料显示合成、
统计和六通道探针；13B-08 已完成真实 Qt 作业流、OBJ/3MF 和 RIP strict 矩阵，
多模型场景链的功能开发闭环；设备 buildVolume/轴向和 22 实例预算继续阻断 13B production GO，
但不阻断 13C TIFF 原生预览开发。

## 12. 任务验证规则

每个任务开始：

```powershell
git branch --show-current
git status --short
```

文档/配置任务：

```powershell
git diff --check
```

C++/Qt 任务至少：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_ci_quick.ps1
```

UI 任务增加对应 `--self-test` 和 `--ui-smoke-test`。联合 package 必须运行 `rip_reader_test --summary`。

## 13. 停止条件

```text
设备 buildVolume 未冻结时，不得把 fixture 幅面标记生产通过；
scene identity 未完成时，不得实现多模型生产路由；
碰撞/越界/admission 失败时停止写包；
13C 未完成时，09A-05 不复制新的 preview PNG 合成路线；
13B-08 未完成时，不得强制启用旧单模型按钮冒充当前场景切片；
13C-05 未完成时，不得开始 13D MainWindow 全局重排；
每个原子任务完成后停止，等待用户明确授权下一任务。
```
