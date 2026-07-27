# TASKS 13 模型场景、排版联合切片与 TIFF 原生预览任务清单

> 状态：13-00 / 13A-01..05 / 13B-01..03 / 12E-09A-02 COMPLETE / NEXT 13B-04 FIXTURE READY
> 日期：2026-07-27
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
13B-04 已按实际 GridLayoutPolicy/SceneDocument API 补齐独立 PREP/PROMPT；
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

状态：READY FOR FIXTURE DEVELOPMENT / PRODUCTION INPUT OPEN

目标：buildVolume、AABB+精确投影碰撞、post-transform admission、fail-closed。

专项准备：`DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md` 和
`CODEX_PROMPT_13B_04_幅面碰撞与逐实例准入执行指令.md`。

### 13B-05 全局 Raster 与联合层合成

状态：PREPARED / WAIT 13B-04

目标：共享 Z 层序、全局 XY grid、逐实例局部层映射、场景 closure、无重叠合成。

### 13B-06 单一 package 与 scene report

状态：PREPARED / WAIT 13B-05

目标：共享 writer、每层一个 TIFF、manifest 可选 scene 摘要、per-instance report、原子发布、RIP strict。

### 13B-07 真实模型矩阵与收口

状态：PREPARED / WAIT 13B-06

目标：1/11/12/22 实例、OBJ/3MF、资源隔离、碰撞/越界、性能和 REPORT_13B。

## 8. 13C TIFF 原生统一预览

### 13C-01 TiffLayerSource 与 LRU

状态：READY FOR DEVELOPMENT / SCHEDULE AFTER IDENTITY WAVE

目标：manifest layer index、异步解码、5 层默认 LRU、取消/stale、防跨层。

### 13C-02 MaterialPreviewComposer

状态：PREPARED / WAIT 13C-01

目标：R/G/B/W/S/V、RGB、RGB+W/S/V、RGB+S+W+V、Empty 和像素探针。

### 13C-03 统一生产预览

状态：PREPARED / WAIT 13C-02

目标：合并生产层和材料叠加控制，诊断入口独立，真实 layerIndex/zMm/dpiX/dpiY。

完成后 Gate：允许执行 `12E-09A-05` 的同层语义 Preview。

### 13C-04 Preview IO 收口

状态：PREPARED / WAIT 13C-03

目标：常规生产不写重复通道 PNG，诊断 preview 按需，兼容旧配置，记录 before/after IO。

### 13C-05 阶段收口

状态：PREPARED / WAIT 13C-04

目标：无 preview 目录 smoke、stripped/tiled、RGB+S+W+V、RIP/协议回归、REPORT_13C。

## 9. 中长期任务

### 13A-R2 3D Viewport Spike

状态：PLANNED / WAIT 13A-R1

比较 VTK、Qt3D 和 QOpenGLWidget。不得在没有 Spike 证据前引入大型依赖。

### 13A-R3 完整 3D 交互

状态：PLANNED / WAIT R2 DECISION

三轴 gizmo、撤销/重做、对齐、吸附、多选和标准视图。

### 13B-R4 自动排版

状态：PLANNED / WAIT 13B-R3

真正 nesting、自动朝向、跨模型支撑和增量重切片另立 PRD/DEV。

## 10. 数量与当前入口

```text
13A-01..05：5 个近程原子任务；
13B-01..07：7 个近程原子任务；
13C-01..05：5 个近程原子任务；
合计：17 个近程原子任务，当前完成 8；
13A-R2、13A-R3、13B-R4 是未拆分的中长期 Epic。
```

当前唯一推荐入口为 `13B-04 fixture 幅面、碰撞和逐实例准入`。其独立 PREP/PROMPT 已按
13B-03 的 `GridLayoutPolicy`、`SceneDocument` 和 Scene Effective Config API 补齐。`13C-01`
技术上可独立开始，但单贡献者按固定顺序先完成模型场景链，再进入 TIFF 原生预览。

## 11. 任务验证规则

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

## 12. 停止条件

```text
设备 buildVolume 未冻结时，不得把 fixture 幅面标记生产通过；
scene identity 未完成时，不得实现多模型生产路由；
碰撞/越界/admission 失败时停止写包；
13C 未完成时，09A-05 不复制新的 preview PNG 合成路线；
每个原子任务完成后停止，等待用户明确授权下一任务。
```
