# ANALYSIS-01 单材料缩裹：现状、为什么做不到、改动面

> 目录：`docs/plan-feature/单材料缩裹整模识别与整版多实例/` ｜ 版本：v1.1 ｜ 日期：2026-09-20
> 专项：**MONOWRAP**（见 `AGENTS.md`「各专项状态」）
> 索引：[README](README.md)
> 证据等级：本文「实测事实」（§1）与「为什么做不到」（§2）全部为 **A 级**
> （当前代码/二进制实测，可直接作为实施依据），来源在条目内逐条标注。
>
> ⚠ **§4「尚未验证的点」第 1 条已被 DECISION-01 推翻并细化**——
> 当时只说「下游是否依赖 materialName 需先读通」，实际读通后发现
> `MaterialVolumePlan.cpp:144` 对空材质名**直接抛错**，那才是真正的阻碍。
> 以 [DECISION-01](DECISION-01-开工门四问的裁定与依据.md) 的 G-1 一节为准。
> 缘起：用户 2026-09-20 提出两件事——
> ①「如果导入 `\model\obj\alg_suoguo\20260908-HuangChenC` 目录下文件，
> 将模型输出为单缩裹材料，但现有预设工艺中没有对应的工艺选项，能否创建新工艺文件」；
> ②「当前缩裹材料的自动识别是根据 mtl 文件中素材命名进行处理，
> 但在单材料中，如果只想将该模型只识别为缩裹材料则无法正常处理」。
> 任务卡：`TASKS-01-任务清单.md`

---

## 0. 一处先行更正：识别不是按「素材命名」，是按漫反射 RGB

用户表述里的「根据 mtl 文件中素材命名进行处理」**对一半**。
本仓实际有**两条互不相干**的材质路径，缩裹走的是后者：

| 路径 | 入口 | 依据 | 产出 |
|---|---|---|---|
| **按名字** | `map_input_material_to_role`（`src/slicer_core/materials/SliceMaterialTexture.cpp:79`） | `materialRoleMapping.rules[].matchNameContains` 子串匹配，小写化 | `MaterialRole`：**Rgb / White / Varnish / Ignore / SupportCandidate / Support** |
| **按颜色** | `ResolveTransferMaterial`（`src/slicer_core/materials/transfer/TransferMaterialResolver.cpp`） | `transferChannelPolicy.materialDiffuseRgbValues` **精确**匹配漫反射 RGB | T 通道（缩裹）的唯一命中材质 |

**`MaterialRole` 里没有缩裹角色**——按名字那条路径根本到不了 T 通道。
而 `TransferMaterialResolver.h` 的注释写得很明确，这是刻意的设计决定：

> Material names are returned as geometry keys only after colour matching;
> they are never treated as transfer-role configuration.

**所以「改成也支持按名字识别缩裹」不是本专项该走的方向**——那会推翻一条有意为之的约束。
真正缺的是另一样东西，见 §2。

---

## 1. 实测事实

### 1.1 目标目录是纯几何，零材质引用

`model/obj/alg_suoguo/20260908-HuangChenC/`：

| 项 | 实测 |
|---|---|
| 文件构成 | **10 个 `.obj`，0 个 `.mtl`** |
| `mtllib` / `usemtl` 声明 | **一条都没有**（10 个文件全查过） |
| 单件规模 | 约 3 049 ~ 7 029 顶点 / 6 094 ~ 14 054 面 |
| 单件包围盒 | 约 12.1 × 15.4 × 4.46 mm（101）、7.1 × 10.2 × 3.4 mm（205） |
| 排布 | Z 在 19 ~ 24 mm，各件 X/Y 不同——**指甲片托盘排版** |

同目录的另一组样本 `model/obj/alg_suoguo/黄晨晨-算法缩裹/` 同样是 10 个 `.obj`、无 `.mtl`。
**这不是个例，是这类资产的常态。**

### 1.2 几何本身没问题，只有材质是缺的

`slicer_cli --inspect-model` 对 `101_ND002-down05.obj` 实测：

```text
  format: obj
  vertices: 7029
  autoOrient.enabled: true / applied: false / selectedOrientation: identity
  originalBboxMm: [78.8649, 1.50646, 19.1017] - [91.0073, 16.933, 23.5656] height=4.46394mm
  orientedBboxMm: [78.8649, 1.50646, 0] - [91.0073, 16.933, 4.46394] height=4.46394mm
```

**导入、定向、包围盒归零全部正常。** 问题不在几何，在材质表为空。

---

## 2. 为什么现在做不到

T 通道的区域来自「按配置的漫反射 RGB 精确匹配出唯一一个材质」。
模型没有 `.mtl` ⇒ 材质表为空 ⇒ `matches.empty()` ⇒
按 `missingRegion` 要么返回空区域（`allow_empty`）要么抛错（`fail_closed`）。
**无论哪种，T 通道都拿不到内容。**

而配置层把这条路封死了，无法绕开：

| 位置 | 约束 |
|---|---|
| `src/slicer_core/config/TransferChannelConfig.cpp:140` | `matchSource != "material_diffuse_rgb"` 直接抛错 |
| `src/slicer_core/config/TransferChannelConfig.cpp:145` | `materialDiffuseRgbValues` 为空直接抛错 |
| `src/slicer_core/materials/transfer/TransferMaterialResolver.cpp:19` | 同样两条，再拦一次 |

### 2.1 关键：W 与 V 早就有「整模」模式，只有 T 没有

这是本专项的核心认识——**不需要发明新概念，照既有先例补齐即可**。

`samples/configs/matvol_t/process_profiles/nail_varnish_only_rgbwsvt.json` 里：

```json
"materialPolicy": {
  "white":   { "mode": "disabled",  "layers": "all_model" },
  "varnish": { "enabled": true, "mode": "all_model", "value": 0 }
}
```

`materialPolicy.varnish.mode = "all_model"` 表示**整模上光油，不看材质**；
`white.layers = "all_model"` 同理（`src/slicer_core/config.h:87,125` 里
`WhitePolicyConfig::layers` 与 `MaterialProcessWhiteConfig::coverage` 默认都是 `all_model`）。

**W、V 都能「整模应用」，唯独 T 只能「按颜色匹配」。**
用户要的「把整个模型都当缩裹材料」，本质就是 T 通道缺的这个模式。

---

## 3. 改动面

### 3.1 代码（按依赖顺序）

| 文件 | 改动 |
|---|---|
| `src/slicer_core/config.h` | `TransferChannelPolicyConfig` 增加整模模式所需字段（如 `match_source` 允许新值） |
| `src/slicer_core/config/TransferChannelConfig.cpp` | 解析与校验放行新值；新值下不再强制 `materialDiffuseRgbValues` 非空 |
| `src/slicer_core/materials/transfer/TransferMaterialResolver.cpp` | 新值下跳过颜色匹配，直接返回「整模命中」 |
| `src/slicer_core/materials/transfer/TransferMaterialVolumePlan.cpp` | 确认整模命中时体积计划的取值路径成立（**待验证**，见 §4） |
| `src/slicer_core/output/rgbwsvt/RgbwsvtLegacyPackageMetadata.cpp:327,345` | 报告里 `matchSource`、`configuredMaterialDiffuseRgbValues`、`materialName`、`matchedDiffuseRgb` 在整模模式下的取值 |

### 3.2 契约（**风险最高，须先评估**）

`contracts/slicesoft.transfer_channel_report.1.schema.json` 有**四处**会挡住整模模式：

```json
"required": [..., "matchSource", "configuredMaterialDiffuseRgbValues",
             ..., "materialName", "matchedDiffuseRgb", ...],
"matchSource": { "const": "material_diffuse_rgb" },
"configuredMaterialDiffuseRgbValues": { "type": "array", "minItems": 1, ... }
```

- `matchSource` 是 **`const`** 不是 `enum`
- `configuredMaterialDiffuseRgbValues` 有 **`minItems: 1`**，空列表即非法
- `materialName` 与 `matchedDiffuseRgb` 都在 `required` 里，而整模模式下**没有被匹配的材质**

**两种出路，需要裁定**：

1. **同版本放宽**：`const` → `enum`，`minItems` 改为条件约束，
   `materialName` / `matchedDiffuseRgb` 用 JSON Schema 的 `if/then` 按 `matchSource` 条件必填。
   好处是只维护一份 schema；代价是 schema 变复杂。
2. **升版本**：新增 `slicesoft.transfer_channel_report.2`，整模模式产出新版报告。
   好处是旧报告形状一字不动、风险隔离；代价是两份 schema 要同时维护。

### 3.3 工艺文件（用户问题 ①）

新增 `samples/configs/matvol_t/process_profiles/nail_transfer_only_rgbwsvt.json`，
以 `nail_varnish_only_rgbwsvt.json` 为模板，区别在于：
`materialPolicy` 的 rgb/white/varnish 全关，`transferChannelPolicy` 用整模模式。

**注意依赖方向：问题 ① 依赖问题 ②。** 在 §3.1/§3.2 落地之前，
即使把工艺文件写出来，跑这批模型也只会得到空的 T 通道——
因为没有 `.mtl` 可供颜色匹配。**不能先交工艺文件当作已解决。**

---

## 4. 尚未验证的点（开工前须先确认）

1. **整模命中时体积计划是否成立**。`TransferMaterialVolumePlan` 目前拿
   `TransferMaterialMatch{present, materialName, diffuseRgb}` 去求体积区间；
   整模模式下 `materialName` 为空、`diffuseRgb` 无意义，
   **下游是否依赖这两个字段做几何筛选，必须先读通再动手**。
2. **字节级基线是否受影响**。`scripts/SliceOutputBaseline.json` 引用了该报告 schema。
   预期是：既有工艺仍用 `material_diffuse_rgb`、报告形状不变 ⇒ 基线不受影响；
   但这是**推断，未实测**，开工第一步就要验。
3. **10 个 obj 是一次整体切片还是逐件**。目标目录是托盘排版，
   需确认走多模型场景（`--scene-config`）还是单模型逐个跑，这决定工艺文件的形状。
4. **T 通道的 `value` 语义**。现有工艺里 `transferChannelPolicy.value` 恒为 0，
   且 schema 里也是 `"value": { "const": 0 }`。整模模式下这个值该取什么，需确认。

---

## 4.5 UI 侧的架构约束（2026-09-20 实测，A 级）

**查「UI 里能不能选到缩裹工艺」时发现的，它决定了这件事是多大的活。**

`<程序目录>/configs/material_process/` 下的 `*_rgbwsvt.json`
**不是逐个可选的 UI 工艺选项**。`HostProcessPresetCatalog::Presets()` 的做法是：

```cpp
hosttransferchannelsettings transfer;
if (HostTransferProcessPresetLoader::LoadDeployedPolicy(&transfer, nullptr))
{
    for (int index = 0; index < baseCount; ++index)
        if (presets.at(index).transfereligible)
            AppendTransferPreset(presets.at(index), transfer, &presets);
}
```

即**从部署目录派生出唯一一条 T 策略**，再给每个 `transfereligible` 的基础工艺
追加一个 T 变体。而 `LoadDeployedPolicy` 是**取目录里第一个能加载成功的**：

```cpp
for (const QString& fileName : DeployedProfileFileNames())
    if (Load(fileName, transferSettings, nullptr)) return true;
```

所以「哪一份当选」依赖文件名排序。`matvol_t_host_profile` 有一条断言正是为此而设：
**部署的所有 T 工艺 `transferChannelPolicy` 必须完全一致**，否则报
「部署 T 工艺副本漂移……宿主派生所取策略将随目录排序而变」。

**这意味着**：往该目录放一份 `whole_model` 工艺**不会多出一个 UI 选项**，
只会破坏「单一策略」这个不变量并让上述断言变红（实测确认）。

**所以本专项的工艺文件放在 `samples/configs/matvol_t/`（与
`transfer_rgbwsvt_prototype.json` 同级），不进 `process_profiles/`**——
该目录是整目录自动部署的（`apps/slicer_ui_host_sim/CMakeLists.txt:310` 等四处 `copy_directory`）。
CLI 用 `--config` 指定它即可，已实测可用。

**要让 UI 里能选缩裹，需要的是宿主侧改造**，不是加一个配置文件：
让 T 变体可以来自多条策略（按工艺选择），而不是全局派生一条。
`HostProcessPresetCatalog.cpp` 的注释明确写着这类改动
**「改这些需另行授权」**，故不在本专项擅自进行。

---

## 5. 与既有专项的边界

- **MATVOL_T** 是 RGBWSVT 缩裹材料通道专项，本专项改的正是它的配置面与报告契约。
  改动须与其任务卡对齐，**不在 merge 里替它承接未完成项**。
- **MATOPQ**（材质不透明度识别与光油通道映射）同样碰 `materialPolicy`，
  改 `config.h` 时注意不要与其在制品冲突。
