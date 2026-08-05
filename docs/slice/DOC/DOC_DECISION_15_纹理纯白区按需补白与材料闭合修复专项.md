# DOC_DECISION_15 纹理纯白区按需补白与材料闭合修复专项

> 文档状态：**ACTIVE / DEVELOPMENT READY**（用户于 2026-08-04 授权成立）
> 版本：v1.2 ｜ 决策日期：2026-08-04
> 作者：Claude（分析与起草）；实现由主线开发（codex）接管
> 证据等级：A=已核实代码/运行事实，B=目标设计，P=判断
> 推导过程见 `docs/slice/DOC/DOC_ANALYSIS_14_Q2_RIP白区带内信号与配置冲突审查.md`
> 实施准备审查见 `docs/slice/DOC/DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md`

---

## 1. 一句话结论

**纹理纯白像素（`R=G=B=255`）在 `black_is_print` 下与背景字节完全相同，导致模型所有权像素无任何材料输出、生产包闭合失败。修复方式是对这类像素按需补写白墨 W 通道 —— 该方案不需要修改 `p0.rgbwsv.2` 协议、不需要修改闭合校验、不需要 RIP 侧配合。**

## 2. 触发事实（A · 实测）

用户对 `model/obj/小马物语/小马物语小指` 使用「全实体 RGB」Profile 切片，进度到 `scene_composition percent=72` 后失败，耗时 3113 ms：

```text
slicer_cli scene error: SCENE_PRODUCTION_PACKAGE_INVALID:
  instance RGBWSV bytes do not close against material ownership;
  pixel=38085 values=255,255,255,255,255,255 ownership=1,0,0
SCENE_SLICE_PROCESS_FAILED: 当前场景切片进程失败，退出码=2
```

解读：

| 证据 | 含义 |
|---|---|
| `values=255,255,255,255,255,255` | 六通道全空 —— 与背景像素字节完全相同 |
| `ownership=1,0,0` | 模型所有权=1，即该像素**属于模型**，却没有任何材料 |
| 报错点 | `SceneLayerComposer.cpp:462` `SourcePixelHasClosure()` |

**这是 fail-closed，不是静默产废件。** 现有保护有效，本专项不是修 bug，而是**补一项当前物理上无法表达的能力**。

## 3. 根因（A）

`black_is_print` 语义下 `255 = 无墨`。「全实体 RGB」Profile 只写 R/G/B 三通道，因此：

```text
纹理采样 = (255,255,255)  →  RGB 三通道全部写 255  →  该模型像素零墨量
                          →  与 background.value=255 撞码  →  闭合失败
```

**这是语义上的必然，不是实现疏漏。** 该 Profile 的说明文字本身已写明此边界：

> 「模型只写 RGB，不写 W/V；black_is_print 下无法表达需要打印的纯白像素，含白色纹理请改用白墨或光油填充 Profile。」

代码侧三条佐证：

| 位置 | 事实 |
|---|---|
| `slicer.cpp:3059-3072` | legacy texture 分支只写 `base+0/1/2`，从不写 `base+3`(W) |
| `slicer.cpp:2583-2601` `ShouldApplyModelFill` | `material == Rgb && textureSurfacePixel` → 返回 false，不补填充 |
| `EffectiveConfigGenerator.cpp:529-548` | `NormalizeModelFillTextureContract` 在 `fillMaterial == "rgb"` 时提前 return false —— **既不自动纠正也不告警** |

第三条是本次事故体验最差的一环：用户选了一个对该模型必然失败的组合，系统在切片前**完全没有提示**。

## 4. 关键发现：协议层已经允许 RGB 与 W 共存（A · 决定性）

`SceneLayerComposer.cpp:250-291` Model 分支的闭合判据：

```cpp
if (channel < kSupportChannel && value != protocol.empty_value) {
    modelMaterialPresent = true;
}
return modelMaterialPresent;
```

`kSupportChannel = 4`，故 `channel < 4` 覆盖 **R、G、B、W 四个通道**。规则是**「至少一种材料非空」，不是「恰好一种」**，代码中不存在任何 RGB/W 互斥判断。

| 像素字节 | ownership | 闭合 | 说明 |
|---|---|---|---|
| `255,255,255,255,255,255` | 1,0,0 | ❌ | 当前故障 |
| `255,255,255,**0**,255,255` | 1,0,0 | ✅ | 纯白区补白墨 —— **本方案** |
| `200,100,50,**0**,255,255` | 1,0,0 | ✅ | 彩色 + 白墨打底同样合法 |

**推论：`p0.rgbwsv.2` 协议、`SourcePixelHasClosure`、`RgbwsvPackageWriter`、TIFF 结构、通道顺序/位深/极性 —— 全部不需要改动。** 阻碍仅存在于合成器写入路径与配置 schema。

### 4.1 为什么只开 W、不开 V（A）

| 通道 | 闭合要求 | 改造成本 |
|---|---|---|
| **W (idx 3)** | 归入 `channel < 4` 组，**无需任何 ownership 掩码配合** | 低 |
| **V (idx 5)** | 写非空值时**必须** `modelvarnishownership != 0` 或 `outervarnishownership != 0`，否则 `return false`（:278-283） | 高，跨模块 |

写 V 需同步维护并透传 ownership 掩码（合成器 + 掩码生成 + 场景传递三处）。而 V 对本问题**无必要** —— 白墨才是打白的材料，光油是表面处理。

**决策：本专项只实现 W，明确不实现 V。**

## 5. 阈值取 0 即充分（A · 对早期判断的更正）

早期评估曾担心抗锯齿边缘像素（如 `254,254,254`）同样不可打印却不触发补白。**该担心不成立**：

```cpp
value != protocol.empty_value   // empty_value = 255
```

`254 != 255` → `modelMaterialPresent = true` → **闭合通过**。**只有严格 `255,255,255` 才失败。**

因此：

| 阈值 | 性质 |
|---|---|
| `unprintableWhiteInkThreshold = 0`（严格相等） | **正确性充分且必要**，消除全部闭合失败。**默认值** |
| `> 0` | 纯粹的**印刷质量选项**（近白像素墨量极低，实际可能呈半透明/不均），非正确性要求 |

这把一个原本需要用户拍板的争议参数降级为可选调优项。

## 6. 交付形态决策：新增 Profile，不改动原 Profile（P）

**`obj_mtl_texture_rgb_only.json` 保持原样不动。**

| Profile | 定位 |
|---|---|
| `textured_nail_rgb_only_lower_support`（既有） | 严格 RGB，遇纯白 **fail-closed**。行为自洽、说明文字准确，作为兼容基线保留 |
| `textured_nail_rgb_white_ondemand_lower_support`（**新增**） | 全实体 RGB + 纹理纯白区按需补白 W |

四条理由：

```text
① 现有回归零风险 —— 原 Profile 的 golden 基线不受任何影响；
② 命名保持诚实 —— 「全实体 RGB」写了 W 之后名字就不再准确；
③ 用户显式选择是否补白，而不是被系统悄悄改变材料分布；
④ 便于 A/B 对照验收（同模型两 Profile 跑，差异必须只落在纯白像素）。
```

### 6.1 已落地的配置产物

| 文件 | 状态 |
|---|---|
| `samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json` | ✅ 已创建 |
| `samples/scenarios/slicer_scenarios.json` 新增条目 | ✅ 已注册，**`enabled: false`** |

`enabled: false` 是刻意的：Stage 15 代码未落地前该 Profile 不在 UI 出现，避免用户选到一个会报配置错误的条目。**任务 15E-02 在阶段出口时翻转为 `true`。**

在 G7 实物 Gate 通过前，该条目的 `productionSafety` 固定为 `diagnostic`。只有 G7 通过后，15E-02 才能同时将 `enabled` 翻转为 `true`、将 `productionSafety` 翻转为 `production`。

### 6.2 前向兼容性（A）

新配置文件对 pre-15 构建保持 fail-closed：`texture.unprintableWhite*` 未识别时不会产生补白能力；同时 `materialProcessProfile.white.mode/coverage` 的既有 allowlist 会拒绝新枚举值，因此不会静默伪装为可生产配置。

> ⚠️ 例外：`materialProcessProfile.white.mode = "unprintable_white_underbase"` 是新枚举值。若 `config.cpp` 对 `white.mode` 存在 allowlist 校验，pre-15 构建会在配置校验期抛错。这仍是 fail-closed，可接受。任务 15A-03 负责确认并扩展该 allowlist。

## 7. 契约设计（B）

### 7.1 新增配置字段

置于 `texture` 节点下，与既有 `missingTexturePolicy` / `nonSurfaceRgbPolicy` 同级同风格（扁平字符串策略）：

| 字段 | 类型 | 默认 | 语义 |
|---|---|---|---|
| `texture.unprintableWhitePolicy` | string | `"fail_closed"` | `fail_closed`＝现状行为；`white_underbase`＝补写 W |
| `texture.unprintableWhiteInkThreshold` | uint8 | `0` | 判据 `(255 - min(R,G,B)) <= threshold` |
| `texture.unprintableWhiteValue` | uint8 | `0` | 写入 W 的值；策略启用时范围 `0..254`，不得等于 `emptyValue=255` |

**默认值 `fail_closed` 保证：不显式开启的既有配置行为逐字节不变。**

### 7.2 判定与写入语义

```text
在纹理表面像素写入 RGB 之后：
  ink = 255 - min(R, G, B)
  if policy == white_underbase && ink <= unprintableWhiteInkThreshold:
      pixels[base + 3] = unprintableWhiteValue     // 仅此一处，不触碰 S(4) / V(5)
      ++semantic_stats.unprintable_white_carrier_pixels
```

三条不变量：

```text
① 只写 W(idx 3)，绝不写 S(idx 4) 与 V(idx 5)；
② 不改变 R/G/B 的既有取值 —— 纯白像素仍保持 255,255,255；
③ 不改变任何 ownership 掩码 —— 该像素本来就是 modelownership=1。
```

不变量 ② 值得强调：**补白不是"把白色改成别的颜色"，而是"在保持颜色不变的前提下补上承载它的白墨"**，与真实 UV 打印工艺一致。

这里的 `underbase` 只表示**同一 layer、同一 XY 像素的白墨材料载体**，不新增 Z 层，不是 Stage 13G 的支撑投影铺底，也不是给全部 RGB 像素增加白墨底层。

### 7.3 `materialProcessProfile` 侧（report-only）

`materialProcessProfile` 为**只读报告层，从不写像素**（既有事实 A），故此处改动仅影响报告与校验：

| 字段 | 取值 | 说明 |
|---|---|---|
| `white.enabled` | `true` | |
| `white.mode` | `"unprintable_white_underbase"` | **新枚举值** |
| `white.coverage` | `"texture_unprintable_white"` | **新枚举值** |
| `validation.requireWhitePixels` | **`false`** | 关键：无纯白纹理的模型合法地产生零白墨像素，置 true 会误杀 |

> ⚠️ **不得复用 `mode: "underbase"`**。`slicer.cpp:3732` 对该模式执行
> `missing_underbase_pixels = rgb_print_pixels - white_print_pixels > 0 → E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW`。
> 本方案白墨像素数远小于 RGB 像素数，复用会必然触发误报。新枚举值必须排除在该检查之外。

## 8. 改动清单与风险（A/P）

| # | 位置 | 改动 | 风险 | 任务 |
|---|---|---|---|---|
| 1 | `config.h` `TextureConfig` | 新增 3 字段 | 低（纯新增，默认关） | 15A-01 |
| 2 | `config.cpp` | 解析 + 校验 + 扩展 white.mode/coverage allowlist | 低 | 15A-02/03 |
| 3 | `slicer.cpp:3059-3072` | 纹理写 RGB 后按判据补 `base+3` | **中**（单体内，动 golden） | 15B-01 |
| 4 | 报告与统计 | 新增 `unprintableWhiteCarrierPixels`；`white_print_pixels` 覆盖新路径 | 低 | 15B-02/03 |
| 5 | `TextureWhitePreflightService` / `EffectiveConfigGenerator` | **切片前**保守预检告警（见 §9）；`ConfigValidator` 只校验字段结构 | 低 | 15C-01/02 |
| 6 | Profile JSON + 注册表 | 已完成，出口时翻转 `enabled` | 低 | 15E-02 |

**明确不改**：`p0.rgbwsv.2` 协议定义、`SourcePixelHasClosure`、`RgbwsvPackageWriter`、TIFF 结构、通道顺序/位深/极性、**RIP 侧**。

### 8.1 风险登记

| 编号 | 风险 | 等级 | 缓解 |
|---|---|---|---|
| **R1** | **工艺风险**：纯白区改打白墨，材料分布改变。白墨固化/叠加特性与彩色墨不同 | **高** | 软件不可消除。**出口门强制要求实物打样确认**（15D-05）。这是本专项唯一无法用测试关闭的风险 |
| **R2** | 阈值语义争议 | **低**（已降级） | 见 §5，默认 0 即充分 |
| **R3** | `unexpected_overlap_pixels` 是硬编码 `constexpr 0U`（`slicer.cpp:3711`），从不实际计算 | 中 | **独立立项，不并入本专项**。见 §10 |
| **R4** | golden 基线漂移 | 中 | 新增 Profile 不影响既有 golden；新 Profile 自建基线。**要求逐像素证明差异只落在纯白像素**（15D-02） |
| **R5** | 命名不实 | 低 | 已通过"新增 Profile 而非改造"消解（§6） |

## 9. 顺带修复：切片前预检缺失（A）

本次事故的体验问题独立于补白能力本身：**用户等了 3 秒、跑到 72%，才拿到一条协议层措辞的报错。**

两项改进（任务 15C）：

```text
15C-01  导入/配置生效阶段由独立异步预检服务扫描纹理是否含严格纯白像素；
        若含纯白 且 Profile 能力集不含 unprintable_white_underbase
        → 切片前给出明确的保守告警，指名可用的替代 Profile。

15C-02  EffectiveConfigGenerator 消费预检结果并按当前 Profile 能力映射告警；
        不改变既有 NormalizeModelFillTextureContract 纠正逻辑。

15C-03  为 SCENE_PRODUCTION_PACKAGE_INVALID 补业务层解释：
        当失败像素为全 255 且 ownership.model=1 时，追加
        「该像素位于纯白纹理区域，当前 Profile 无法表达需打印的纯白，
          请改用『全实体 RGB + 按需补白』或白墨填充 Profile」
```

设计先例：`config.cpp:803-810` 已存在同类生产禁令 ——

```cpp
if (model_fill.enabled && !empty_allowed_in_production && !legacy_rgb_fallback
    && material == "rgb" && texture.enabled && non_surface_rgb_policy == "empty") {
    throw std::runtime_error(
        "modelFill production profile cannot use rgb fill with texture.nonSurfaceRgbPolicy=empty");
}
```

项目**早已识别"rgb fill 会产生空像素"这类问题**，只是只堵了 `nonSurfaceRgbPolicy=empty` 一个分支，漏了"纹理本身是纯白"这个分支。本专项补上的是同一族校验的缺失成员，**不是新造概念**。

`ConfigValidator` 当前只有 JSON 上下文，不负责读取 OBJ/MTL/纹理。它仅校验 Stage 15 字段与非法组合。资产扫描由预检服务复用导入后的纹理或 `SceneViewSurfacePreview.contenthash` 缓存；扫描不得阻塞 UI 线程，相同内容采用 single-flight，结果绑定 scene/revision/contentHash 且过期即丢弃。若只能扫描整张源贴图，告警必须标记为保守判断，因为未被 UV 使用的纯白 texel 也可能被统计。告警不替代核心 fail-closed。

## 10. 明确划出范围之外的事项

| 事项 | 处置 |
|---|---|
| `unexpected_overlap_pixels` 恒 0（R3） | **独立缺陷卡，不并入**。一旦改为真算，`13b_07_xiao_ma_legacy.json` 等既有「白墨打底 + 纹理」配置本就是 RGB+W 共存，会突然报 `E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP` 打破绿基线。必须先定义「合法共写」与「意外重叠」的区分规则 |
| V 通道共写 | 明确不做（§4.1） |
| 1-bit 掩膜 sidecar | 已由用户否决（会要求 RIP 重做） |
| RIP 侧任何改动 | 不需要。W=0 本就是合法白墨信号 |
| `p0.rgbwsv.3` | 不涉及 |

## 11. 与既有阶段的关系

| 阶段 | 关系 |
|---|---|
| **Stage 14**（能力包封装，✅ **ACTIVE** 2026-08-04）| Stage 15 已 COMPLETE 并让出优先级，Stage 14 随后激活 |
| **14 · RIP 六问 Q2** | ✅ **已闭合**：RIP 侧选定**路径 D**（废弃 `WSV=000`，改用 `W=0` 真实材料语义），Stage 15 的显式不透明白证据成为该路径的实证。权威条款见 `DOC_DECISION_14_S2_RIP接口合同定案.md` §1.3 |
| 12A | 本专项是 `PRD_12A` 材料策略的增量，不推翻其结论 |
| 12E | Legacy 为目标路径；Global 已有精确白写 W 行为，本阶段只做回归，不改其语义 |
| 13B/13F | 并行不冲突；场景闭合校验保持原样（本方案通过其检查而非绕过） |
| 12G-TCWS | 保持冻结，不涉及 |

## 12. 阶段出口门（G）

```text
G1  新 Profile 对含纯白纹理模型（小马物语小指）切片 PASS，无 SCENE_PRODUCTION_PACKAGE_INVALID
G2  通过纯策略/单层组合 fixture 证明：启用策略相对关闭策略的差异【仅】落在 W 通道、且【仅】在命中判据的纹理像素上；旧 Profile 另行保持 fail-closed
G3  policy=fail_closed（默认）时，全部既有 golden TIFF SHA-256 逐字节不变
G4  无纯白纹理模型的新旧 Profile，manifest layer list 对应 TIFF 文件 SHA-256 一致；Profile/报告元数据允许不同
G5  项目内 `rip_reader_test` strict 读取新包通过，且本仓库 RIP 侧零改动；外部目标 RIP 仍由 G7/集成证据确认
G6  异步切片前预检对「纯白纹理 + 不支持补白的 Profile」给出明确告警；重复扫描命中缓存，过期结果不串线
G7  【实物打样确认白墨区固化与外观可接受】—— 唯一无法自动化的门
G8  scenarios 注册表 enabled 翻转为 true，用户手册与 Profile 说明同步
```

## 13. 关联文档

| 文档 | 作用 |
|---|---|
| `PRD_15_纹理纯白区按需补白与材料闭合修复.md` | 需求与验收标准 |
| `DEV_15_纹理纯白区按需补白设计.md` | 实现契约与精确改动点 |
| `DEMO_15_纹理纯白区按需补白验收方案.md` | 验收用例集 |
| `REPORT_15_纹理纯白区按需补白当前状态.md` | 执行状态追踪 |
| `docs/codex_task/current/TASKS_15_纹理纯白区按需补白任务清单.md` | 任务卡分解 |
| `docs/codex_task/current/CODEX_PROMPT_15_纹理纯白区按需补白执行指令.md` | 执行提示词 |
| `DOC_ANALYSIS_14_Q2_RIP白区带内信号与配置冲突审查.md` | 白区语义推导前史 |
| `DOC_PREP_15_纹理纯白区按需补白实施准备与依赖审查.md` | 实施依赖、范围矩阵、基线与 Gate 修订 |
