# DOC_DECISION_TEXFAIL-R1 声明贴图缺失时的视图数据行为

## Status

**已定案**（2026-09-16）。本文档**推翻**了 2026-09-16 早些时候在
`TASKS_TEXFAIL_缺贴图静默降级为灰模型修复.md` 中记录的裁定（「扩展 `TextureStatus` 加第三态」）。
推翻的理由是该裁定所依据的分析不完整——详见 Context。

## Context

### 原裁定与它依据的分析

TF-00 定位出：声明了贴图但取不到时，`SceneViewAssetResolver` 走中性外观路径，
`Refresh` 返回 true，用户拿到灰模型。

据此我提出三个选项并推荐「扩展 `TextureStatus` 加第三态（如 `DeclaredButMissing`）」，
理由是当前 `enum class TextureStatus { Available, NotProvided }` 无法区分
「声明了但缺失」与「从未声明」，宿主因此无从拒绝。用户选定该选项。

### 后续发现：契约早已规定，无需第三态

写决策文档、逐条核实改动面时读到 `contracts/slicer_capability_dtos.md:220-222`：

> 对声明纹理的模型，`top` 必须返回带纹理 `surfacePreview`，`three_d` 必须返回 UV、材质和纹理；
> 预算不足可降低 LOD/纹理分辨率，但**不得静默退为无纹理灰模**。**模型本身没有纹理时**返回
> `textureStatus=not_provided` 并使用 `baseColorFactor`。

三层含义，逐条对照本场景：

| 契约条款 | 本场景 |
|---|---|
| 声明纹理的模型，top **必须**返回带纹理 `surfacePreview` | 贴图文件不存在，**无法**满足 |
| 预算不足可降 LOD/分辨率，但**不得静默退为无纹理灰模** | 连预算不足都不许退灰，缺文件更不许 |
| `not_provided` 适用于**模型本身没有纹理** | 本场景模型**声明了**贴图，不适用 |

**结论：契约里唯一合规的结果是失败。** 第三态不但不必要，而且会把
「声明了但缺失」洗成一种可接受的成功态——与 `protocolInvariants.noSilentTextureFallback: true`
直接冲突。

### 那么真正违约的是谁

`tests/stage14b_03a/PositiveCases.cpp` 的 `MissingTextureFallsBackToNeutralGrayCase`：

```cpp
auto model = MakeTexturedQuad("missing-texture.obj", "used", "missing.png");
model.material_infos.front().texture_exists = false;   // 声明了贴图，但文件不存在
...
Require(top.IsOk(), "used missing texture should use an explicit neutral fallback");
```

`MakeTexturedQuad`（`TestSupport.h:162`）设 `material.has_texture = true`，
用例再把 `texture_exists` 改为 false——**正是契约点名禁止的那个场景**，
而该用例要求 `IsOk()`。

**该用例编码的行为与契约相反，且全仓没有任何 `DOC_DECISION_*` 授权过这个偏离**
（已逐份核对 `docs/` 下非归档的决策文档）。它是被写成这样的，不是被裁定成这样的。

### 为什么一直没被发现

两条测试各自为政、从未被放在一起看：

- `slicer_stage14e04d_dual_view_contract_test` 要求失败 —— 它**一直是红的**，
  在 P0FIX 的基线失败集里躺了整个专项。
- `stage14b_03a` 要求成功 —— 它**一直是绿的**。

一红一绿，红的那条被当成「待修的历史红灯」，没人追问它为什么红；
绿的那条被当成既有正确行为。**两者矛盾这件事本身，直到有人真去修红灯才暴露。**

## Decision

1. **不扩展 `TextureStatus`。** 维持 `{ Available, NotProvided }` 两态，
   `contracts/slicer_capability_dtos.json:206` 的 `enum:available|not_provided` 不动。
2. **ViewData 层对「声明了贴图但取不到」返回失败**，复用既有的
   `PM-SLICER-INPUT-0001`／`used ViewData material declares a missing texture`，不新增错误码。
3. **改写 `MissingTextureFallsBackToNeutralGrayCase`**，使其断言契约要求的失败行为，并改名。
4. **导入层不动。** `AssessModelAppearance` 仍返回 `single_material_only=true` 与
   `status="degraded_missing_texture"`，宿主导入流程的「降级导入」提示逐字不变
   （`HostModelImportWorkflowTests.cpp:402-407` 保持绿）。

### 变更清单

| # | 文件 | 改动 |
|---|---|---|
| 1 | `src/slicer_core/api/viewdata/SceneViewAssetResolver.cpp` | `useDegradedNeutralAppearance` 增加 `status != "degraded_missing_texture"` 条件 |
| 2 | `tests/stage14b_03a/PositiveCases.cpp` | 改写该用例断言失败并改名 |

**只有两处。** 原裁定估的六处改动面（DTO 枚举、模块映射、契约钉、两条宿主解码路径等）**全部不需要**。

## Boundaries

- **不改**「模型本身没有纹理」的行为：那种模型仍返回 `not_provided` + `baseColorFactor`。
  这是契约明文规定的，也是 `MakeTexturedQuad` 之外那些无纹理用例依赖的。
- **不改**导入层。缺贴图仍可导入并被标记为降级，宿主据此提示——
  契约管的是 ViewData 的渲染响应，不是导入是否允许。
- **不改**错误码集合、通道语义、包字节。
- **不动** three_d 侧已有的 `textureContractClosed` 判定。

## Consequences

**好的**：仓内两条互相矛盾的测试收敛为一条口径，且该口径就是契约原文；
改动面从六处降到两处；不引入新的枚举值、错误码或契约钉。

**要承担的**：改写了一条**当前是绿的**测试的断言。这是本决策里最重的一笔——
把绿灯改成「测别的东西」通常是危险信号。此处的正当性在于：
它断言的行为与契约直接冲突，且从未被授权；不是它测错了实现，是它测了一个错误的期望。

**留下的**：`TexturePolicy` 仍是全仓从未被读的单值枚举（TF-00 附带发现）。
宿主发 `require_if_present`、模块校验它、然后忽略它。本次不动——
修掉 ViewData 层之后它在功能上已无影响，但作为「声明了却不消费」的残留应单独清理。

## Verification

- 定向：`slicer_stage14e04d_dual_view_contract_test` 转绿；
  `textured_scene_viewdata_14b03a_unit_tests` 与 `_real_fixture_tests` 保持绿；
  `hostflow_hb01_model_import` 保持绿（导入层未受影响）。
- 全量回归对照 P0FIX 基线：`slicer_stage14e04d_dual_view_contract_test`
  应**从失败集中消失**，且不得新增任何失败。比失败**集合**而非数量（`AGENTS.md` 8c）。

## Related

- `docs/codex_task/current/TASKS_TEXFAIL_缺贴图静默降级为灰模型修复.md`（TF-00 定位记录）
- `contracts/slicer_capability_dtos.md:220-222`（本决策的依据原文）
- `contracts/slicer_capability_dtos.json:599-600`（`textureRequiredIfPresent` / `noSilentTextureFallback`）
- `analysis/04_问题清单与改动空间.md`（F-45 同属「声明了却不消费」这一族）
