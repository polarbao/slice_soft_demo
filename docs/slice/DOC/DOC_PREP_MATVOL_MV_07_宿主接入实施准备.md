# DOC_PREP_MATVOL MV-07 宿主接入实施准备

> 文档状态：**PREPARED / 等待生产语义变化确认**
> 版本：v1.0 ｜ 日期：2026-08-21
> 任务真源：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`
> 依据：`CODEX_PROMPT_MATVOL` §2「生产语义变化卡必须先给 Implementation Plan 并等待确认」

---

## 0. 为什么单独立这份准备

MV-01..MV-06 全部落在 `src/slicer_core` 内、且对既有行为只做**收窄**（新块默认关闭、
新条件只在 opt-in 时生效），因此可直接实施。**MV-07 不同**：它必须放宽参考宿主里两道
既有生产校验门，且宿主目录受两套独立门禁约束。下面把约束、方案与未决项一次列清。

## 1. 三条改变卡片性质的约束（A 级，已实测）

### 1.1 宿主绝对不能引用 slicer_core

`tests/stage14e_02/ValidateQtHostBoundary.py:50-60` 的禁止清单是**裸子串**匹配：

```python
forbidden = ('#include "slicer_core/', "#include <slicer_core/",
             "slicer_core", "slicer_base", "slicer_engine")
```

⇒ 宿主侧**不能复用** `MaterialVolumePolicyConfig`（`src/slicer_core/config.h`），
必须在宿主自建镜像结构，序列化成 `materialVolumePolicy` JSON 块，由 Worker 侧
`load_slice_config()` 解析回来。**连注释里写这个词都会 FAIL。**

### 1.2 两套行数门禁，宿主 UI 无法申请豁免

| 门禁 | 规则 | 当前状态 |
|---|---|---|
| `ValidateQtHostBoundary.py:81-85` | 任何宿主 `.cpp/.h` > 500 行即 FAIL | 🔴 **HEAD 上已失败**，8 个文件超限，fail-fast 先报 `HostModelImportWorkflow.cpp (516)` |
| `scripts/ValidateSourceSizeGuard.py` | G1 新源 >500；G3 新头 >200；G2 base>1000 的文件不得再增长 | ✅ self-test PASS |

`scripts/SourceSizeGuardConfig.json` 的 `protectedPrefixes` 含 `apps/slicer_ui_host_sim/`，
而 `ReadConfig` 明确拒绝把受保护前缀放进 `allowlist`（当前白名单为空）。

```text
HostSliceSettingsPanel.cpp  820 行 → 距 G2 永久冻结线（1000）仅剩 180 行
HostMaterialProfile.c       404 行 → 不在受保护前缀，余量 596 行
HostRequestBuilder.c        561 行 → 同上，余量 439 行
HostProcessPresetCatalog.cpp 126 行 → 余量充足
```

⇒ **实施纪律**：MATVOL 的宿主 UI 逻辑必须新建独立子面板文件（新 `.cpp` ≤ 500、新 `.h` ≤ 200），
语义尽量堆在 `HostProcessPresetCatalog.*` 与 `apps/slicer_host_sim/`（后者不受保护前缀约束），
**不得**把大块逻辑加进 `HostSliceSettingsPanel.cpp`。

### 1.3 必须放宽两道既有生产门（本卡需要确认的核心）

「多材质纵深 RGB + 按需补白」意味着 `whitepolicy == WhiteUnderbase` 而
`materialstrategy != RgbSolid`，这会同时撞上：

```text
① apps/slicer_ui_host_sim/HostSliceSettings.cpp:297-310
   按需补白只支持 Legacy 全实体 RGB 纹理、RGB 实体材料且禁用材料角色映射。

② apps/slicer_host_sim/HostMaterialProfile.c:79-86 与 :210-215
   BuildWhiteCarrierFragments 的前置门 + HostBuildMaterialProfileFragments 的短路，
   条件不满足时【静默返回 0】导致 Build 返回 NULL。
```

⇒ 这两处**必须显式放宽为新组合，不得绕过**。放宽方式须与 MV-06 在 `config.cpp` 的做法同构：
只增加 `|| materialvolumeenabled` 形态的条件，使**旧组合逐字节行为不变**。

## 2. 已确认的第四条约束：slicingMode 耦合缺口

`src/slicer_core/config.cpp` 的 MATVOL 校验要求 `slicingMode=relief_heightfield`，
而宿主的推导在 `apps/slicer_host_sim/HostRequestBuilder.c:355-362`：

```c
useReliefHeightfield = settings->textureenabled != 0
    || (settings->geometrysamplingstrategy == HOST_GEOMETRY_SAMPLING_SLAB_2X2_AT_LEAST_TWO
        && (settings->materialstrategy == HOST_MATERIAL_WHITE_SOLID
            || settings->materialstrategy == HOST_MATERIAL_VARNISH_SOLID));
```

⇒ 若 MATVOL 预设不开纹理，宿主会发出 `closed_mesh_scanline`，**Worker 必然拒绝**。
而 MATVOL 的本意恰恰是「不走纹理 RGB 路径」。修法：`useReliefHeightfield` 增加
`|| settings->materialvolumeenabled != 0` —— MATVOL 本身就是 relief 列采样路径，这是正确归类。

## 3. profileHash 闭合的四条规范化规则（不可违反）

Worker 权威算法 `src/slicer_core/api/ProfileIdentity.cpp:11-20` 用
`slicer_core::Json::dump(0)` 做规范化，因此宿主手写的 canonical 串必须逐字节对齐：

```text
① 键序 = std::map 字典序；materialVolumePolicy 落在 materialRoleMapping 与 modelFill 之间
② indent=0 下每个键值对独占一行，格式为 "key": value（冒号后有一个空格），行尾 ,\n
③ 空容器不换行：空数组是 [] 而不是 [\n]；空对象是 {}
④ 数字：整数不带小数点；浮点用 %.15g
```

### 3.1 H-F-04 的根因与必须复用的写法

提交 `ddbcb82` 修的正是规则 ③：当时 `rules` 的方括号写在外层模板里，空规则产出
`"rules": [\n]`，而 Worker 的 `dump(0)` 产出 `"rules": []`，SHA-256 不同 →
`PM-SLICER-PROFILE-0030`。修法是**方括号由片段自己产出、空分支显式返回 `"[]"`**，
且 canonical / compact **成对实现**（`HostMaterialProfile.c:21-71`）。

⇒ MATVOL 的 `overlap.rules` 必须照抄这个形状，且新块必须**条件产出**：
未启用时两份片段都是空串 `""`，保证旧六类预设的 canonical 串**字节不变、profileHash 不变**。

## 4. 建议的原子拆分

| 子卡 | 范围 | 是否含生产语义变化 |
|---|---|---|
| **MV-07A** | 宿主设置镜像结构、`apps/slicer_host_sim/` 的 `materialVolumePolicy` 双模板发射、`useReliefHeightfield` 耦合修复、放宽 §1.3 两道门、值域校验、预设目录新增一条、`VerifyPresetProfileHashClosure` 覆盖 | **是** —— 需本文确认 |
| **MV-07B** | 新建独立子面板（新 `.cpp` ≤500 / `.h` ≤200）、控件 objectName、能力不足时禁用而非回退、UI smoke self-test、`HostWorkspaceState` 持久化 | 否（纯 UI 增量） |
| **MV-07C** | 结果页 RGB-only 判读入口与「S 为伪彩色」标注 | 否 |

## 5. 验收口径的两处修正

### 5.1 「无资产能力时禁用而非静默回退」与现状冲突

`HostSliceSettingsPanel.cpp:654-672` 的 `SetSingleMaterialRestriction` 与
`:288-293` 的 `SetSingleMaterialOnly` **当前确实做自动回落**。MV-07 要求禁用而非回退，
因此新预设的能力门应照 `HostProfilePanel::SetProfiles`（`HostProfilePanel.cpp:85-135`）
的形状：条目 `setEnabled(false)` + 原因上屏 + 全不可用时整面板禁用，
并在 `BuildSubmissionProfile` fail-closed。**不要**沿用回落写法。

### 5.2 「scene revision 持久化一致」在现状下无对应实现

`HostWorkspaceState` 里没有任何 revision 键；场景修订号只在进程内
（`HostModelImportWorkflow` / `SceneInteractionController` / `TransformCommitPolicy`）。
⇒ 该验收项应理解为：workspace 与 profile 经 `HostWorkspaceState` 持久化；
revision 保持进程内，且新预设不得破坏 `HostSliceSettingsPanel::ValidateSceneBinding`。

### 5.3 RGB-only 入口已存在，缺的是默认与标注

`HostPackageReviewPanel.cpp:136-157` 的预览模式 index 0 就是 `RGB（纹理）`，
但默认停在 index 4 的六通道组合。而**全仓库 `apps/` 与 `src/` 下「伪彩」零命中** ——
S 通道伪彩色在 UI 上完全没有说明文字，伪彩混色实际发生在
`src/slicer_core/preview/MaterialPreviewComposer.cpp`，调色板来自 profile 的
`preview.pseudoColors`。⇒ MV-07C 的最小改动是调整默认索引 + 用
`Qt::ToolTipRole` 与既有 `hostPackageStage16SummaryLabel` 加标注，不必新建控件。

## 6. 待确认事项

| 编号 | 事项 | 为何需要确认 |
|---|---|---|
| **MV07-Q1** | 是否授权放宽 §1.3 的两道宿主生产门 | 改的是既有 white_underbase 校验语义，属生产语义变化 |
| **MV07-Q2** | `ValidateQtHostBoundary` 的 500 行规则：拆文件、调规则，还是接受门禁维持红色 | HEAD 上已 8 处违规；宿主 UI 无法白名单 |
| **MV07-Q3** | MATVOL 预设是否需要固定几何采样或层厚 | 若需要则须先扩 `hostprocesspreset`（当前只承载 4 个语义域）与面板套用逻辑 |
| **MV07-Q4** | MV-07C 是否可改结果页默认预览索引 | 会影响既有结果页 UI smoke 的默认项断言 |

## 7. 边界

```text
✅ 不引入宿主对 slicer_core 的任何引用（含注释）
✅ 放宽门禁只增加 materialvolumeenabled 形态条件，旧组合逐字节行为不变
✅ 新块条件产出，既有六类预设的 canonical 串与 profileHash 不变
✅ 不改 p0.rgbwsv.2、RGBWSV、SPI 与 11 个导出
⛔ 未获 MV07-Q1 确认前不修改 HostSliceSettings.cpp 与 HostMaterialProfile.c 的既有门
⛔ 不把大块 UI 逻辑加进已 820 行的 HostSliceSettingsPanel.cpp
```

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-21 | v1.0 | 首版。固化宿主禁引 slicer_core、两套行数门禁与宿主 UI 不可白名单、必须放宽的两道白区门、slicingMode 耦合缺口、profileHash 四条规范化规则与 H-F-04 根因；提出 MV-07A/B/C 原子拆分；修正三处验收口径（禁用而非回退、revision 不持久化、RGB-only 入口已存在但缺默认与标注）；登记 MV07-Q1..Q4 待确认。 |
