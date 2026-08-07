# TASKS_HOSTFLOW 宿主业务流程与场景生命周期补齐任务清单

> 文档状态：**PROPOSED / NOT ACTIVE**（待用户授权）
> 版本：v1.1 ｜ 日期：2026-08-06 ｜ 冻结口径更新：2026-08-07
> **定位：独立补充专项，不属于 Stage 14 任何任务组，不占阶段编号。**
> 起因：14E 交付的 `slicer_ui_host_sim` 是「技术验证壳」，未实现原设想的完整业务流程
> 上游：`DOC_DECISION_14_UI_宿主模拟改造专项.md`、`contracts/slicer_capability_dtos.json`
> 证据等级：A=已核实代码事实，B=目标设计，P=判断
>
> **2026-08-07 排期覆盖说明**：用户已授权 Stage 14F 暂缓打印侧验证并立即冻结
> Stage 14 对外接口。本文 H-A 仍是有效缺口分析，但不得在 14F 收口前修改冻结合同；
> 后续启动 H-A 必须新建受控修订并再次取得用户授权。

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

**场景创建隐含化**：请求不带 `sceneHandle` 且 `scene` 为空时，模块创建新空场景并在响应中返回 `sceneHandle`。
**场景关闭**复用 `pm_release`（句柄生命周期已有机制），无需新能力。

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
| **H-A-01** | 扩展 `operations[].type` 枚举与 `addInstance`/`removeInstance` 字段；`sceneHandle` 语义明确化（缺省即创建）；契约文档与 Schema 同步 | — | 契约测试通过；**`PM_SPI_VERSION`、11 导出、15 能力全部不变**；旧请求（不含新枚举）行为逐字节不变 | PROPOSED |
| **H-A-02** | `SceneFacade` 与 `SceneCapabilityAdapter` 实现新操作；实例 id 分配、重复添加、删除不存在实例的负例 fail-closed | H-A-01 | Debug/Release 单测；`operationId` 幂等对新操作同样成立 | PROPOSED |
| **H-A-03** | **端到端最小闭环**：从**空场景**开始，仅经 11 个导出完成 `import → addInstance → transform → slice → verify` | H-A-02 | 宿主**不构造任何 scene JSON**；`slicer_host_sim`（纯 C）与 `slicer_ui_host_sim` 各跑通一次 | PROPOSED |

> **H-A-03 是本专项的核心验收**：它证明「宿主不需要知道 scene 内部结构」，
> 也就是证明「打印软件移植只需最少改动」这一原始目标成立。

### H-B 组 · 宿主业务流程 UI（依赖 H-A）

> 验收口径一律**业务级**，不是 API 级。统一硬标准见 §4。

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-B-01** | 模型导入流程：文件对话框 → `model.import` → `addInstance` → 列表显示 + 导入预检结果展示 | H-A-03 | 可导入 OBJ/3MF；预检失败有明确提示；对照主干 `ModelPreflightPanel` 行为一致 | PROPOSED |
| **H-B-02** | 模型/实例列表与选择：增删、多选、选中联动视图 | H-B-01 | 对照主干 `ModelListPanel` | PROPOSED |
| **H-B-03** | 排版与变换 UI 入口：移动/旋转/缩放/镜像 + 精确数值输入（三车道机制 14E-03 已有，本卡只补入口） | H-B-02 | 对照主干 `ModelTransformPanel`、`SceneLayoutPanel`；**UI-M1 仍须成立** | PROPOSED |
| **H-B-04** | Profile 选择：经 ABI 查询可用 Profile 与能力集，展示能力标签与生产安全级别 | H-B-01 | 对照主干 `ProductionModePanel` + `ScenarioRegistry`；**不得直接读 `slicer_scenarios.json`** | PROPOSED |
| **H-B-05** | 切片参数设置：DPI、层厚、输出目录、材料策略等；含有效配置预览与校验提示 | H-B-04 | 对照主干 `ConfigEditorPanel` / `QuickConfigPanel` / `EffectiveConfigGenerator` | PROPOSED |
| **H-B-06** | 切片提交与作业管理：提交、进度条、取消、错误展示 | H-B-05 | 对照主干 `ProductionSliceRunSession`；取消 ≤2s 且无 `.staging` 残留 | PROPOSED |
| **H-B-07** | 结果查看：包校验、摘要、报告、层预览与通道图 | H-B-06 | 对照主干 `PackageLoader` / `LayerPreviewPanel` / `ChannelChartPanel` | PROPOSED |
| **H-B-08** | 设置持久化与工作区状态：会话恢复、布局记忆 | H-B-05 | 对照主干 `WorkspaceLayoutState` | PROPOSED |

### H-C 组 · 移植交付物

| 卡号 | 任务 | 前置 | 验收 | 状态 |
|---|---|---|---|---|
| **H-C-01** | **逐文件三桶分类定稿**（§1.2 是候选，须逐个核实是否依赖 core 类型）| H-B-07 | 68 个文件全部归桶；桶 A 需给出「可直接复制」的证据（无 core include） | PROPOSED |
| **H-C-02** | 移植指南：桶 B 每个文件的「改走 ABI」改造要点与工作量估算 | H-C-01 | 打印侧确认可据此排期 | PROPOSED |
| **H-C-03** | 并排 A/B 对照报告：主干 UI 与宿主模拟对同一模型同一 Profile 的行为差异清单 | H-B-07 | 差异逐条有解释（属于「切片专有」或「已知裁剪」）| PROPOSED |

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
**当前决策**：2026-08-07 用户已要求冻结接口并继续 14F，因此该建议被覆盖；
H-A-01/02/03 保持 `PROPOSED / NOT ACTIVE`，不得插入当前 14F。

### 5.3 建议排序

```text
P0 · 当前唯一主线
     14F-03 → 14F-04 → 14F-05          冻结接口下完成切片侧本地收口

P1 · 14F 收口后先做受控准入评审
     H-A-01 → H-A-02 → H-A-03          ABI 场景生命周期；需新决策和用户授权
     H-B-01..08                        业务流程 UI（决定打印侧移植成本）
     R-A-01                            实测甲片三角面数
     R-B-01/02                         LOD 跳采样修复（若 R-A-01 判为 P1）

P2 · 14F 收口后
     H-C-01..03                        移植交付物（需业务流程先跑通）
     R-C-01/02                         屏幕空间 LOD 提示
     R-D-01                            Qt6 升级可行性评估

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

**14F 之后先评审 H-A 的受控合同修订，再决定是否启动 H-B 业务流程 UI。**
当前 14F 的目标是验证已冻结能力包和 S1/S2 合同，不宣称完整业务宿主已经交付。

---

## 6. 与其它专项的边界

| 专项 | 关系 |
|---|---|
| Stage 14（14A–14F）| H-A 修的是 Stage 14 冻结契约的一处缺口，属受控 minor 修订；H-B/H-C 完全独立 |
| `TASKS_RENDER`（R 组）| 无依赖。H-B 的视图部分复用 14E 已实现的 `ViewWorkspaceWidget` 与 `IRenderBackend` |
| Stage 16 | 无依赖，可完全并行 |
| 14E | 本专项是 14E 的**功能补齐**，不推翻 14E 已交付的机制（ModuleClient、三车道、渲染、能力覆盖全部复用）|

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-06 | v1.0 | 首版。定位 14E 交付「技术验证壳 ≠ 业务流程」的根因（卡片全为能力导向）；确认**主干 `slicer_debug_ui` 未被 Stage 14 改动、功能规格就在 HEAD**，68 个头文件按业务流程分组并给出三桶分类候选；给出 ABI 场景生命周期缺口的 A 级证据与**扩展 operations 枚举**的采纳方案（不新增能力/导出，minor 兼容）；分 H-A/H-B/H-C 三组共 14 张卡；确立业务级验收硬标准；**按「晚做的返工成本」建立 P0–P4 优先级框架，并修正「H-A 不能等到 14F 之后」** |
| 2026-08-07 | v1.1 | 对齐用户最新授权：Stage 14F 外部验证延期、对外接口冻结并继续本地收口；H-A 缺口分析保留，但改为 14F 后受控修订候选，禁止在当前 14F 中直接改变冻结合同 |
