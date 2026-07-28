# REPORT_13 模型场景、排版联合切片与 TIFF 原生预览准备状态

> 文档版本：v1.5
> 日期：2026-07-28
> 当前状态：原 P0 14/17 COMPLETE / 13B-08 APPROVED IN PROGRESS / NEXT 13B-08-01

## 1. 本轮完成

已把用户提出的三组需求拆分为：

```text
13A 模型俯视、选择和实例变换；
13B 多模型规则排版与联合切片；
13C RGBWSV TIFF 原生统一预览。
```

已生成：

```text
阶段决策；
依赖和准入矩阵；
路线图；
三组 PRD；
三组 DEV；
三组 DEMO；
统一 TASKS；
统一 CODEX_PROMPT；
本准备状态报告。
```

2026-07-27 执行级补充：

```text
DOC_PREP_13A_01_ModelTransform与ModelInstance合同准备；
DOC_PREP_13B_01_MultiModelScene与EffectiveConfig准备；
DOC_PREP_13C_01_TIFFLayerSource与Cache准备；
DOC_PREP_13_全阶段原子任务实施准备与文件所有权；
DOC_CHECKLIST_13_未决产品输入与阶段Gate；
DOC_DECISION_12X_剩余任务优先级与专项冻结；
TASKS_12_13_后续开发计划总览清单。
```

13A-04 完成后继续补充：

```text
REPORT_13A_02_模型俯视渲染当前状态；
REPORT_13A_03_选择与精确变换当前状态；
REPORT_13A_04_镜像与变换后预检当前状态；
DOC_PREP_13A_03_选择与精确变换准备；
CODEX_PROMPT_13A_03_选择与精确变换执行指令；
DOC_PREP_13A_04_镜像与变换后预检准备；
CODEX_PROMPT_13A_04_镜像与变换后预检执行指令；
DOC_PREP_13A_05_模型俯视与变换阶段收口准备；
CODEX_PROMPT_13A_05_模型俯视与变换阶段收口执行指令；
DOC_PREP_13B_02_模型列表与实例操作准备。
```

## 2. 当前实现事实

```text
13A-01 已实现无 Qt 的 ModelTransform/ModelInstance Public DTO；
13A-02 已实现无 Qt SceneViewGeometry、异步模型导入和 +Z 俯视 Qt 工作区；
模型页已显示毫米网格、+X/+Y、bbox、身份、选择和 blocked 只读状态；
独立“导入模型预览”不启动 slicer_cli、不创建生产 package；
实例变换已冻结 XY translate、rotateZ、uniformScale、mirrorX/mirrorY；
pivot 固定为 source bbox XY 中心和 minZ，实例变换不二次落台；
TransformedModelAdapter 已输出 transformed triangles/bbox/UV/winding/revision；
transform hash、稳定错误和 optimistic revision 已由单测覆盖；
13B-01 已实现 MultiModelScene、ModelSource、ResourceScope 和 Scene Effective Config；
scene/model/instance/revision、requested/derived/effective 和 scene_profile_only 已冻结；
scene effective config 已支持原子保存、回读、hash、cancel 和 stale；
12E-09A-02 已实现 single_model/scene Diagnostic Effective Config；
诊断配置已绑定 current model/instance/transformRevision，并支持原子保存、回读、hash、cancel 和 stale；
当前核心生产配置仍为单一 input.modelPath；
当前已有单模型 X/Y/rotateZ/uniformScale、镜像、居中、重置和 session config 保存；
当前已有 source/transformed 双预检和 Legacy/Global 独立 admission；
当前已有 1..22 有序实例、添加/复制/删除/显隐/锁定、列表/画布选择同步；
当前已有多源多实例 Scene Effective Config 保存、校验、原子写入和回读；
当前已有 11x2 row-major 排版、20/30 mm 边到边净距、原子恢复和 Qt 排版页；
当前已有显式 fixture buildVolume、逐实例越界/admission/revision 和投影碰撞检查；
加载可编辑场景后生产切片保持阻断，scene effective config 尚未接入 slicer_cli；
当前已有 fixture 多模型全局 Raster 和联合内存层合成；
当前已有 joint package、typed scene report 和 Debug/Release 真实模型功能矩阵；
当前 PreviewWorkspace 已统一 UI 容器和 layerIndex；
当前生产 RGB/像素探针已能读取 TIFF；
当前已有 manifest 权威 TiffLayerSource、5 层/256 MiB LRU、稳定错误和 Qt 异步 Worker；
当前 W/S/V/overlay 仍主要依赖 preview PNG；
当前没有 RGB+S+W+V 预设。
```

因此，13A 单模型显示、精确变换、镜像和变换后预检，以及 13B 多实例场景草稿、11x2 排版和
fixture 碰撞准入已经实现；联合切片和 TIFF 原生统一预览尚未实现。

## 3. 关键产品决策

```text
短期显示：+Z 俯视 XY；
短期变换：XY、rotateZ、uniformScale、mirrorX/mirrorY；
Z 落台沿用当前流程；
规则排版：最多 11x2=22；
间距：列 20.00 mm、行 30.00 mm，均为边到边净距且 UI 可配置；
多模型：一个场景、一个 package、每层一个 TIFF；
生产预览：TIFF 为唯一权威像素源；
诊断语义：report/mask 可选，不从 TIFF 猜测；
中期 3D：先比较 VTK/Qt3D/QOpenGLWidget，不直接锁库。
```

## 4. 与 12E 的顺序结论

Stage 13 的全部实现不需要都阻断 09A，但 scene identity 必须先冻结：

```text
13A-01 + 13B-01
  -> scene-aware 12E-09A-02
  -> 13A/13B P0
  -> 13C TIFF 原生预览
  -> 12E-09A-03..06
  -> 12E-10A..D。
```

原因：

```text
09A-02 若继续只绑定 modelPath，进入多模型后会返工；
09A-05/12E-10A 若继续依赖 preview PNG，会重复建立即将淘汰的数据链；
12E-10 仍是单模型双引擎基线收口，不吸收 Stage 13 的全部验收。
```

## 5. 准备审计结论

### 5.1 审计前缺口

原 v0.1 已覆盖产品范围和总体架构，但不足以直接指导首批代码，主要缺少：

```text
现有 modelTransform/autoOrient 与新 InstanceTransform 的先后关系；
pivot、minZ、矩阵次序、镜像 winding 和 transformRevision；
未知 buildVolume 的可序列化状态；
多实例材料 Profile 尚未确认时的 P0 fail-closed 规则；
scene draft 与 production ready 的区别；
TIFF cache 的内存上限、stale generation 和稳定错误码；
首批任务的具体代码落点、测试 target 和验证命令。
```

### 5.2 当前结论

```text
Stage 13 P0 PRD/DEV/DEMO：COMPLETE；
17 个近程原子任务的依赖、建议文件所有权、验证入口和验收输出：COMPLETE；
13A-01：COMPLETE；
13B-01：COMPLETE；
12E-09A-02：COMPLETE；
13A-02：COMPLETE，核心单测和 model-top-view UI Smoke PASS；
13A-03：COMPLETE，精确变换、异步重投影和 session config PASS；
13A-04：COMPLETE，镜像、transformed preflight 和独立双模式准入 PASS；
13A-05：COMPLETE，统一回归、真实资产、三窗口 UI Smoke、用户说明和 M13-1 候选 PASS；
13B-02：COMPLETE，1..22 实例列表、场景操作、多实例保存/回读、UI Smoke 和 Quick CI PASS；
13B-03：COMPLETE，11x2 row-major、原子恢复、Scene Effective Config 和 Qt 排版页已通过回归；
13B-04：FUNCTIONAL FIXTURE COMPLETE，production buildVolume 输入仍 OPEN；
13B-05：FIXTURE COMPLETE；
13B-06：FIXTURE COMPLETE / PRODUCTION INPUT OPEN；
13B-07：FUNCTIONAL MATRIX COMPLETE / PRODUCTION INPUT OPEN；
13B-08：批量导入与当前场景一键切片专项 APPROVED / IN PROGRESS；
13C-01：COMPLETE，manifest/TIFF layer source、LRU、异步 generation 和稳定错误已落地；
13C-02：COMPLETE，无 Qt 材料合成、生产统计、六通道探针和稳定错误已落地；
13C-03：代码前置和任务级 PREP/PROMPT 完整，READY / SEQUENCE WAIT 13B-08；
13D：工作台布局 PRD/DEV/DEMO/TASKS 完整，PREPARED / WAIT 13C-05；
Stage 13 全阶段 production readiness：尚未完成；
Stage 13 已实现能力：14/17 个近程原子任务完成。
```

因此，“Stage 13 P0 开发准备完成”适用于 13A-01..05、13B-01..07、13C-01..05 的任务计划；
它不等于 Stage 13 已实现、13B production GO 或中长期 3D/自动 nesting 已完成详细设计。

## 6. 尚未确认

```text
设备正式 buildVolume；
场景原点和机器轴方向；
不同实例是否可使用不同材料 Profile；
多模型生产性能预算；
未来是否需要真正自动 nesting；
中期 3D 后端选择。
```

处理规则：

```text
buildVolume/轴方向不阻断 13A-01、13B-01 schema 和 13C，但阻断 13B-04 production；
多 Profile 未确认时，P0 使用 scene_profile_only；
性能预算不阻断功能开发，阻断 13B-07 GO；
3D backend 不阻断 Qt 俯视，阻断 13A-R2/R3。
```

## 7. 任务数量

```text
13A 近程：5；
13B 近程：7；
13C 近程：5；
原 P0 合计：17 个近程原子任务，当前完成 14；
插入专项：13B-08 四个任务、13D 四个任务；13B-08 已获批准，原 P0 完成率仍单独统计；
中长期另有 13A-R2、13A-R3、13B-R4 三个未拆分 Epic。
```

## 8. 下一任务

```text
实现并验证 13B-08-01 批量导入与主切片入口
```

13B-07 已完成真实 OBJ/3MF 的 1/11/12/22 Debug/Release 功能矩阵、复用、单 package 和 RIP strict，
并保持 production INPUT_OPEN；13C-01 TIFF 原生层数据源和 13C-02 同层材料显示合成已完成。
13C-03 的 UI 接线、并发、坐标和 smoke 合同已补齐，但当前 Qt 多模型场景尚不能直接切片，推荐先完成
13B-08 的批量导入、显式 scene route 和主动作闭环，再恢复 13C-03；
设备输入未关闭前不得给出
13B production GO。

## 9. 详细设计完整性

| 范围 | 当前结论 | 是否阻断 13C-03 |
|---|---|---|
| 13A/13B/13C P0 需求 | 完整 | 否 |
| P0 架构、DTO、依赖和协议边界 | 完整 | 否 |
| 17 个近程任务实施准备 | 完整 | 否 |
| 13B-05 联合内存层 | FIXTURE COMPLETE | 否 |
| 13B-06 单 package/scene report | FIXTURE COMPLETE | 否 |
| 设备 buildVolume/机器轴 | 外部输入未关闭 | 否；阻断 13B production |
| 22 实例正式性能预算 | 外部输入未关闭 | 否；阻断 13B production GO |
| 13A-R2/R3 真实 3D | 只有 Epic，等待技术 Spike | 否 |
| 13B-R4 自动 nesting | 只有 Epic，等待 13B-R3 证据 | 否 |
| 13B-08 场景作业流 | PRD/DEV/DEMO/TASKS 及 01..04 PREP/PROMPT 已批准 | 是；当前产品主流程 |
| 13D 工作台布局 | 总体准备完成，等待 13C-05 | 否；不得提前重排 |

13A-01..05 和 13B-02..07 的实际 API、单测、UI Smoke、用户手册及状态报告已形成 A 级证据；
13B-03 已冻结并实现 row-major、11x2、20/30 mm 边到边净距、锁定和原子提交规则。13B-04
已关闭 13B-05 功能 Fixture Gate，13B-06 已完成单 package/scene report fixture 闭环，13B-07
真实模型功能矩阵已通过；正式 buildVolume/原点/机器轴和 22 实例预算未关闭，因此
production acceptance 仍阻断。

## 10. 2026-07-28 UI 作业流插入结论

用户截图暴露的“无明显切片按钮、只能单文件导入、两个右侧栏挤压画布”已形成独立证据。代码审计确认：

```text
场景存在实例时旧 run_slicer_button_ 被主动禁用；
旧 OnImportModelAndSlice 不消费当前 SceneDocument；
OnImportModelPreview 使用单文件对话框；
13B 联合切片核心存在，但缺少 Qt 产品场景入口；
模型上下文右栏与全局参数/诊断右栏职责重叠，诊断还与底部 Dock 重复。
```

因此正式拆为：

```text
13B-08：功能优先，解决批量导入和当前场景一键切片；
13D：13C-05 后执行，解决顶部主动作、单一检查器和诊断 Dock 布局。
```

本次只完成文档和计划准备，未宣称上述代码已经实现。
