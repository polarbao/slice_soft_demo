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
| TF-00 定位：区分「Refresh 返回 true」还是「返回 false 但 error 为空」 | PROPOSED | — | 待：给出确切分支与代码行，不得凭测试名推断 |
| TF-01 修复失败传递并补 three_d 侧覆盖 | PROPOSED | — | 待：TF-00 结论确定修法后填写 |
| TF-02 正例守护：未声明贴图的模型仍正常渲染 | PROPOSED | — | 待：新增正例用例 |
| TF-03 收口：定向 + 全量回归对照基线 | PROPOSED | — | 待：#241 转绿且失败集零新增 |

## 修订记录

- 2026-09-15：建卡。来源是 P0FIX 的 P0-00 回归基线第 7 条红灯，不是静态分析发现的，故不占 `analysis/` 的 F 编号。用户裁定单独立卡、不并入 P0FIX。当前只确认「断言不成立」，**未定位到具体分支**——TF-00 为硬前置，禁止跳过定位直接改代码。
