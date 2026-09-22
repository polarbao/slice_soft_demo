# ANALYSIS-03 标签栏与切片流程一致性实测（UX-01）

> 目录：`docs/plan-feature/单材料缩裹整模识别与整版多实例/` ｜ 日期：2026-09-20 ｜ 专项：**MONOWRAP**
> 索引：[README](README.md) ｜ 任务：[TASKS-02](TASKS-02-后续任务.md) 的 UX-01
> 证据等级：**A 级**（代码实测），逐条给位置。§4 末尾标注了未经运行验证的推断。
> 状态：**调研完成，改进方案待裁定。本文不含任何已执行的改动。**

---

## 0. 一个先说清的判据

**「标签顺序 ≠ 流水线顺序」本身不是缺陷。**
标签顺序应反映**用户操作顺序**，流水线顺序反映**数据依赖**，两者本就不必一一对应。
本文只标出**会让用户走错路**的不一致，其余归入 §3「看似不一致但合理」。

---

## 1. 标签栏实际顺序

**是两层 QTabWidget，不是一层**——这一点本身就值得注意。

### 1.1 顶层 `hostWorkspaceTabs`（`HostMainWindow.cpp:104-105`）

| # | 显示名 | 内容 | 位置 |
|---|---|---|---|
| 0 | 工作区 | 3D 视图 + 右侧 inspector 组 | `:191` |
| 1 | 结果 | `HostPackageReviewPanel` | `:193-194` |
| 2 | 设置 | 仅「显示设置」（默认视图 / 3D 投影） | `:196-232` |
| 3 | 模块诊断 | 只读日志 | `:234-242` |

### 1.2 嵌套 `hostSceneInspectorTabs`（`:124-125`，在「工作区」内）

| # | 显示名 | Panel | 位置 |
|---|---|---|---|
| 0 | 模型 | `HostModelListPanel` | `:161` |
| 1 | 工艺配置 | `HostProfilePanel` | `:163-164` |
| 2 | 切片设置 | `HostSliceSettingsPanel` | `:166-167` |
| 3 | 切片作业 | `HostSliceJobPanel` | `:169-170` |
| 4 | 变换与排版 | `HostTransformLayoutPanel` | `:172-174` |
| 5 | RIP 设置 | `HostRipSettingsPanel` | `:176-179` |

**无隐藏/条件标签**：`setTabVisible` / `removeTab` / `setTabEnabled` 在 `apps/slicer_ui_host_sim/` 内**零命中**。
10 个标签恒定可见可点，禁用是面板级（`HostMainWindowScene.cpp:136-141`）。

---

## 2. 切片流水线实际阶段顺序

**UI 实走的是 scene 路径**（`WorkerSliceExecutor.cpp:558-559`）：

| 序 | phase | percent | 位置（`MultiModelProductionService.cpp`） |
|---|---|---|---|
| 1 | `scene_config_load` | 0 | `:504-510` |
| 2 | `scene_model_load` | 10 | `:708-714` |
| 3 | `scene_admission` | 18 | `:749-755` |
| 4 | `scene_instance_slice` | 22→ | `:812-818`，逐实例 `:1003-1010` |
| 5 | `scene_composition` | 72 | `:1211-1217` |
| 6 | `scene_package_write` | 78→ | `:1377`、`:1456` |
| 7 | `scene_package_validation` | 96 | `:1485-1491` |
| 8 | `completed` | 100 | `:1564-1570` |

**宿主侧是同一套 phase**，外加两个宿主合成的（`queued` / `cancelling`，
`HostSliceJobController.cpp:182,213`），中文文案映射在 `HostSliceJobPanel.cpp:74-114`，
**对核心 8 个 phase 完全覆盖、无遗漏无多余**。

另有一个**正交**的作业生命周期状态机（`HostSliceJobController.h:13-22`：
`Idle → Queued → Running → (Cancelling) → Succeeded|Failed|Cancelled`）——
它与 phase 是两个维度，不是「两套 phase」。

⚠ **一处命名错位**：`HostSliceJobController.cpp:86-112` 把 `scene_admission` 的耗时
记进 `gridSetupMs`，但 admission 是准入校验不是建网格。
且 `scene_package_validation` 与 `completed` 返回空键，无耗时行。

---

## 3. 确实是问题的不一致（4 条）

### 问题 1｜「导入落位与规则排版」在 tab 4，但导入动作在 tab 0——**真实逆序**

> ✅ **2026-09-20 已修复**：该组已迁到「模型」页并紧邻「添加模型」，
> 「变换与排版」随之更名「变换」。修复前先补了 `hostflow_panel_ownership` 护栏，
> 它把「哪个控件在哪一页」钉死，使这次搬迁成为**显式改断言**而非悄悄变更。
> 详见 [DECISION-02](DECISION-02-CFG与UX的执行范围与取舍.md)。

`HostTransformLayoutPanel.cpp:134-160` 的「导入落位与规则排版」含
`导入时自动定向`(`:152`) 与 `导入后自动排版`(`:159`)。这两项在**导入执行的那一刻**被读
（`HostMainWindowImport.cpp:55-56,69-70`），而「添加模型」按钮在 tab 0
（`HostModelListPanel.cpp:64`）。

**用户会怎么走错**：按标签从左到右操作 → 在 tab 0 导入 → 模型以默认落位进场 →
之后才在 tab 4 发现这两个开关 → 勾选**无任何效果**（只在下一次导入时生效），
必须删掉重导或手动点「应用排版」。**序号 0 的动作依赖序号 4 的设置。**

### 问题 2｜自动 RIP 时，进度被切到一个看不见的标签里

`HostMainWindowResult.cpp:36` 切顶层到「结果」（index 1）；
紧接着 `:40-42` 若开启自动 RIP 则调 `StartRipForPackage`，其内
`HostMainWindowRip.cpp:78` 切 `m_inspectorTabs` 到「RIP 设置」——
但 inspectorTabs 嵌在「工作区」（顶层 index 0）里，**此刻顶层当前页是「结果」**。

**用户会怎么走错**：切片完成后自动跑 RIP，用户面前是「结果」页，
RIP 进度条全部渲染在不可见的面板上。看不到 RIP 在跑、跑到哪、是否失败，
只有 `m_statusLabel`（`HostMainWindow.cpp:245`）一行文字。失败分支同样如此。

### 问题 3｜顶层「设置」与 inspector 的「切片设置」重名近似

顶层 tab 2「设置」只有「显示设置」一个 group（`HostMainWindow.cpp:199-230`），
tooltip 明写「不改变场景或切片数据」。而切片参数在「工作区 → 切片设置」
（`HostSliceSettingsPanel.cpp` 的六个 group）。

**两处名称区分度不足，且层级深度相反**：无关的在浅层，关键的在深层。

### 问题 4｜RIP 的前置条件不可见

`HostRipSettingsPanel.cpp:483` 的运行按钮门槛是 `!m_packageDirectory.isEmpty()`，
该值唯一来源是切片成功后的 `HostMainWindowResult.cpp:31`。
用户在没切片前打开 RIP 设置，只看到一个灰掉的按钮，**UI 未说明「需先完成一次切片」**。

---

## 4. 看似不一致但实际合理的

| 现象 | 为什么合理 |
|---|---|
| 「变换与排版」排在「切片作业」之后 | 变换主入口是 3D 画布直接操作，tab 4 的「变换」子页是数值微调的补充；`RefreshSliceJobReadiness`（`HostMainWindowJob.cpp:6-43`）四道门槛里**没有一条要求变换已完成**——变换是可选的。（只为「变换」子页开脱，不为「导入与排版」子页，见问题 1） |
| 标签 `模型→工艺配置→切片设置` vs 流水线 `config_load→model_load` | **标签序更正确**。流水线先读配置是数据依赖，用户操作序必须先有模型才谈得上配材料，`HostMainWindowJob.cpp:29-33` 的「请先导入模型」门槛确认了这一点。两序天然相反 |
| 「工艺配置」在「切片设置」之前 | **必须如此**。`OnProfileChanged`（`HostMainWindowProfile.cpp:42-79`）会改写 `processpresetid`/`packageprotocol`/`transferchannel`——工艺是切片设置的上游 |
| 顶层「结果」在「工作区」之后 | 数据上确是下游，且切片完成时自动切过去，用户不必手动找 |
| 「RIP 设置」排在最后 | RIP 是切片的下游消费者，顺序符合依赖。它的问题是**可见性**（问题 2、4），不是顺序 |
| 宿主状态机与核心 phase 并存 | 两者维度正交，宿主只补了核心无法表达的 `queued`/`cancelling`，文案映射无漂移 |

---

## 5. 改进建议（待裁定，本专项不擅自执行）

| # | 针对 | 建议 | 代价 |
|---|---|---|---|
| 1 | 问题 1 | 把「导入落位与规则排版」从 tab 4 移到 tab 0（模型面板内，紧邻「添加模型」） | 面板搬迁，需同步 `HostInspectorPages.h` 的分组 |
| 2 | 问题 2 | 自动 RIP 时不要切 inspector，改为把 RIP 进度也投到「结果」页；或干脆不自动切顶层 | 改两处切换逻辑 |
| 3 | 问题 3 | 顶层「设置」改名「显示」或「外观」，与「切片设置」拉开区分度 | 改一个字符串 |
| 4 | 问题 4 | RIP 面板在无包时显示「需先完成一次切片」提示，而不是只灰掉按钮 | 加一行提示标签 |
| 5 | §2 命名错位 | `scene_admission` 的耗时不要记进 `gridSetupMs`；两个空键补上 | 改耗时键映射 |

**建议优先级：3（一个字符串）→ 4 → 2 → 1 → 5。**
问题 1 收益最大但搬迁面也最大，应在有 UI 回归护栏之后再动。

---

## 6. 未验证项

1. 问题 2 的可见性推断基于 Qt QTabWidget 语义与代码调用顺序，**未实际运行程序观察**。
2. 「结果」标签在无切片包时的空状态表现未验证。
3. 顶层「设置」是否有其他入口补偿命名混淆，未穷尽排查。
