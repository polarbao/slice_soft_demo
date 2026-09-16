# TEXFAIL 缺贴图静默降级为灰模型修复

日期：2026-09-15。授权：用户 2026-09-15 在 P0FIX 基线红灯处置一问中选择「单独立卡」。本卡独立于 P0FIX，不并入其范围。

来源：P0FIX 的 P0-00 回归基线（2026-09-14，`d28b6451`，246 项 7 既有失败）中的第 7 条红灯 `slicer_stage14e04d_dual_view_contract_test`。

## Implementation Plan

### Problem Type

**已声明协议不变量的实时违规**：声明了贴图的模型在贴图缺失时，本应失败并返回错误，实际却被静默降级成灰模型。不是性能、不是几何、不是工艺问题。

### Layer(s) Involved

宿主渲染刷新路径（`TopViewRenderPolicy` / `topRenderer.Refresh`）、ViewData 资源解析（`src/slicer_core/api/viewdata/SceneViewAssetResolver`、`SceneViewResources`）、外观完整性准入（`ModelAppearanceAssessment`）。

### Official Documents

`contracts/slicer_capability_dtos.json` 已把这条写成机器可读的不变量：

```
protocolInvariants.textureRequiredIfPresent : true
protocolInvariants.noSilentTextureFallback  : true
viewDataRules.failureMode                   : "declared texture load/decode/UV/material
                                               failure returns an error and no success"
viewDataRules.budgetExhaustion              : "...; never drop"
```

`AGENTS.md` 亦反复写明「任何引擎失败均不得静默回退」。**本卡是兑现既有合同，不是新增约束。**

### Historical Documents

`analysis/09_输入健壮性与安全面.md`（输入面与降级面盘点）、`analysis/04_问题清单与改动空间.md`（F-01..F-36，本条不在其中——它是基线实测暴露的，不是静态阅读发现的）。

### AI Workspace Evidence

分支 `codex/feature-p0fix-contract-robustness`（P0FIX 专项分支）。本卡尚未开工，若开工建议另切分支，避免与 P0FIX 的加闸改动混在一次回归里。

### Current Code Reality

`tests/stage14e_04d/Stage14E04DViewSwitchTests.cpp:347-358`：

```cpp
const QString missing = fixtures.filePath(QStringLiteral("missing_texture_small.obj"));
Import(client, missing);
SceneInteractionController badController(client);
Require(badController.Initialize(BuildScene(missing, QStringLiteral("missing")), &error), error);
error.clear();
TopViewFrame badFrame;
Require(!topRenderer.Refresh(badController.SceneHandle(),
    badController.SceneRevision(), &badFrame, &error)
    && !error.isEmpty(),
    QStringLiteral("missing texture silently became a gray model"));
```

断言要求 `Refresh` **返回 false 且 `error` 非空**。当前该断言失败，说明二者至少有一个不成立。

### Current State

**红灯，原因未定位到具体分支。** 只知道断言不成立，尚未区分是以下哪一种：

- `Refresh` 返回了 true（缺贴图被当成可接受，走了灰模型/降级外观）
- `Refresh` 返回 false 但 `error` 为空（失败了但没给出可读原因，宿主无法呈现）

**这两种的修法完全不同**，所以 TF-00 定位是硬前置，不得跳过直接改代码。

### Target State

声明了贴图的实例在贴图缺失/解码失败/UV 缺失/材质失败时，`Refresh` 失败并携带稳定可读错误；宿主据此明确提示用户，而不是渲染出一个看起来正常的灰模型。

### Historical State

不改变**未声明贴图**的模型的行为——它们本来就应该正常渲染为无贴图外观。本卡只针对「声明了贴图但取不到」。

### Pending Confirmation

- 修复方向若涉及宿主 UI 的错误呈现方式（弹窗/状态栏/列表标记），需产品确认呈现形态。TF-00 定位后再提。

### Risk Points

- **把「未声明贴图」误伤成失败**：会让一批正常模型无法预览。必须用正例守住。
- **只修 top 视图漏掉 three_d**：测试里两条渲染路径分开（`topRenderer` / `threeDRenderer`），两条都要覆盖。
- **改成失败后宿主没有呈现**：从「静默灰模型」变成「静默不显示」，对用户没有改善。
- 本卡与 P0FIX 若在同一分支同一次回归里做，失败集归因会混淆。

### Files To Change

TF-00 定位后再确定。预计落在 ViewData 资源解析与外观准入，以及宿主渲染刷新的错误传递路径。**不改** DTO 不变量本身（它已经是对的），**不改**通道语义与包字节。

### Verification Plan

构建目录 `build-slicesoft/main`，构建与回归分开判定退出码。定向验证 `slicer_stage14e04d_dual_view_contract_test` 转绿，并补正例（未声明贴图的模型仍正常渲染）与 three_d 侧负例。收口时全量回归对照 P0FIX 的 P0-00 基线（246 项 7 失败），本条应从失败集中消失且不得新增其他失败。

## 任务清单

| 任务 | 状态 | 完成日期 | 实际验证 |
| --- | --- | --- | --- |
| TF-00 定位：区分「Refresh 返回 true」还是「返回 false 但 error 为空」 | COMPLETE | 2026-09-16 | **是第一种：`Refresh` 返回 true**。确切分支与行号见下 |
| TF-01 修复失败传递并补 three_d 侧覆盖 | COMPLETE | 2026-09-16 | **实际未扩展枚举**——契约已规定，两处改动即可。原裁定的前提被后续调查推翻，见 `DOC_DECISION_TEXFAIL_R1` |
| TF-02 正例守护：未声明贴图的模型仍正常渲染 | COMPLETE | 2026-09-16 | 既有正例已覆盖且保持绿：`textured_scene_viewdata_14b03a` 全套、`hostflow_hb01_model_import`（导入层未受影响）、`scene_facade_14b03`（untextured 路径）。未新增用例——重复覆盖无益 |
| TF-03 收口：定向 + 全量回归对照基线 | COMPLETE | 2026-09-16 | 全量回归 250 项 / 5 失败（1887.66 s），**从基线失败集中消失 2 条**：slicer_stage14e04d_dual_view_contract_test（本卡目标）与 slicer_stage14c04_sync_capability_safety_test（**独立测试同一不变量**，其 `ValidateMissingTextureFails` 因同一缺陷红了整个专项期）。新增失败 **0** 条 |

## TF-00 定位结论（2026-09-16）

### 是哪一种

**`Refresh` 返回了 true。** 不是「返回 false 但 error 为空」。

原断言是复合条件，失败时只打印一句 `missing texture silently became a gray model`，
分不出两种情形。已把它拆成两条具名断言（本卡唯一落地的代码改动），现在红灯自己说清楚：

```
14E-04d FAIL: declared-texture model with a missing texture must FAIL Refresh,
              but Refresh returned true (silently became a gray model)
```

### 确切的代码路径

| 位置 | 事实 |
|---|---|
| `src/slicer_core/model.cpp:1861-1871` | 检测**是对的**：识别出 `status="degraded_missing_texture"` 并给出可读 detail |
| 同处 | 但它用同一个布尔位 `single_material_only=true` 表达了**多种**降级 |
| `src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp:148-149` | 只读该布尔位，**丢弃 `status`**——丢掉的正是区分所需的信息 |
| 同文件 `:194-196` | 套用隐式中性（灰）材质 |
| 同文件 `:241` | 因该位为真而**跳过整段贴图解析**，连带跳过 `:247` 那条**本来就正确**的失败传递 |
| 同文件 `:84-89` | 被跳过的那条失败：`PM-SLICER-INPUT-0001` +「used ViewData material declares a missing texture」 |

**fixture 佐证**：`missing_texture_small.obj` 单材质 `missing_tex`，
`map_Kd textures/does_not_exist.png` —— 正是「声明了贴图但取不到」。

### 附带查出的一条：`TexturePolicy` 有名无实

宿主在 `scene.get_viewdata` 请求里**确实**发了 `"texturePolicy": "require_if_present"`
（`TopViewRenderPolicy.cpp:79-80`、`SceneRenderPolicy.cpp:80-81`），
模块侧 `SceneCapabilityAdapter.cpp:692` 也**校验**了这个字符串。

但 `src/slicer_core/api/SceneViewDtos.h:59` 的

```cpp
enum class TexturePolicy { RequireIfPresent };
```

是个**单值枚举**，`RequireIfPresent` 全仓只出现 **2 次**——枚举声明本身与 `:177` 的字段默认值。
**从未被读取、比较或分支。** 策略被接受、被校验、被存储，然后被忽略。

单值枚举看起来是「完整的」，没有第二个值可以跳转，于是谁也没写那条分支——这是它长期没被发现的原因。

## TF-01 为何 BLOCKED

### 两次尝试与撤回理由

**尝试一：在 ViewData 解析层按 `status` 分流。** 让 `degraded_missing_texture` 不走中性路径，
由既有的 `ResolveTexture` 产生失败。#245 转绿，但打坏了
`tests/stage14b_03a/PositiveCases.cpp:156` 的 `MissingTextureFallsBackToNeutralGrayCase`——
该用例**明确要求**缺贴图时 ViewData 返回 OK，并标记 `texture_status=NotProvided`、`textures` 为空。
**已撤回。**

**这两条测试其实不冲突，冲突的是我选错了层。** 14b03a 要的是**声明式回退**（如实报告没提供贴图），
契约 `noSilentTextureFallback` 反对的是「静默」而非「回退」本身；14e04d 要的是**宿主拒绝渲染**。
两者可以同时成立。

**尝试二：把判定挪到宿主层。** three_d 路径（`SceneRenderPolicyData.cpp:380-422`）**早就有**
这条判定：

```cpp
const bool textureContractClosed =
    textureStatus == "available"
    || (textureStatus == "not_provided" && identities.value(appearance).isEmpty());
```

即 `not_provided` 仅当该外观本就没声明贴图时可接受。**top 路径缺这条**——
`TopViewRenderPolicyData.cpp:155` 只把 `textureStatus` 读进结构体就不管了。
补齐后 14b03a 恢复绿，但 **#245 仍红**。**已撤回。**

### 真正的阻塞点

补齐后仍红的原因是决定性的：走中性路径时外观里**根本没有 textures**，
所以宿主看到的是「外观没声明贴图 + `not_provided`」——与「模型本来就没贴图」
**在载荷里逐字节相同，无法区分**。

而 `SceneViewDtos.h:20-24` 的

```cpp
enum class TextureStatus { Available, NotProvided };
```

**只有两个值**，`SceneViewDataAdapter.cpp:242-244` 也只做二值映射。
载荷里没有任何位置能表达「声明过但缺失」。

宿主要能拒绝，必须**扩展这个 DTO**——新增第三种状态（如 `DeclaredButMissing`）或独立字段，
牵涉：`SceneViewDtos.h` 枚举、`SceneViewDataAdapter.cpp` 映射、
`contracts/slicer_capability_dtos.json` 的相关钉、top 与 three_d 两条宿主解码路径。

按本仓惯例这属于**受控修订，需先出裁定与决策文档**（参照 P0FIX 的 P0-02）。
本卡的 Pending Confirmation 一节也已预告过需要确认。**故不单方面改冻结契约，在此止步待裁。**

### 裁定结果（2026-09-16）

**用户选定选项 A：扩展 `TextureStatus` 加第三态。** 理由是它最贴合契约
`protocolInvariants.noSilentTextureFallback` 的原文，且 three_d 侧已有的
`textureContractClosed` 判定形状可直接复用。

**开工前置**：这是对冻结 DTO 的受控修订，须先出决策文档（参照 P0FIX 的
`DOC_DECISION_P0FIX_R1_*`），把下列改动面逐条列清并逐条验证：

| # | 改动面 | 说明 |
|---|---|---|
| 1 | `src/slicer_core/api/SceneViewDtos.h` | `TextureStatus` 增第三态（建议 `DeclaredButMissing`） |
| 2 | `src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp` | 中性路径下按 `status==degraded_missing_texture` 置第三态；**不改** 14b03a 要求的「返回 OK + textures 为空」 |
| 3 | `src/slicer_module/SceneViewDataAdapter.cpp:242-244` | 二值映射改三值 |
| 4 | `contracts/slicer_capability_dtos.json` | 相关 const 钉与 `viewDataRules` 同步 |
| 5 | `apps/slicer_ui_host_sim/render/TopViewRenderPolicyData.cpp` | 补 `textureContractClosed` 判定（本轮已试过形状，撤回时留有记录） |
| 6 | `apps/slicer_ui_host_sim/render/SceneRenderPolicyData.cpp` | three_d 侧同步认第三态 |

**排期**：用户裁定本梯队**先做 F-45**，TF-01 随后开工。

### 原提交的裁定选项（存档）

| 选项 | 含义 | 代价 |
|---|---|---|
| A. 扩展 `TextureStatus` 加第三态 | 宿主据此拒绝渲染，两条路径对称 | 受控修订；触及 DTO、适配器、契约钉、两条解码路径 |
| B. 外观里保留「声明过的贴图」痕迹 | 不改枚举，改为让降级外观仍带贴图声明但标记不可用 | 改动面可能更大，且会动 14b03a 的 `textures.empty()` 断言 |
| C. 维持现状，改 14e04d 的期望 | 承认「缺贴图渲染成灰 + 导入层已报告降级」是可接受行为 | **与契约 `noSilentTextureFallback` 直接冲突**，须同时修订契约 |

我倾向 **A**：它最贴合契约原文，且 three_d 侧已有的判定形状可以直接复用。

## 本卡当前落地的改动

**只有一处**：`tests/stage14e_04d/Stage14E04DViewSwitchTests.cpp` 的复合断言拆成两条具名断言。
这是纯诊断改善——断言的**条件逐字未变**，只是失败时能说清是哪一种。
#245 仍在失败集中（与基线一致），未新增也未消失任何失败。

## TF-01 / TF-02 / TF-03 收口（2026-09-16）

### 实际修法与原裁定不同

原裁定是「扩展 `TextureStatus` 加第三态」，依据是我当时的分析：载荷无法区分
「声明了但缺失」与「从未声明」。**写决策文档时读到 `slicer_capability_dtos.md:220-222`，
发现契约早已规定了这种情形**，唯一合规结果是失败，不需要第三态。
原裁定所依据的分析不完整，已由 `DOC_DECISION_TEXFAIL_R1` 推翻并留痕。

**改动面从原估的六处降到两处**：

| # | 文件 | 改动 |
|---|---|---|
| 1 | `src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp` | `useDegradedNeutralAppearance` 增加 `status != "degraded_missing_texture"` 条件 |
| 2 | `tests/stage14b_03a/PositiveCases.cpp` | 改写 `MissingTextureFallsBackToNeutralGrayCase` 为 `DeclaredButMissingTextureMustFailCase` |

`TextureStatus` 枚举、`SceneViewDataAdapter` 映射、`slicer_capability_dtos.json` 的
`enum:available|not_provided` 钉、两条宿主解码路径——**全部未动**。

### 一个真实损坏的资产被揪了出来

冻结矩阵 `hostflow_hd02_real_asset_matrix` 由 29/0/2 变为 28/0/3，恰好一个资产移动：

**`model/obj/aishen_fudiao/MF_aishen_xiaozhi_L.obj`** —— 它被 `usemtl` 绑定的材质
`blinn1SG` 声明了 `map_Kd RGB.png`，而**该文件不存在**。此前它在宿主里被渲染成灰模型，
正是本卡要治的病的真实实例；现在按契约被拒绝（`PM-SLICER-INPUT-0001`）。

**这不是回归，是缺陷被正确暴露。** 已按该矩阵的既有先例重固化并留痕（含上述资产名与原因）。
同目录另两个资产逐个核过不受影响：`MF_aishen_zhongzhi_L_tx03.obj` 是 mtllib 整份缺失
（另一种降级，本次未改）；`MF_shengdanjie_zhongzhi_R_fy02.obj` 被用材质的贴图存在，
缺的都是未被 `usemtl` 绑定的材质。

### 收口回归

全量回归 250 项 / 5 失败（1887.66 s），**从基线失败集中消失 2 条**：slicer_stage14e04d_dual_view_contract_test（本卡目标）与 slicer_stage14c04_sync_capability_safety_test（**独立测试同一不变量**，其 `ValidateMissingTextureFails` 因同一缺陷红了整个专项期）。新增失败 **0** 条

### 仍留在案上的

`TexturePolicy` 仍是全仓从未被读的单值枚举（TF-00 附带发现）。宿主发
`require_if_present`、模块 `SceneCapabilityAdapter.cpp:692` 校验它、然后忽略它。
修掉 ViewData 层之后它在功能上已无影响，但作为「声明了却不消费」的残留应单独清理。

## 修订记录

- 2026-09-16：**TF-01~TF-03 完成，本卡收口**。实际修法与原裁定不同——契约已规定，无需扩展枚举，改动面六处降为两处，推翻留痕见 `DOC_DECISION_TEXFAIL_R1`。顺带揪出一个真实损坏的资产 `MF_aishen_xiaozhi_L.obj`（声明 RGB.png 但文件不存在），冻结矩阵据此重固化为 28/0/3。

- 2026-09-16：**TF-00 完成**，定位为「`Refresh` 返回 true」，确切到 `SceneViewAssetResolver.cpp:148-149` 丢弃 `status` 这一行。附带查出 `TexturePolicy` 是全仓从未被读的单值枚举。**TF-01 转 BLOCKED**：两次修法尝试均已撤回，真正的阻塞是 `TextureStatus` 只有两个值、载荷无法区分「声明了但缺失」与「从未声明」，需先裁定是否扩展该冻结 DTO。

- 2026-09-15：建卡。来源是 P0FIX 的 P0-00 回归基线第 7 条红灯，不是静态分析发现的，故不占 `analysis/` 的 F 编号。用户裁定单独立卡、不并入 P0FIX。当前只确认「断言不成立」，**未定位到具体分支**——TF-00 为硬前置，禁止跳过定位直接改代码。
