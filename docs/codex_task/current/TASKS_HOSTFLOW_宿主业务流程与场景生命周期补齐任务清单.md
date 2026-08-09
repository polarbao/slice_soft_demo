# TASKS_HOSTFLOW 宿主业务流程与场景生命周期补齐任务清单

> 文档状态：**ACTIVE — H-D-01/05 完成 / H-E E1 准备通过**（H-A/H-B/H-C 完成；外部 ACK 延期）
> 版本：v3.4 ｜ 日期：2026-08-09 ｜ 激活日期：2026-08-07
> **定位：独立补充专项，不属于 Stage 14 任何任务组，不占阶段编号。**
> 起因：14E 交付的 `slicer_ui_host_sim` 是「技术验证壳」，未实现原设想的完整业务流程
> 上游：`DOC_DECISION_14_UI_宿主模拟改造专项.md`、`contracts/slicer_capability_dtos.json`
> 证据等级：A=已核实代码事实，B=目标设计，P=判断
>
> **2026-08-07 排期覆盖说明**：用户已授权 Stage 14F 暂缓打印侧验证并立即冻结
> Stage 14 对外接口。本文 H-A 仍是有效缺口分析，但不得在 14F 收口前修改冻结合同；
> 用户已于 2026-08-07 授权 HQ-01，并建立
> `DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md`。H-A-01、H-A-02、H-A-04
> 已按独立原子卡完成；H-A-03、H-B-01..08 也已完成。用户已于
> 2026-08-08 授权 HQ-08-A；H-C-01..03 已完成。
>
> **2026-08-08 复核结论（A 级）**：H-A/H-B/H-C 十五张卡的**业务与数据链路**确已闭合，
> 但**视图显示链路未接线** —— 中央画布仍是占位 `QLabel`，渲染与交互内核只被批处理自检
> 引用。因此 §4「操作员能独立完成」的验收口径**尚未真正达成**，本专项不能算关闭。
> 新增 **H-D 组（视图接线与端到端可操作性，6 张卡）**，状态 `PROPOSED`，待用户点名启动。
>
> **同时发现第二层缺口**：即使 H-D 全部完成，参考宿主**仍不等于封装前切片软件的操作能力** ——
> H-C-02 机器计划中 8 项 `adapt_to_host_profile` 的替代物（支撑编辑、材料工艺 Profile 编辑、
> 生产纹理设置）**当前并不存在**，另缺 STL 与批量导入。列为 **H-E 组（6 张卡）**，
> 但其范围**超出用户此前「HQ-03 只做核心流程」的裁决**，须由用户重新裁决（**HQ-09**）。
> 场景/项目保存加载另立 **HQ-10**。
>
> **2026-08-08 用户裁决**：**HQ-09 = 乙**（提升到等价水位，分 E1/E2/E3 三批）；
> **HQ-10 = 甲**（场景保存加载归 PrintApp，参考宿主不实现）。
> 权威记录：`DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md`。
> H-E 六卡范围已授权，但仍须用户点名单张原子卡后方可开工。

---

## 0. 为什么需要这个专项

原设想：**宿主模拟实现切片软件的完整功能，供打印软件参考，使移植只需最少改动。**

实际交付（A）：`HostMainWindow.h` 自述为 *"Minimal Qt reference shell that consumes only the public module ABI"*，
成员仅有 `ModuleClient` / 视图设置 / 模块自述文本框；`HostMainWindow.cpp` 中
`QFileDialog`、`QPushButton`、`slice` **零命中**。`CapabilityCoverageRunner` 自述为
*"Executes the frozen Stage 14 capability checklist"* —— 是**批处理测试跑批**，不是用户可操作流程。

**根因（P）**：14E 的九张卡全部是「能力/机制」导向，**没有一张是「业务流程」导向**。
14E-04b 写「P0 五项端到端打通并可演示」，被合理地实现为自动跑批 ——
技术上满足卡片，但与设想不是一回事。

```text
「能力覆盖」 = 每个 API 被调用过一次        ← 已完成
「业务流程」 = 人能用它完成一次真实作业      ← 缺失
```

---

## 1. 功能规格来源：主干 `slicer_debug_ui` 就在 HEAD 上（A）

**不需要回溯 commit `4fd00978`。** 14E 的决策是「主干 `apps/slicer_debug_ui/` 一行不改」，
因此那个版本的 UI 功能**当前仍完整存在于 HEAD**。

这比翻历史 commit 好三点：

```text
① 是【活代码】，可直接编译运行
② 可与 slicer_ui_host_sim 【并排运行做 A/B 对照】—— 同模型同 Profile 比行为
③ 已含 Stage 15 的最新改造（TextureWhitePreflightService 等），不是过时快照
```

### 1.1 主干功能清单（68 个头文件，按业务流程分组 · A）

| 业务环节 | 主干实现 |
|---|---|
| **模型导入与管理** | `ModelListPanel`、`SceneModelRepository`、`ModelPreflightPanel/Controller/Presenter`、`TransformedModelPreflightLoader` |
| **排版与变换** | `SceneLayoutPanel`、`ModelTransformPanel`、`ModelTopViewWidget`、`ModelTopViewLoader`、`SceneActionBar` |
| **参数设置** | `ConfigEditorPanel`、`QuickConfigPanel`、`SliceSettingsModel`、`ConfigDocument`、`MaterialPolicyEditor`、`MaterialProcessProfileEditor`、`MaterialRoleMappingEditor`、`SupportEditor`、`ProductionTextureSettingsPanel/Model/Contract`、`DiagnosticSettingsPanel` |
| **设置辅助** | `SettingHelpPanel`、`HelpTextProvider`、`ConfigValidator`、`EffectiveConfigGenerator`、`ConfigDiffPanel/Model` |
| **Profile / 生产模式** | `ProductionModePanel`、`ProductionModeCatalog`、`ScenarioRegistry`、`ProductionProfileSourceResolver`、`SingleMaterialReliefResolver` |
| **切片执行** | `ProductionSliceRunSession`、`SlicePreflightCoordinator`、`SliceProgressProtocolParser`、`ProcessRunner`、`SliceTimingPanel`、`ProductionPackageResultValidator` |
| **结果与预览** | `PackageLoader`、`ReportLoader`、`PreviewReportIndex`、`PreviewPanel`、`PreviewWorkspace`、`PreviewOverlayPanel`、`PreviewPhysicalScale`、`LayerPreviewPanel`、`LayerPreviewDataProvider`、`TiffLayerLoadWorker`、`MaterialPreviewImageAdapter`、`ChannelChartPanel`、`ReportPanel`、`MaterialClosurePanel/ReportInterpreter`、`DiagnosticSemanticPreviewPanel` |
| **工程支撑** | `WorkspaceLayoutState`、`ProjectToolsDock`、`ToolPaths`、`DiagnosticsDock`、`ContextInspector`、`LogPanel`、`UiSmokeTestRunner` |

### 1.2 三桶分类（决定移植成本，同时是 14E-06 可移植清单的基础 · B）

```text
桶 A · 可直接移植   纯 UI 控件，输入为数据、不依赖 core 类型
                    候选：ConfigEditorPanel、SettingHelpPanel、LogPanel、
                          ChannelChartPanel、PreviewOverlayPanel、ModelListPanel
桶 B · 需改走 ABI   当前直连 core，宿主侧要改为经 ModuleClient
                    候选：ModelPreflightController、SceneModelRepository、
                          ProductionSliceRunSession、PackageLoader、
                          LayerPreviewDataProvider、EffectiveConfigGenerator
桶 C · 切片专有     打印侧不需要
                    候选：UiSmokeTestRunner、DiagnosticsDock、
                          OpenVdbUtilityReportInterpreter、ConfigDiffPanel
```

> 分桶必须**逐文件核实**再定稿，上表是候选而非结论（任务 H-C-01）。

---

## 2. 🔴 硬阻断：ABI 缺场景生命周期（A）

`contracts/slicer_capability_dtos.json`：

```json
:89  { "path": "operations[].type",
       "type": "enum:translate|rotateZ|uniformScale|mirror", "required": true }
:90  { "path": "operations[].instanceId", "type": "string", "required": true }
:84  { "path": "sceneHandle", "type": "integer", "required": false }
:85  { "path": "scene",       "type": "object",  "required": false }
```

**只有四种变换，没有 `addInstance` / `removeInstance`；15 项能力里也没有 `scene.create` / `scene.close`。**
每个 operation 都必须带 `instanceId`，即实例必须已存在。

**结论：ABI 层没有「把导入的模型加进场景」的通路。** `model.import` 只返回 `modelId`。
宿主只能靠传完整 `scene` 对象绕过 —— 而那要求宿主知道 scene schema 的全部内部结构：

```text
宿主自己拼 scene JSON  →  宿主必须理解内部数据结构
                       →  违背 ABI 封装初衷
                       →  打印软件移植时同样要拼
                       →  【最少改动移植】的目标落空
```

### 2.1 采纳方案：扩展 operations 枚举（不新增能力、不新增导出）

约束：**11 个导出符号与 15 项能力已冻结**，不得新增第 16 项能力。

```jsonc
// 扩展后
"operations[].type":
  "enum:addInstance|removeInstance|translate|rotateZ|uniformScale|mirror"

// addInstance 专有字段
{ "path": "operations[].modelId",        "type": "string",     "required": false }  // addInstance 必填
{ "path": "operations[].initialTransform","type": "object",    "required": false }
{ "path": "operations[].assignInstanceId","type": "string",    "required": false }  // 缺省由模块分配

// removeInstance 只需既有的 operations[].instanceId
```

**场景创建隐含化**：请求不带 `sceneHandle`、`scene` 缺省或为 `{}`，且 operations 至少包含
`addInstance` 时，模块创建新空场景并在响应中返回 `sceneHandle`。仅含旧变换的请求保持原失败语义。

**场景释放边界修正**：`pm_release` 只接受 `pm_job_t*`，不能关闭整数 `sceneHandle`。当前 scene session
绑定 `pm_module_t`，由 `pm_destroy` 统一释放；显式 per-scene close 不在 H-A-01 范围内。

```text
✅ PM_SPI_VERSION 不变    ✅ 11 个导出不变    ✅ 15 项能力不变
✅ minor 兼容（新枚举值 + 可选字段，旧宿主不受影响）
```

> 已评估但未采纳：新增 `scene.create`/`scene.close` 两项能力（违反 15 项冻结，需 major 决策与打印侧重新回签）；
> 「无状态全量场景 + 公开 scene schema」（宿主永久依赖内部结构，与移植目标相悖）。

---

## 3. 任务卡

### H-A 组 · ABI 场景生命周期（阻断项，最高优先）

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-A-01** | 扩展 `operations[].type` 枚举与 `addInstance`/`removeInstance` 字段；仅在空场景首操作为 `addInstance` 时允许缺省 `sceneHandle` 并隐式创建；契约文档与 Schema 同步 | HQ-01 | 契约测试通过；**`PM_SPI_VERSION`、11 导出、15 能力全部不变**；旧请求（不含新枚举）行为语义不变，运行时回归由 H-A-02 锁定 | **COMPLETE（2026-08-07）** |
| **H-A-02** | `SceneFacade` 与 `SceneCapabilityAdapter` 实现新操作；实例 id 分配、重复添加、删除不存在实例的负例 fail-closed | H-A-01、HQ-07 | Debug/Release 单测；`operationId` 幂等对新操作同样成立 | **COMPLETE（2026-08-07）** |
| **H-A-03** | **端到端最小闭环**：从**空场景**开始，仅经 11 个导出完成 `import → addInstance → layout/transform → slice → verify` | H-A-02、H-A-04 | 宿主**不构造任何 scene JSON**；`slicer_host_sim`（纯 C）与 `slicer_ui_host_sim` 各跑通一次 | **COMPLETE（2026-08-07）** |
| **H-A-04** | **补齐规则排版能力**（行列 + 间距 + 碰撞）：在既有 `scene.apply_operation` 增加 `applyGridLayout`，复用 `GridLayoutPolicy`（13B-03 的 11×2 排版），**不新增第 16 项能力** | H-A-01 | 宿主一次调用完成 22 实例规则排版；结果与主干 `SceneLayoutPanel` 共用策略；仍不新增导出/能力 | **COMPLETE（2026-08-07）** |

> 🔴 **H-A-04 的发现过程与必要性**
>
> 打印侧 `CLD_04` §4.2 列出能力包应提供 **8 项**能力，其中 `scene.layout`
> 「规则排版（行列 + 间距）+ 碰撞检测 · 进程内」**在我方冻结的 15 项能力中没有对应项**。
>
> 后果与 H-A 的 `addInstance` 缺口同性质：切片侧**有实现**（`GridLayoutPolicy`，13B-03 已交付
> 11×2 规则排版），但**未经 ABI 暴露**。宿主只能自己算每个实例的落位再逐个 `translate` ——
> 排版算法在宿主侧重新实现一遍，**两边结果可能不一致**，又是一处「最少改动移植」的破口。
>
> **2026-08-07 受控修订**：H-A-04 仍然依赖 H-A-01，但不得捆绑实施。本次 H-A-01
> 只冻结 `addInstance` / `removeInstance`；`applyGridLayout` 必须在 H-A-04 单独授权和受控修订后引入。

> **H-A-03 是本专项的核心验收**：它证明「宿主不需要知道 scene 内部结构」，
> 也就是证明「打印软件移植只需最少改动」这一原始目标成立。

### H-B 组 · 宿主业务流程 UI（依赖 H-A）

> 验收口径一律**业务级**，不是 API 级。统一硬标准见 §4。

> 🔑 **关键依赖细化（2026-08-07）**：H-B 八卡中**只有两处真正依赖 H-A**。
> 其余六卡操作的是**已存在的场景**，可用既有 fixture scene 开工，**不必等 H-A 授权**。
> 这使得「等 HQ-01/HQ-02 决定」不再阻塞整个 H-B 组。

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-B-01** | 模型导入流程：文件对话框 → `model.import` → `addInstance` → 列表显示 + 导入预检结果展示 | **H-A-03**（硬依赖）| 可导入 OBJ/3MF；预检失败有明确提示；对照主干 `ModelPreflightPanel` 行为一致 | **COMPLETE（2026-08-07）** |
| **H-B-02** | 模型/实例列表与选择：增删、多选、选中联动视图 | **无**（用 fixture scene）；删除实例需 H-A-01 | 对照主干 `ModelListPanel` | **COMPLETE（2026-08-07）** |
| **H-B-03** | 排版与变换 UI 入口：移动/旋转/缩放/镜像 + 精确数值输入（三车道机制 14E-03 已有，本卡只补入口） | **无**（四种变换已在 ABI）；**规则排版部分需 H-A-04** | 对照主干 `ModelTransformPanel`、`SceneLayoutPanel`；**UI-M1 仍须成立** | **COMPLETE（2026-08-07）** |
| **H-B-04** | Profile 选择：宿主提供 Profile 目录与安全等级；模块能力经 ABI 查询，二者求交得到可用 Profile | **HQ-08-A**（已授权） | 对照主干 `ProductionModePanel` + `ScenarioRegistry`；**不得直接读 `slicer_scenarios.json`**；选择不触发 DLL 调用 | **COMPLETE（2026-08-08）** |
| **H-B-05** | 切片参数设置：DPI、层厚、输出目录、材料策略等；含有效配置预览与校验提示。**须体现 `buildVolume` 归属宿主**（`CLD_04`:360）| H-B-04 | 对照主干 `ConfigEditorPanel` / `QuickConfigPanel` / `EffectiveConfigGenerator` | **COMPLETE（2026-08-08）** |
| **H-B-06** | 切片提交与作业管理：提交、进度条、取消、错误展示 | H-B-05 | 对照主干 `ProductionSliceRunSession`；取消 ≤2s 且无 `.staging` 残留 | **COMPLETE（2026-08-08）** |
| **H-B-07** | 结果查看：包校验、摘要、报告、层预览与通道图 | H-B-06 | 对照主干 `PackageLoader` / `LayerPreviewPanel` / `ChannelChartPanel` | **COMPLETE（2026-08-08）** |
| **H-B-08** | 设置持久化与工作区状态：会话恢复、布局记忆 | H-B-05 | 对照主干 `WorkspaceLayoutState` | **COMPLETE（2026-08-08）** |

**不依赖 H-A 的后续链**：

```text
H-B-04..08（已完成）
                    （Profile → 参数 → 切片 → 结果 → 持久化）
H-B-02 · H-B-03（变换部分）                      可并行，用既有 fixture scene
```

这条链覆盖了「选 Profile → 调参 → 切片 → 看结果」，是业务流程的**后半段**；
前半段（导入建场景、规则排版）等 H-A。**两段之间用 fixture scene 衔接，互不阻塞。**

### H-C 组 · 移植交付物

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-C-01** | **逐文件三桶分类定稿**（§1.2 是候选，须逐个核实是否依赖 core 类型）| H-B-07 | 当前 77 个头文件全部归桶；桶 A 给出依赖闭包证据 | **COMPLETE（2026-08-08）** |
| **H-C-02** | 移植指南：桶 B 每个文件的「改走 ABI」改造要点与工作量估算 | H-C-01 | 41 项机器计划、工作包顺序与 38-59 人日建议 | **COMPLETE（2026-08-08）** |
| **H-C-03** | 并排 A/B 对照报告：主干 UI 与宿主模拟对同一模型同一 Profile 语义的行为差异清单 | H-B-07 | 13 条差异覆盖 8 个维度；Debug/Release 对照门禁通过 | **COMPLETE（2026-08-08）** |

### H-D 组 · 视图接线与端到端可操作性（2026-08-08 新增）

> 🔴 **本组的成立原因是一条 A 级代码事实：渲染内核已交付，但没有接到可见画布上。**
>
> `apps/slicer_ui_host_sim/ViewWorkspaceWidget.cpp:13-27` 的两个画布是
> `QLabel` + 静态文字（`"俯视工作区\n1 mm / 10 mm 自适应网格"`、
> `"3D 工作区\n正交 / 透视 · 七向预设"`），**没有任何像素输出**。
>
> `SceneRenderPolicy` / `TopViewRenderPolicy` / `CpuRasterBackend` / `CameraController` /
> `SceneInteractionController` / `TransformCommitPolicy` / `MoveOptimizationPolicy`
> 在整个 app 中的引用位置只有：**自身文件、`CMakeLists.txt`、
> `CapabilityCoverageWorkflow.cpp`（批处理自检）**。
> `HostMainWindow.cpp` 对 `m_workspace` 的调用只有三处：
> `SetMode` / `ShowViewError` / `SetSelectedInstances`。
>
> `REPORT_14E_04d` §5 自己写明了这次延期：
> 「参考宿主的中央画布目前是双视图交互入口；**真实场景载入与渲染策略接线由打印侧集成时
> 按本任务类边界复用**。」
>
> ⛔ **该延期决定作废。** 理由：本专项 §4 的验收口径要求「操作员能独立完成……」，
> 而模型不可见时排版、变换、越界都无法用眼判断；把接线推给打印侧，等于把
> 「参考实现」最难的一段留给移植方，与 Stage 14「最少改动移植」目标直接冲突。
> **接线归切片侧，在 H-D 组内完成。**

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-D-01** | **俯视画布接线**：用 `TopViewRenderPolicy` + `QImage` 输出替换 `topCanvas` 占位 `QLabel`；显示当前 `sceneHandle/sceneRevision` 的真实带纹理俯视 + buildVolume 平台与 1 mm/10 mm 网格 | 无（14E-04 已交付策略）| 导入模型后**画布上能看到模型**；缩放/平移可用；纹理失败显式报错不显示灰模 | **COMPLETE（2026-08-09）** |
| **H-D-02** | **3D 画布接线**：`SceneRenderPolicy` + `CpuRasterBackend` 输出接 `threeDCanvas`；`CameraController` 接鼠标/滚轮事件与七向预设、正交/透视切换控件。<br/>🔴 **本卡内先做 RB-P1**：`SceneRenderPolicy` 改用 `lod=auto`，并按 `DOC_SCHEMA_14` §4 规则 6 处理 `PM-SLICER-VIEWDATA-BUDGET` —— **超预算时显式报错，不静默显示破碎网格** | H-D-01（**R-B-02 前置已解除**）| 3D 可 orbit/pan/光标中心 zoom；**UI-M7 相机操作期跨 DLL 调用恒为 0**（实测计数）；36 个真实模型均为完整 `lod0` 或按冻结纹理合同显式 fail-closed，不出现破碎网格 | **COMPLETE（2026-08-09）** |
| **H-D-03** | **三车道拖拽接线**：`SceneInteractionController` + `TransformCommitPolicy` + `MoveOptimizationPolicy` 接画布拾取与拖拽 | H-D-02 | **UI-M1 拖拽期跨 DLL 调用恒为 0**；松手一次原子 Commit 推进一次 revision；Stale 回滚可演示（UI-M4）| **COMPLETE（2026-08-10）** |
| **H-D-04** | **视图刷新事件接线**：导入 / add / remove / transform / applyGridLayout / `sceneContext` 变更后自动 Refresh；按 `DOC_SCHEMA_14` §6 失效规则，**`worldMatrix` 变化不得使网格缓冲失效** | H-D-01；完整验收需 H-D-02 | 22 实例排版后视图一次刷新到位；`MeshUploadCount` 在纯变换后**增量为 0**（实测）| **PREPARED**（H-D-02 前置已完成）|
| **H-D-05** | **打开切片数据目录**：结果页新增「打开包目录」，`QDesktopServices::openUrl(QUrl::fromLocalFile(...))`；路径**只能取作业返回的 `packageDir`**，不得自行拼接或猜测；目录缺失显式 fail-closed | H-B-07（已完成）| 点击后系统文件管理器定位到包目录；作业失败或路径不存在时按钮禁用并给出原因 | **COMPLETE（2026-08-09）** |
| **H-D-06** | **端到端人工可操作性验收**：按 §4 口径逐步走通七步流程并归档截图；把「显示」维度回填 `REPORT_HOSTFLOW_H_C_03` | H-D-01..05 | 七步全部人工可完成；A/B 对照新增「显示」维度差异条目 | **PREPARED / WAIT H-D-03..04**（H-D-02 前置已完成）|

> 决策记录：`DOC_DECISION_HOSTFLOW_H_D_R1_视图接线归属与14E_04d延期作废.md`

> **H-D-02 验证证据（2026-08-09）**：Debug/Release 的
> `hostflow_hd02_three_d_canvas`、双视图合同和 H-A/H-B 联合门禁通过；
> UI-M7 实测 `cameraCalls=0`。Release 真实资产矩阵覆盖 `model/obj` 全部 36 个 OBJ，
> 结果为 `lod0=22 / budgetRejected=0 / assetRejected=14`。14 个拒绝均为冻结合同要求的
> `PM-SLICER-INPUT-0001/0002` 贴图或材质资产错误，画布清空旧帧并显式提示，不回退灰模或
> 破碎网格。逐资产修复不属于 H-D-02，后续由 RENDER 资产治理任务承接。

> H-D-01 实际门禁：Debug/Release 真实纹理甲片俯视画布与 14E-04d 回归均 PASS；
> 本地 pan/zoom 的 DLL 调用计数为 0；H-A/H-B 联合门禁各 22/22 PASS。
> 证据见 `REPORT_HOSTFLOW_H_D_01_俯视画布接线当前状态.md`。

> H-D-05 实际门禁：Debug/Release 精确作业 `packageDir`、缺失路径 fail-closed、H-B-07
> 结果回归与 H-A/H-B 联合门禁均 PASS。证据见
> `REPORT_HOSTFLOW_H_D_05_生产包目录入口当前状态.md`。

> H-D-02..06 准备审查见
> `DOC_PREP_HOSTFLOW_H_D_02_06_视图后续任务准备审查.md`。R-A-01 已确认现有跳采样为
> 真实资产 P1 风险；R-B-02 未完成前不得把破洞 mesh 接入生产 3D 画布。

> 🔴 **H-D-01 / H-D-05 开工前必看：`HostMainWindow.cpp` 当前 484 行，门禁上限 500 行，只剩 16 行余量（A）。**
> 接线代码**必然放不下**。按既有拆分惯例（`HostMainWindowScene/Job/Result/Profile/Settings/State.cpp`），
> H-D-01 应新建 **`HostMainWindowView.cpp`** 承载视图刷新与画布接线，
> H-D-05 的"打开包目录"槽应放入既有 **`HostMainWindowResult.cpp`**。
> ⛔ **不得把 `apps/slicer_ui_host_sim/` 加入 14B-06 白名单来绕过门禁。**

> ⚠️ **H-D-01 会把一个既有缺陷从「不可见」变成「用户可见」**：
> `src/slicer_core/api/viewdata/SceneViewMeshBuilder.cpp:143-146` 的 LOD 用**均匀跳采样**
> （`stride = (tris + limit - 1) / limit`，`groupIndex += stride`），
> 超过阈值后会产出**满身是洞的网格**。阈值约 13.8k 三角面/实例。
> 因此 `TASKS_RENDER` 的 **R-A-01（实测甲片模型三角面数）必须与 H-D-01 同批执行** ——
> 它决定这个缺陷在真实资产上是否会被触发。

### H-E 组 · 参数深度与导入补齐（✅ **2026-08-08 用户裁决 HQ-09 = 乙，范围已授权，分三批执行**）

> ✅ **HQ-09 已裁决：乙 —— 参考宿主提升到「等价于切片库封装前的切片软件」水位，分批执行。**
> 决策记录：`DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md`
>
> **批次划分（按「用户可感知价值 / 单位成本」降序）：**
>
> ```text
> E1  H-E-01 STL 导入 · H-E-03 支撑参数编辑        ← 立即价值，建议随 H-D 一并推进
> E2  H-E-04 材料工艺 Profile · H-E-05 生产纹理设置  ← 依赖 E1 建立的 Profile 编辑框架
> E3  H-E-02 批量导入 · H-E-06 白区预检接入          ← 便利性与安全网，晚做不返工
> ```
>
> ⚠️ **每批完成后必须复核，不得三批一次性排完。**
> H-E-03 会先建立「宿主 Profile 可编辑段」的结构形态，E2 在其上扩展；
> 若 E1 完成后发现框架需调整，E2 尚未开工则返工成本为零。
>
> ⛔ **H-E 全组不新增 ABI 面**：新增参数一律进入**宿主 Profile**，经既有 `slice.rgbwsv`
> 的有效配置通道下发。**不得改为读取内部 `slicer_scenarios.json` 或 `samples/configs/**`**
> —— 那会推翻 HQ-08-A 已确立的「Profile 目录归宿主」。

> 🔴 **成立原因**：H-D 只解决「看得见」。即使 H-D 全部完成，参考宿主**仍不等于
> 封装前切片软件的操作能力** —— 差的不是显示，是**参数深度与导入广度**。
>
> 证据来自 H-C-02 自己的机器计划 `assets/hostflow_hc02_migration_plan.json`：
> 41 项 B 桶中 `action = adapt_to_host_profile` 的 **8 项**，其 `replacement` 字段
> 指向的宿主功能**当前并不存在**，是计划目标而非既有实现：

| 主干单元 | 计划替代物 | 宿主现状 |
|---|---|---|
| `widgets/SupportEditor.h` | 「host support Profile editor」| ❌ 不存在 |
| `widgets/MaterialProcessProfileEditor.h` | 「host material process Profile editor」| ❌ 不存在 |
| `widgets/MaterialRoleMappingEditor.h` | 「host material role Profile editor」| ❌ 不存在 |
| `widgets/MaterialPolicyEditor.h` | 「host material strategy editor」| ❌ 不存在 |
| `widgets/ProductionTextureSettingsPanel.h` | 「HostSliceSettingsPanel advanced Profile section」| ❌ 不存在 |
| `services/ProductionTextureSettingsContract.h` | 「host Profile schema and editor DTO」| ❌ 不存在 |
| `services/ProductionTextureSettingsModel.h` | 「HostSliceSettings model」| ⚠️ 仅覆盖 DPI/层厚/输出目录/材料策略 |
| `services/SingleMaterialReliefResolver.h` | 「host Profile material strategy」| ❌ 不存在 |

> 核实方式：`HostSliceSettings.h` 中 `support` / `texture` / `支撑` / `纹理` **零命中**。

| 卡号 | 批次 | 任务 | 前置 | 验收 | 状态 |
|---|:--:|---|---|---|---|
| **H-E-01** | E1 | **STL 导入**：文件过滤增加 `.stl`，预检结果展示 | 无 | `src/slicer_core/model.cpp:626` 已支持 `.stl`（A 级）；**纯 UI 缺口，不需改 ABI**；ASCII/binary 两种编码均可导入并切片 | **READY / E1 PREP PASS** |
| **H-E-03** | E1 | **支撑参数编辑**：宿主 Profile 支撑段可编辑；对照主干 `SupportEditor` | 无 | 与 `lower_support` 类 Profile 的实际字段对齐；越界值 fail-closed；**本卡确立「宿主 Profile 可编辑段」的结构形态，E2 在其上扩展** | **READY / E1 PREP PASS** |
| **H-E-04** | E2 | **材料工艺 Profile 编辑**：材料策略 / 角色映射 / 单材料浮雕；对照 `MaterialPolicyEditor`、`MaterialRoleMappingEditor`、`MaterialProcessProfileEditor`、`SingleMaterialReliefResolver` | H-E-03（复用其编辑框架）| 编辑结果进入宿主 Profile 并反映到有效配置自哈希；**不得改用内部 JSON** | **AUTHORIZED / 待 E1 批次门** |
| **H-E-05** | E2 | **生产纹理设置**：对照 `ProductionTextureSettingsPanel` + `Contract` + `Model` | H-E-04 | 纹理应用模式、UV 策略等进入有效 Profile；与 Stage 15 `unprintable_white_*` 字段协同 | **AUTHORIZED / 待 E1 批次门** |
| **H-E-02** | E3 | **批量导入**：多选文件一次导入并逐个 `addInstance`；对照主干 `SceneBatchImportController` | H-E-01 | 单次原子 Commit 或明确的逐项失败语义；**部分失败不得留半个场景** | **AUTHORIZED / 待 E2 批次门** |
| **H-E-06** | E3 | **纹理白区预检接入**：对照主干 `TextureWhitePreflightService`（Stage 15 15C 成果）| H-E-05 | 导入/切片前给出保守告警；结果携带 scene/revision/contentHash，切换场景后旧结果丢弃（Stage 15 坑 7）| **AUTHORIZED / 待 E2 批次门** |

**批次门**（每批完成后复核，不得三批一次排完）：

```text
E1 完成 → 复核实际可用性，再决定 E2 是否照原样执行
E2 完成 → 复核 8 项 adapt_to_host_profile 是否已全部有归宿
E3 完成 → 回填 REPORT_HOSTFLOW_H_C_03 的 known_trim 条目
```

> 📌 **`SceneDocument`（场景/项目保存与加载）：HQ-10 已裁决为【甲】—— 归 PrintApp，参考宿主不实现。**
> 依据：`CLD_04` §4.2 已把持久化与多任务划归 PrintApp；两侧各存一份工程格式必然产生权威之争；
> 该项不影响「一次切片作业能否完成」。
> **`SceneDocument.h` 维持 B 桶 `abi_scene`**（打印侧按 H-C-02 计划自行实现），
> **`ProjectToolsDock.h` 维持 C 桶 `debug_only`**（不移植）。
> ⚠️ 因此参考宿主**重启后场景丢失、须重新导入**是**已裁决的预期行为**，不是缺陷，不得作为 bug 提报。

---

## 4. 业务级验收硬标准

**统一口径（替代 API 级的「能力覆盖」）：**

```text
一个没读过本项目文档的操作员，能在 slicer_ui_host_sim 中独立完成：
  空场景 → 导入模型 → 排版 → 选 Profile → 调参数 → 切片 → 看到结果包并确认校验通过
全程【不需要手写任何 JSON】，【不需要命令行】，【不需要查阅源码】。
```

三条附加约束：

```text
① 宿主全程只经 11 个公开导出，不得 include slicer_core（CI 守卫已在）
② 宿主不得构造 scene JSON（H-A-03 的直接体现）
③ 拖拽与相机操作期跨 DLL 调用恒为 0（UI-M1 / UI-M7 不得因新增流程而破坏）
```

---

## 5. 优先级框架：按「晚做的返工成本」排序

> 用户问「14F 完成后的优先级」。**这里需要一处修正**：见 §5.2。

### 5.1 全量待办与返工成本评估（P）

| 工作项 | 晚做的后果 | 返工成本 |
|---|---|---|
| **H-A · ABI 场景生命周期** | 打印侧先按「拼 scene JSON」接入，之后全部要改；Stage 14 核心目标无法验证 | **极高** |
| **H-B · 业务流程 UI** | 打印侧无参考，自行摸索，交互与语义可能与切片侧不一致 | **高** |
| R-A-01 · 实测甲片三角面数 | 无（但决定 R-B 优先级）| 无（成本极低，随时可做）|
| R-B · LOD 跳采样修复 | 3D 视角上线后用户看到破碎模型 | 中（缺陷修复，不返工）|
| 03E 第二步 · 默认开启压缩 | golden 越积越多，重固化越贵 | 中（**随时间递增**）|
| Qt6 升级 | 安全更新缺失持续 | 中 |
| R-C · 屏幕空间 LOD 提示 | LOD 选择低效，但不出错 | 低 |
| Stage 16 · 几何采样与性能 | 性能未优化 | 无 |
| 12F-02..09 · 性能算法 | 同上 | 无 |
| `unexpected_overlap_pixels` 缺陷卡 | 护栏继续空转 | 无 |

### 5.2 ⚠️ H-A 应当【立刻】做，理由是回签批次而非 14F 时序

原推理是「H-A 是 14F 联调的输入」。核对 `REPORT_14` v3.58 后需修正 —— 实际时机论据**更强**：

```text
当前状态（A）
  EXTERNAL_EVIDENCE_GATE = DEFERRED_BY_USER   外部验证已延期，打印侧尚未开始接入
  14F_INTERFACE_STATE    = FROZEN             接口已冻结
  14A_EXTERNAL_ACK       = PENDING            14A-03 与 14A-04-R1 【正等打印侧回签】
```

**关键：现在有一个已经打开的回签批次。**

| H-A 落地时机 | 协调成本 |
|---|---|
| **现在**（与 14A-03 / 14A-04-R1 同批）| ✅ **零额外成本** —— 并入同一次回签请求 |
| 14F 之后 | ❌ **第三次独立修订**，需再发起一轮回签往返 |

打印侧尚未开始接入，所以「联调建立在错误接入方式上」的风险已被延期消解；
但**「多花一轮回签」的成本是实打实的**，且会随时间只增不减。

三条补充理由：

```text
① 接口已 FROZEN —— 修订只会越来越贵（14A-04-R1 已是一次受控修订的先例）
② 14E-04b 已 COMPLETE（能力覆盖通过）而业务流程缺失，正说明「能力齐 ≠ 目标达成」；
   H-A-03 的空场景闭环才是「最少改动移植」的真正证据
③ H-A 不触碰 14C/14D/14E 的文件（只改 contracts + SceneFacade + SceneCapabilityAdapter），
   与 14F-03 并行的冲突风险低
```

**历史建议**：在尚未冻结接口时，H-A-01/02/03 适合与 14F-03 并行。
**当前决策**：2026-08-07 用户已授权 HQ-01，并通过
`DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md` 启动受控修订。H-A-01、H-A-02、
H-A-03、H-A-04 已完成；HQ-07 已关闭。DTO v1.7、权威 scene 透传和空场景生产闭环均已通过
Debug/Release 门禁，H-A 全组收口。

### 5.3 建议排序

> ⚠️ **2026-08-08 再次更正**：H-A/H-B/H-C 已全部完成，本表原 P0/P1/P2 均已消耗。
> 新的 P0 是 **H-D 组视图接线** —— 当前唯一阻止「操作员能独立走通七步流程」的缺口。
>
> **为什么 H-D-01 单独排最前**：投入最小、可见收益最大。策略类已交付且有门禁，
> 接线本身只是把 `QImage` 输出接到画布并处理 resize 与刷新时机；
> 做完这一张，用户第一次能在新宿主里**看见自己导入的模型**，
> 后续所有显示类问题才有了可观察的载体。

### 5.3.1 并行性判定（H-D-01 · R-A-01 · H-D-05）

**结论：R-A-01 可完全并行；H-D-01 与 H-D-05 建议串行，不建议真并行。**

| 卡 | 触碰的文件 | 冲突 |
|---|---|:--:|
| **R-A-01** | 无源码改动（只统计三角面数、出数字）| **零** |
| **H-D-01** | `ViewWorkspaceWidget.h/.cpp`、新建 `HostMainWindowView.cpp`、`HostMainWindow.h` | ⚠️ |
| **H-D-05** | `HostPackageReviewPanel.h/.cpp`（283 行）、`HostMainWindowResult.cpp`、`HostMainWindow.h` | ⚠️ |

```text
H-D-01 与 H-D-05 的实现文件【完全不重叠】，
但两者都要在 HostMainWindow.h 增加成员/槽声明 —— 这是唯一的共享写入点。
```

**判定（P）**：

```text
✅ R-A-01 与任意卡同批     零源码改动；且它的结果决定 H-D-01 要不要带 LOD 兜底
                          → 建议【先跑 R-A-01】，出数字后再开 H-D-01

⚠️ H-D-01 与 H-D-05 真并行  两个分支同时改 HostMainWindow.h 会产生合并冲突；
                          更关键的是会破坏「每卡独立 Debug/Release 门禁 + 单独提交」
                          的证据链 —— 门禁绿了也分不清是哪张卡的功劳

✅ 同一会话顺序执行         无技术障碍。推荐：R-A-01 → H-D-01 → H-D-05
                          （H-D-05 独立于视图，放后可避免与 H-D-01 的头文件改动交织）
```

> **一句话**：三张卡可以**一次性授权**，但应**顺序执行**。
> 「同步开启」若指「现在把三张都批下去」→ 可以；若指「三张同时动代码」→ 不建议。
>
> ⚠️ **2026-08-07 更正**：本表原以「14F-03 → 14F-04 → 14F-05」为 P0 主线，**该前提已失效**。
> 核对 `TASKS_14` :376-380 与 `REPORT_14` v3.61：**14F-01..05 全部 `SLICER-SIDE COMPLETE`**
> （外部验收延期），`CURRENT_NEXT_TASK = NONE`。
> **Stage 14 切片侧已无待办主线** —— 下一步做什么需要用户选定。

```text
P0 · 视图接线（唯一阻塞项，不改契约、不需新授权，只需用户点名启动）
     H-D-01                            俯视画布接线   ← 最先做，一张卡就能让模型可见
     R-A-01                            实测甲片三角面数（与 H-D-01 同批，成本极低）
     H-D-05                            打开包目录（独立可并行，半天量级）

P1 · 交互接线
     H-D-02                            3D 画布 + 相机接线
     H-D-04                            视图刷新事件接线
     R-B-01/02                         LOD 跳采样修复（若 R-A-01 判为会触发）

P2 · 收口
     H-D-03                            三车道拖拽接线
     H-D-06                            端到端人工可操作性验收 + 回填 H-C-03「显示」维度

P1.5 · 【HQ-09 已授权】E1 批 —— 可与 H-D 交替推进
     H-E-01                            STL 导入（纯 UI 缺口，成本极低）
     H-E-03                            支撑参数编辑（确立宿主 Profile 编辑框架）
     ── E1 批次门：复核实际可用性 ──

P2.5 · E2 批（过 E1 批次门后）
     H-E-04                            材料工艺 Profile 编辑
     H-E-05                            生产纹理设置
     ── E2 批次门：复核 8 项 adapt_to_host_profile 是否全部有归宿 ──

P3.0 · E3 批（过 E2 批次门后）
     H-E-02                            批量导入
     H-E-06                            纹理白区预检接入
     R-C-01/02                         屏幕空间 LOD 提示
     R-D-01                            Qt6 升级可行性评估（长期版本 Qt 6.8+）

P3 · 独立排期
     Stage 16                          几何采样 / 接触姿态 / 性能
     03E 第二步                        默认压缩（建议与 LibTIFF 默认后端、tiff_io 字对齐缺陷合并为一次决策）
     R-D-02/03/04                      渲染后端实施

P4 · 无返工压力，可最后做
     12F-02..09                        性能算法
     unexpected_overlap 缺陷卡          护栏修复（需先定义「合法共写」规则）
```

> **不建议把 H-B 排到 P2 之后。** 打印侧一旦开始移植而手上没有可运行的业务流程参考，
> 就会自行设计交互与语义，之后与切片侧对不齐的返工成本远高于现在把 H-B 做完。

### 5.4 一句话结论

**Stage 14 切片侧已全部收口，当前无主线任务。**
建议先评审 H-A 的受控合同修订（时机窗口见 §5.2），通过后再启动 H-B 业务流程 UI；
**若不愿在此时动契约，则直接启动 H-B** —— H-B 不改任何契约，无需额外授权，
只是届时宿主仍需自拼 scene JSON，「最少改动移植」这一条留待 H-A 落地后才能成立。

### 5.5 H-A 与 H-B 的先后取舍（需用户定）

| 顺序 | 优点 | 代价 |
|---|---|---|
| **H-A → H-B**（推荐）| 业务流程一次写对；「最少改动移植」可被 H-A-03 验证 | 需授权修订已冻结契约；若打印侧已回签则协调成本上升 |
| **H-B → H-A** | 不动契约，立即可开工；先看到可用界面 | H-B 的导入流程要先按「自拼 scene JSON」写一遍，H-A 落地后**该部分需重写** |

> 关键判据是**打印侧回签时机**（只有用户知道）：
> 回签尚远 → 选 `H-A → H-B`；回签在即或已发生 → H-A 成本陡增，可考虑 `H-B → H-A` 并接受返工。

---

## 6. 与其它专项的边界

| 专项 | 关系 |
|---|---|
| Stage 14（14A–14F）| H-A 修的是 Stage 14 冻结契约的一处缺口，属受控 minor 修订；H-B/H-C 完全独立 |
| `TASKS_RENDER`（R 组）| 无依赖。H-B 的视图部分复用 14E 已实现的 `ViewWorkspaceWidget` 与 `IRenderBackend` |
| Stage 16 | 无依赖，可完全并行 |
| 14E | 本专项是 14E 的**功能补齐**，不推翻 14E 已交付的机制（ModuleClient、三车道、渲染、能力覆盖全部复用）|

## 6.5 🔴 待用户确认的开放项（开工前必须澄清）

| 编号 | 待确认 | 为什么必须由用户定 |
|---|---|---|
| ~~HQ-01~~ | ~~H-A 是否授权修订已冻结契约~~ → ✅ **已授权：2026-08-07；H-A-01 受控修订已执行** | 权威记录：`DOC_DECISION_14F_R1_HOSTFLOW场景生命周期合同受控修订.md` |
| **HQ-02** | **打印侧回签的预计时机** | 直接决定 H-A 的协调成本，也决定 §5.5 的先后取舍。**只有用户知道** |
| ~~HQ-03~~ | ~~H-B 范围边界~~ → ✅ **已定：选项甲，只覆盖核心作业流程**（用户 2026-08-07）| — |
| ~~HQ-04~~ | ~~权威来源歧义~~ → ✅ **已定：切片侧逻辑优先**；差集比对已完成，见 §6.6 | — |
| HQ-05 | R-A-01 甲片三角面数（需实测，非决策）| 决定 LOD 缺陷是 P1 还是 P2 |
| HQ-06 | 渲染四项待决 RD-A/B/C/D | 见 `TASKS_RENDER` §9 |
| **HQ-07** | **隐式场景如何取得宿主权威的 `resolvedProfileId` 与 `buildVolume`** | H-A-02 代码审计确认 DTO v1.5 缺少生产所需场景上下文；建议采用 `sceneContext` 并受控升至 v1.6，见 `DOC_DECISION_14F_R2_HOSTFLOW隐式场景初始化上下文受控修订.md` |
| **HQ-08** | **Profile 目录由宿主提供，还是随模块 ABI 自述** | H-B-04 准备审计确认 `pm_module_info` 无 Profile 目录且禁止内部 JSON；推荐宿主目录 + ABI 能力求交，见 `DOC_DECISION_HOSTFLOW_H_B_04_R1_Profile发现协议缺口.md` |
| ~~HQ-09~~ | ~~参考宿主的目标水位~~ → ✅ **已裁决：乙（等价于封装前切片软件），分 E1/E2/E3 三批** | 权威记录：`DOC_DECISION_HOSTFLOW_H_E_R1_参考宿主目标水位裁决.md`（2026-08-08） |
| ~~HQ-10~~ | ~~场景/项目保存与加载归谁~~ → ✅ **已裁决：甲（归 PrintApp，参考宿主不实现）** | 同上；`SceneDocument` 维持 B 桶、`ProjectToolsDock` 维持 C 桶，分类不变 |

## 6.6 ✅ HQ-03 / HQ-04 已闭合（2026-08-07）

### 6.6.1 用户裁定

```text
① 切片软件的逻辑【就是】打印软件中切片模块的逻辑
② 两者定义有区别时，【优先采用切片软件的需求与逻辑】
③ H-B 范围：【只覆盖核心作业流程】（选项甲，当前 8 张卡即为此范围）
```

据此，主干 68 文件中的**切片专有诊断类明确排除**在 H-B 之外：
`DiagnosticsDock`、`OpenVdbUtilityReportInterpreter`、`ConfigDiffPanel/Model`、
`UiSmokeTestRunner`、`DiagnosticSemanticPreviewPanel`、`SliceTimingPanel`、
`ContextInspector`、`ProjectToolsDock`、`DiagnosticSettingsPanel`。

### 6.6.2 与打印侧的差集比对结果（A · 已读 `CLD_04`）

打印侧 `CLD_04` §4.2 声明能力包应提供 **8 项**能力。逐项对照我方冻结的 15 项：

| 打印侧 `CLD_04` 要求 | 切片侧冻结能力 | 结论 |
|---|---|:--:|
| `model.import` | `model.import` / `get_metadata` / `release` | ✅ 覆盖（更细粒度）|
| `geometry.preflight` | `geometry.preflight(fast)` / `(full)` | ✅ |
| `geometry.repair` | `geometry.repair` | ✅ |
| `scene.transform` | `scene.apply_operation`（4 种变换）| ✅ |
| **`scene.layout`**（行列+间距+碰撞）| `scene.apply_operation/applyGridLayout` | ✅ H-A-04 已补齐 |
| `scene.viewdata` | `scene.get_viewdata` | ✅ |
| `slice.rgbwsv` | `slice.rgbwsv` | ✅ |
| `package.verify` | `package.verify` / `get_summary` / `get_layer_descriptor` / `render_layer_preview` / `read_report` | ✅ 覆盖（更细）|

**切片侧另有 `scene.get_snapshot` 与独立的 `geometry.collision`**（`CLD_04` 把碰撞并入
`scene.layout`）—— 属更细粒度拆分，不冲突。

**差集结论：H-A-04 完成后，8 项要求已全部具备切片侧能力映射；打印侧外部 ACK 仍延期。**

### 6.6.3 🎯 一个消解风险的重要发现

我此前担心「打印侧可能需要切片侧没有的功能（作业队列、多设备、多文档）」。
**`CLD_04` §4.2「能力包不提供（7 项）」已明确把它们划给 PrintApp**：

```text
设备 Profile / buildVolume / 原点 / 轴向     → PrintApp（宿主）
作业队列 / 多任务 / 持久化                   → PrintApp
渲染 / 拾取 / 相机 / gizmo 手感              → PrintApp UI
交互临时状态                                 → PrintApp UI
RIP / 通道化 / Qt 类型                       → RIP 模块 / ChannelSplitter / —
```

**所以宿主模拟不需要实现作业队列、多设备与持久化** —— 它们本就不属于切片能力包。
这条把 HQ-04 的主要风险直接消解了。

### 6.6.4 打印侧 UI 分区与 H-B 八卡的映射（A · `CLD_04` :291-296）

| 打印侧 UI 分区 | 对应 H-B 卡 |
|---|---|
| 项目与模型：模型列表、导入、实例管理 | H-B-01 / H-B-02 |
| 参数：切片 / RIP / 材料 Profile 选择与摘要 | H-B-04 / H-B-05 |
| 预览：模型-构建空间 / 切片层 / 通道 三模式 | 14E 视图工作区（已有）+ H-B-07 |
| 主操作：导入 / 排版 / 开始处理 / 取消 / **加载到打印** | H-B-01 / H-B-03（+H-A-04）/ H-B-06 |

**唯一打印侧独有项是「加载到打印」** —— 属打印软件动作，宿主模拟不实现。

> 结论：**H-B 现有八卡与打印侧 UI 分区高度吻合，无需扩充范围**；
> 真正需要补的是能力侧的 `scene.layout`（H-A-04），而非 UI 侧。

### ~~HQ-03~~ · H-B 范围边界的歧义（已闭合，原文保留备查）

主干 `slicer_debug_ui` 的 68 个文件里，有相当一部分是**切片专有的诊断能力**
（`DiagnosticsDock`、`OpenVdbUtilityReportInterpreter`、`ConfigDiffPanel`、
`UiSmokeTestRunner`、`DiagnosticSemanticPreviewPanel` 等）。

```text
选项甲   只覆盖【核心作业流程】—— H-B-01..08 当前的 8 张卡即为此范围
选项乙   对齐主干【全部功能】—— 需再增约 6–10 张卡，工作量翻倍
```

本清单当前按**选项甲**编写，但**未在卡面明确排除诊断类**。这是一处需要拍板的边界。

### ~~HQ-04~~ · 「打印软件的所有模拟逻辑」应以哪一份为准（✅ 已闭合，原文备查；结论见 §6.6）

用户要求「满足打印软件中关于切片模块的所有模拟逻辑与功能」。这里有两个**可能不一致**的来源：

| 来源 | 内容 | 风险 |
|---|---|---|
| 切片侧主干 `slicer_debug_ui` | 68 文件，含大量切片专有诊断 | 打印侧**不需要**其中一部分 |
| 打印侧 `ry_print_demo/docs/claude/INTEGRATION/CLD_04,05,06,10,27` + `PLANNING/CLD_07` | 打印软件对切片模块的调用需求 | 可能包含切片侧主干**没有**的项（如作业队列、多设备、多文档）|

**本清单目前只对照了切片侧主干，未核对打印侧 CLD 文档。**
若「所有模拟逻辑」以打印侧为准，需先做一次双向差集比对（建议新增卡 **H-B-00**）。

> ⚠️ 这是本专项最可能导致范围误判的一处。建议在 H-B 开工前先关闭 HQ-04。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-10 | v3.7 | 完成 H-D-03：俯视画布支持本地实例拾取与拖拽，pointer-move 仅更新缓存矩阵和 QImage，实测 12 次移动为 0 次 DLL 调用；松手只执行一次 `scene.apply_operation`，revision 恰好 `+1` 且正常路径不追加 snapshot。Stale 路径沿用 UI-M4 权威快照恢复并丢弃瞬态帧。Debug/Release H-D-01/02/03、14E-03/04/04d、边界和源码尺寸联合门禁各 8/8 PASS。 |
| 2026-08-09 | v3.6 | 完成 H-D-02 与优先 RB-P1：参考宿主接入真实纹理 3D 画布、七向预设、正交/透视、orbit/pan/光标中心 zoom；相机操作保持 0 次 DLL 调用。`SceneRenderPolicy` 改用 `lod=auto`，预算失败显式 `PM-SLICER-VIEWDATA-BUDGET` 且清空旧帧。Release 36 资产矩阵为 22 个完整 lod0、14 个冻结纹理合同显式拒绝、0 个破碎降级；H-D-03/04 前置解除。 |
| 2026-08-09 | v3.5 | **解除 H-D-02 的 R-B 硬阻塞**。复核发现暂停理由（破洞模型）可由更便宜的手段消除：`SceneRenderPolicy.cpp:46` 把 `lod` **硬编码为 `lod2`**（阈值恒为 10,000，与 128 MiB 预算无关），改为 `auto` 即为一行改动；`SceneViewCandidateBuilder.cpp:194` 逐实例重建 mesh、外观有缓存而网格没有，违反 `DOC_SCHEMA_14` §3。真实触发面为 **35/36**（非报告的 17/36）。RB-P1 并入 H-D-02，H-D-03/04/06 阻塞同步解除；RD-B 推迟至 R-A-02 重测后裁决。分析见 `DOC_ANALYSIS_RENDER_RD_B_前置复核_预算膨胀三处根因.md`。 |
| 2026-08-09 | v3.4 | 完成 H-D-02..06 与 H-E E1 准备审查：H-D-02 因 R-A-01 证实的 LOD P1 风险转为 `BLOCKED BY R-B DECISION`，H-D-03/06 同步阻断，H-D-04 冻结刷新与缓存矩阵；H-E-01/03 的 STL 编码、支撑 Profile 字段、fail-closed 和 Debug/Release 门禁准备通过。 |
| 2026-08-09 | v3.3 | 完成 H-D-05：结果页只使用作业返回并经严格校验的 `packageDir` 打开生产包目录；空路径、目录缺失、包校验失败和身份不一致均 fail-closed。Debug/Release H-D-05、H-B-07 与 H-A/H-B 联合门禁通过。 |
| 2026-08-09 | v3.2 | 完成 H-D-01：参考宿主俯视占位控件替换为真实 ViewData/QImage 画布，导入后按权威 scene identity 刷新；本地缩放/平移保持零 DLL 调用，失败清空旧帧并显式报错。Debug/Release H-D-01 与 14E-04d 回归、H-A/H-B 22 项联合门禁、宿主边界和源码尺寸守卫全部通过。同批 R-A-01 确认 17/36 模型超过 13.8k 阈值，H-D-02 前须先裁决 LOD 修复。 |
| 2026-08-08 | v3.1 | **用户裁决 HQ-09 = 乙（等价水位，分 E1/E2/E3 三批）、HQ-10 = 甲（场景保存归 PrintApp）**；新增 `DOC_DECISION_HOSTFLOW_H_E_R1`。H-E 六卡状态由 PROPOSED 转 AUTHORIZED 并按批次重排（E1: H-E-01/03 → 批次门 → E2: H-E-04/05 → 批次门 → E3: H-E-02/06）；H-E-04 前置改为 H-E-03（复用其 Profile 编辑框架）。新增 §5.3.1 并行性判定：R-A-01 零冲突可同批，H-D-01 与 H-D-05 实现文件不重叠但共享 `HostMainWindow.h`，建议顺序执行。新增行数门禁警示：`HostMainWindow.cpp` 已 484/500 行，H-D-01 须新建 `HostMainWindowView.cpp`，不得进白名单。 |
| 2026-08-08 | v3.0 | **第二层复核：H-D 完成也不等于封装前切片软件**。依据 `hostflow_hc02_migration_plan.json` 的 8 项 `adapt_to_host_profile`，其 `replacement`（host support Profile editor、host material process Profile editor、HostSliceSettingsPanel advanced Profile section 等）**当前均不存在**；`HostSliceSettings.h` 中 support/texture 零命中。另发现 STL 已由 `model.cpp:626` 支持但宿主未提供入口，批量导入缺失。新增 **H-E 组 6 卡**（状态 PROPOSED，超出 HQ-03 核心流程裁决）与 **HQ-09（目标水位）/ HQ-10（场景保存归属）**；补 `DOC_DECISION_HOSTFLOW_H_D_R1` 决策记录；同步 `REPORT_HOSTFLOW_HX`、`DOC_INDEX`、`CODEX_PROMPT_HOSTFLOW`。 |
| 2026-08-08 | v2.9 | **复核发现视图显示链路未接线**：`ViewWorkspaceWidget` 两个画布仍是占位 `QLabel`；`SceneRenderPolicy`/`TopViewRenderPolicy`/`CpuRasterBackend`/`CameraController`/`SceneInteractionController`/`TransformCommitPolicy`/`MoveOptimizationPolicy` 在 app 内只被 `CapabilityCoverageWorkflow` 引用；`HostMainWindow` 对 `m_workspace` 仅调用 `SetMode`/`ShowViewError`/`SetSelectedInstances`。另发现结果页无「打开包目录」入口，`H-C-03` A/B 对照缺「显示」维度。作废 `REPORT_14E_04d` §5 把接线推给打印侧的延期决定，新增 **H-D 组 6 张卡**，文档状态从 LOCAL COMPLETE 回退为 ACTIVE，§5.3 优先级改以 H-D-01 为 P0。 |
| 2026-08-08 | v2.8 | 完成 H-C-03：冻结同一模型与等价 Profile 语义的 A/B 口径，建立 13 条/8 维机器差异矩阵、门禁和双轨运行脚本；Debug/Release 主干 5 个 smoke、参考宿主 6 个 UI self-test、H-A/H-B CTest 各 18/18 PASS。HOSTFLOW 本地交付完成，打印侧 ACK 继续延期。 |
| 2026-08-08 | v2.7 | 完成 H-C-02：41 个 B 桶单元逐项映射 action、公开替代实现、工作流和复杂度；按 scene=15、profile=14、job=2、package=10 形成 38-59 人日工作包建议。机器门禁与 H-C-01 B 桶双向集合闭合，未新增 ABI 或改动主干 UI。 |
| 2026-08-08 | v2.6 | 完成 H-C-01：把历史 68 个范围校正为当前 77 个头文件，并以 `hostflow.migration_inventory.1` 唯一归类为 A=6、B=41、C=30；新增机器门禁验证全覆盖、理由码、路径和 A 桶无 core/非 A 本地依赖。H-C-02 前置已解除，H-C-03 可独立准备。 |
| 2026-08-08 | v2.5 | 完成 H-B-08：参考宿主使用 `hostflow.workspace.1` 持久化窗口、分栏、业务页、Profile、DPI、层厚、输出目录、实体材料与 buildVolume；运行时 scene/job/model/cache 身份不持久化，旧 schema 与非法状态安全回退，自检进程不读写真实用户设置。Debug/Release H-A/H-B 联合门禁各 18/18、宿主边界各 4/4、源码尺寸守卫 PASS。H-B 核心业务流程全部完成，下一组为 H-C 移植交付。 |
| 2026-08-08 | v2.4 | 完成 H-B-07：参考宿主仅经冻结 `package.*` 能力闭合生产包校验、实例/Profile 摘要、逐层 descriptor、生产 TIFF 预览、命名报告与 RGBWSV 六通道图；生产 Writer 补齐真实 `perInstance/profileEcho`，旧包仍 fail-closed。Debug H-B 联合门禁 16/16、Release 含 package query 联合门禁 18/18、Debug/Release 宿主边界各 4/4 PASS。下一卡为 H-B-08。 |
| 2026-08-08 | v2.3 | 完成 H-B-06：参考宿主经公开 SPI 提交真实 `slice.rgbwsv` Worker 作业，非阻塞轮询进度、协作取消、终态释放、包身份闭合和错误详情展示；取消 ≤2s 且不残留临时目录。Debug/Release 宿主联合门禁各 13/13、Worker 合同与取消门禁各 6/6 PASS。下一卡为 H-B-07。 |
| 2026-08-08 | v2.2 | 完成 H-B-05：参考宿主新增 635/600 DPI、0.038 mm 层厚、输出目录、RGB/W/V 实体材料和宿主设备 buildVolume 编辑；有效 Profile 自哈希预览与场景 Profile/buildVolume 绑定 fail-closed。Debug/Release H-B-01..05 各 8/8、模块边界各 3/3、主干设置 A/B smoke 通过。下一卡为 H-B-06。 |
| 2026-08-08 | v2.1 | 用户授权 HQ-08-A；完成 H-B-04 宿主 Profile 目录、`pm_module_info` 能力结构化求交、生产安全等级与缺失能力 UI。Profile 选择仅更新宿主草稿且零 DLL 调用；Debug/Release H-B-01..04 各 6/6、模块边界各 3/3、主干 A/B smoke PASS。下一卡为 H-B-05。 |
| 2026-08-07 | v2.0 | H-B-04 准备审计完成：确认冻结 `pm_module_info`/15 项能力没有 Profile 目录，主干内部 JSON 与 `slicer_core` 目录均不可移植；新增 HQ-08，推荐“宿主 Profile 目录 + ABI 模块能力求交”。H-B-04 标记为 PREP COMPLETE / BLOCKED，H-B-05..08 不得越过该 Gate。 |
| 2026-08-07 | v1.9 | 完成 H-B-03：参考宿主增加多选实例精确移动/旋转/缩放/镜像及 11×2 规则排版入口；本地编辑零 DLL 调用，变换与排版各以单次原子 Commit 推进一次 revision。Debug/Release 各 8/8 联合门禁通过，主干三组 A/B smoke PASS，下一卡为 H-B-04。 |
| 2026-08-07 | v1.8 | 完成 H-B-02：参考宿主增加扩展选择、全选、添加/原子删除入口和中央工作区选择联动；删除经单次 `removeInstance[]` Commit，未知实例 fail-closed。Debug/Release 各 6/6 联合门禁通过，主干 `multi-model-list` 源码构建 smoke PASS，下一卡为 H-B-03。 |
| 2026-08-07 | v1.7 | 完成 H-B-01：参考宿主经公开 SPI 完成 OBJ/3MF 导入、`addInstance`、快速预检、模型列表与问题展示；缺失/不支持文件显式 fail-closed。Debug/Release 各 5/5 联合门禁通过，下一卡为 H-B-02。 |
| 2026-08-07 | v1.6 | 完成 H-A-03：纯 C/Qt 宿主均从空场景经 11 导出完成 import、隐式 add、规则排版、transform、slice 与 package verify；移除宿主手工 scene builder；权威 snapshot scene 改为不透明透传；修正源路径身份与 signed-zero hash 稳定性。Debug/Release 各 5/5 联合回归及合同/边界门禁通过，H-A 全组收口，H-B-01 解锁。 |
| 2026-08-07 | v1.5 | 完成 H-A-04：DTO 升至 v1.7，在既有 `scene.apply_operation` 增加单操作 `applyGridLayout`，复用 `GridLayoutPolicy` 完成 11×2/22 实例规则排版；容量、参数、混批、replay 与原子失败负例通过 Debug/Release 门禁。下一卡为 H-A-03。 |
| 2026-08-07 | v1.4 | 用户授权 HQ-07 及 H-A 后续开发；DTO 升至 v1.6 并完成 H-A-02：隐式场景取得宿主权威 Profile/buildVolume，add/remove、稳定实例 id、原子批处理、operationId replay 与负例 fail-closed 已通过 Debug/Release 门禁。H-A-04 标记为已授权且准备就绪，但继续保持独立原子卡。 |
| 2026-08-07 | v1.3 | H-A-02 准备审计发现 DTO v1.5 的隐式创建缺少宿主权威 Profile/buildVolume，新增 HQ-07 与 H-A-02 准备文档；在用户接受 14F-R2 `sceneContext` 受控修订前，H-A-02 标记为准备已审查但禁止编码，避免写死模块默认值形成伪闭环。 |
| 2026-08-07 | v1.2 | 用户明确授权 HQ-01；新增 14F-R1 受控修订决策；完成 H-A-01 的 DTO v1.5、`addInstance`/`removeInstance` 条件字段、兼容型隐式建场景和合同门禁；纠正“`pm_release` 关闭场景”的类型错误，scene session 改为绑定 `pm_module_t` 并由 `pm_destroy` 清理；H-A-02 转为下一候选但未自动执行。 |
| 2026-08-07 | v1.1 | 用户裁定 HQ-03（只覆盖核心流程）与 HQ-04（切片侧逻辑优先）；**已读打印侧 `CLD_04` 完成差集比对**（§6.6）：8 项要求 7 项已覆盖，**唯一缺口 `scene.layout` → 新增 H-A-04**；确认 `CLD_04` §4.2 已把作业队列/多任务/持久化/设备 Profile 划归 PrintApp，**HQ-04 主要风险消解**；打印侧 UI 分区与 H-B 八卡完成映射，确认无需扩充范围；明确排除 9 个切片专有诊断类文件；修正 §5.3（14F-01..05 已全部 SLICER-SIDE COMPLETE，当前无主线）与 §5.5（H-A/H-B 先后取舍） |
| 2026-08-06 | v1.0 | 首版。定位 14E 交付「技术验证壳 ≠ 业务流程」的根因（卡片全为能力导向）；确认**主干 `slicer_debug_ui` 未被 Stage 14 改动、功能规格就在 HEAD**，68 个头文件按业务流程分组并给出三桶分类候选；给出 ABI 场景生命周期缺口的 A 级证据与**扩展 operations 枚举**的采纳方案（不新增能力/导出，minor 兼容）；分 H-A/H-B/H-C 三组共 14 张卡；确立业务级验收硬标准；**按「晚做的返工成本」建立 P0–P4 优先级框架，并修正「H-A 不能等到 14F 之后」** |
| 2026-08-07 | v1.1 | 对齐用户最新授权：Stage 14F 外部验证延期、对外接口冻结并继续本地收口；H-A 缺口分析保留，但改为 14F 后受控修订候选，禁止在当前 14F 中直接改变冻结合同 |
