# REPORT_13 模型场景、排版联合切片与 TIFF 原生预览准备状态

> 文档版本：v0.9
> 日期：2026-07-27
> 当前状态：P0 DESIGN COMPLETE / 13A-01..05、13B-01、跨阶段 12E-09A-02 COMPLETE / NEXT 13B-02 READY

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
加载可编辑场景后生产切片保持阻断，scene effective config 尚未接入 slicer_cli；
当前没有多模型 buildVolume/layout/collision/joint package；
当前 PreviewWorkspace 已统一 UI 容器和 layerIndex；
当前生产 RGB/像素探针已能读取 TIFF；
当前 W/S/V/overlay 仍主要依赖 preview PNG；
当前没有 RGB+S+W+V 预设。
```

因此，13A 单模型显示、精确变换、镜像和变换后预检已经实现，但阶段收口、多模型排版、联合切片和
TIFF 原生统一预览尚未实现。

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
13B-02：READY FOR DEVELOPMENT，专项 PREP/PROMPT 已按 13A 实际 API 校正；
13C-01：READY FOR DEVELOPMENT，但按单贡献者计划排在模型交互和场景排版之后；
Stage 13 全阶段 production readiness：尚未完成；
Stage 13 已实现能力：6/17 个近程原子任务完成。
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
合计：17 个近程原子任务，当前完成 6；
中长期另有 13A-R2、13A-R3、13B-R4 三个未拆分 Epic。
```

## 8. 下一任务

```text
13B-02 模型列表与实例操作
```

13A-05 已统一回归 13A-01..04，并形成 M13-1 候选。下一步按
`DOC_PREP_13B_02_模型列表与实例操作准备.md` 和
`CODEX_PROMPT_13B_02_模型列表与实例操作执行指令.md` 扩展 1..22 实例的场景草稿；不能提前实现
11x2 规则排版、碰撞、联合切片或生产 package。

## 9. 详细设计完整性

| 范围 | 当前结论 | 是否阻断 13B-02 |
|---|---|---|
| 13A/13B/13C P0 需求 | 完整 | 否 |
| P0 架构、DTO、依赖和协议边界 | 完整 | 否 |
| 17 个近程任务实施准备 | 完整 | 否 |
| 设备 buildVolume/机器轴 | 外部输入未关闭 | 否；阻断 13B production |
| 22 实例正式性能预算 | 外部输入未关闭 | 否；阻断 13B-07 GO |
| 13A-R2/R3 真实 3D | 只有 Epic，等待技术 Spike | 否 |
| 13B-R4 自动 nesting | 只有 Epic，等待 13B-R3 证据 | 否 |

13A-01..05 的实际 API、单测、UI Smoke、用户手册和状态报告已形成 A 级证据；13B-02 已冻结
多实例 identity/resource/list 状态，并以 13A 实际 API 完成执行级准备。后续按
`CODEX_PROMPT_13B_02_模型列表与实例操作执行指令.md` 开发，不提前实现 13B-03。
