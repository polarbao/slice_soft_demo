# DOC_ANALYSIS_14 Q2 RIP 白区带内信号与配置冲突审查

> 文档状态：⛔ **SUPERSEDED / 推导过程档案**（2026-08-04）
> 版本：v1.6 ｜ 日期：2026-08-04
> 适用范围：固定六通道 `R/G/B/W/S/V`、`uint8`、`black_is_print` 的切片包与既有 RIP 白区处理
> 结论边界：本文只审查现状、碰撞范围和可选迁移路径，不授权修改 `p0.rgbwsv.2`、切片代码或 RIP
>
> ⛔ **Q2 已闭合。** RIP 侧于 2026-08-04 第二轮回复中选定**路径 D**（废弃 `WSV=000` 私有信号，
> 统一改用 `W=0` 真实材料语义）。本文 §2.1（`WSV=000` 结构性不可达论证）、§2.1.4（两条保护建议）、
> §5、§6（路径比选）**仅作历史推导记录**，不再具有实施意义 —— §2.1.4 的两条保护**明确禁止实现**。
> **权威条款见 `DOC_DECISION_14_S2_RIP接口合同定案.md`；实施只看该文。**

---

## 1. 本次审查结论

用户已明确：当前阶段不新增逐层 1-bit sidecar。既有 RIP 基本链路已经完成，新增文件会扩大
RIP 输入、包结构、校验、缓存和联调范围。因此 Stage 14 的 Q2 不再把 sidecar 作为本轮推荐方案。

对候选白区信号：

```text
R/G/B/W/S/V = 0/0/0/255/255/255
```

当前结论仍是：**不能把它直接定义为全局白区哨兵**。原因不是“只有 5 份配置不方便”，而是该值在
现有协议中本来就是合法的“RGB 纯黑打印，W/S/V 不打印”像素。即使删完目前所有冲突配置，OBJ
贴图、MTL 漫反射色、3MF ColorGroup、Texture2D 和未来用户输入仍可合法产生它。

本轮对 109 份 `samples/configs/**/*.json` 的完整审计结果为：

| 证据 | 数量 | 含义 |
|---|---:|---|
| 明确配置黑色模型基线 | **59** | `modelMaterial.rgb=[0,0,0]` 且 W/V 空值为 255，可把普通模型像素写成候选值 |
| 启用纹理且有效 fallback 为黑色 | **32** | 缺纹理、无 UV 或纹理转移回退时可产生候选值；与上项有重叠 |
| 场景索引引用上述配置 | **23 / 30** | 影响不只在孤立测试 JSON，还进入 UI 场景、生产检查和回归入口 |
| 含可见纯黑像素的现有贴图 | **15 / 39** | 即使修改配置，真实输入贴图仍能产生候选值 |

原文中的“5 份配置”只是早期抽样下限，不是完整影响范围。

---

## 2. 为什么这个字节值天然碰撞

`p0.rgbwsv.2` 固定：

```text
0   = 打印
255 = 不打印
通道顺序 = R G B W S V
```

所以候选值按当前生产协议的唯一自解释含义是：

```text
R=0, G=0, B=0       -> RGB 三通道打印，形成纯黑 RGB 材料像素
W=255, S=255, V=255 -> 白墨、支撑、光油均不打印
```

它不是协议保留值，也不是非法值。当前模型 ownership 闭环只要求模型像素存在合法模型材料输出；
RGB 非空即可通过。因此 Reader、Writer 或材料闭环无法只凭这 6 个字节判断它究竟是“真实黑色”还是
“RIP 私有白区”。

配置文件只能改变默认材料和回退值，不能禁止贴图或 3MF 颜色资源包含纯黑。由此得到一个重要结论：

> **删除配置不能建立协议级保留值。只有在代码和 RIP 合同中执行全输入源转义，才能真正保留该值。**

---

## 2.1 反向验证：既有 `W/S/V = 0/0/0` 在当前 composer 中【结构性不可达】（v1.1 新增）

上文说明了候选值 `0/0/0/255/255/255` 为何天然碰撞。本节补一个方向相反、且对决策更关键的结论：
**既有 RIP 使用的 `W/S/V = 0/0/0`，在当前 `compose_layer` 的分支结构下是不可达的**。

### 2.1.1 S 通道（`base + 4U`）的全部写入点（A 级，已核实）

全仓库 `slicer.cpp` 中对 S 通道的写入只有三处：

| 位置 | 上下文 | 写入值 |
|---|---|---|
| `slicer.cpp:2436` | `write_model_pixel()`（legacy 模型材料路径）| `= config.background.value` → **255**（`background.value` 被校验强制为 255）|
| `slicer.cpp:2839` | `write_material_role_pixel()` 的 `case MaterialRole::Support` | `= 0` |
| `slicer.cpp:3125` | `else if (support.enabled && support_mask != 0)` 分支 | `= config.support.value`（通常 0）|

### 2.1.2 三条互斥性（A 级，已核实）

```text
① 第 3125 处位于 model → outerVarnish → support 的 else-if 链的第三段
   → 只对【非模型像素】写 S；模型像素永不经过该分支

② write_material_pixel()（policy 路径，slicer.cpp:2798-2802）写 base+0,1,2,3,5
   → 【完全跳过 base+4】，S 保持初始化值 255

③ write_material_role_pixel()（slicer.cpp:2826-2845）是 switch，
   每个 case 只写一个通道后立即 return：
     White   → 仅 base+3 = 0
     Varnish → 仅 base+5 = 0
     Support → 仅 base+4 = 0
   → 一个像素只承载一个 role，【不可能同时得到 W=0 与 S=0】
```

### 2.1.3 结论

要产生 `W/S/V = 0/0/0`，需要同一像素上白墨、支撑、光油三通道同时为 0。而：

- 模型像素要拿到 `S=0`，唯一路径是 `role=Support`；但该 case 只写 S 就返回，W 与 V 仍为 255；
- 走 policy 路径可同时得到 `W=0` 与 `V=0`，但该函数根本不写 S，S 必为 255；
- 走 legacy 路径时 S 被**显式赋为 255**；
- 非模型像素可得 `S=0`，但其 W 恒为 255（未被任何分支写过）。

> **因此 `W/S/V = 0/0/0` 在当前生产 composer 中不可达。既有 RIP 白区信号是"由构造保证安全"，而不是碰巧没撞上。**

补充说明：材料闭环 1px 修复（`material/MaterialClosureRepair.cpp`）只写入**空隙像素**（`layerEmpty && expectedOccupiedDomain`），这类像素 W 亦为 255，不改变上述结论。

### 2.1.4 ⛔【已作废 · 2026-08-04】原路径 A 配套保护建议

> **本小节的两条保护建议已全部作废，禁止实现。**
>
> RIP 侧于 2026-08-04 第二轮回复中选定**路径 D**（废弃 `WSV=000` 私有信号，
> 统一改用 `W=0` 真实材料语义），本小节的「保护一 Writer 断言」与
> 「保护二 manifest `ripBoundIntermediate`」是**路径 A 的配套设计**，随路径 A 一同失效。
>
> 路径 D 下根本不存在哨兵，无需任何保护机制。若实现它们，
> 会把一个已废弃的概念固化进 manifest。
>
> **权威条款见 `DOC_DECISION_14_S2_RIP接口合同定案.md` §1.3 与 §4。**
> 以下内容仅作为历史推导记录保留。

---

当前的不可达性是**分支结构的涌现性质**，没有任何测试或断言守护它。一次看似无害的重构（例如让 role 支持组合、或让 policy 路径也写 S）就会静默破坏它，而 RIP 侧不会收到任何信号。

因此建议**不修改协议字节布局，只补两条保护**：

```text
保护一（Writer 断言）：
  在 RGBWSV 写出前扫描每层，若发现任一像素满足 W==0 && S==0 && V==0
  → 视为 P0 缺陷，fail-closed 报 PM-SLICER-CONTRACT-0060，不允许发布该包
  代价：一次逐层扫描（可与既有通道统计扫描合并，零额外遍历）

保护二（manifest 显式声明）：
  在 manifest 增加只读声明字段（不改通道布局、不改像素语义）：
    "ripBoundIntermediate": { "whiteRegionSentinel": "WSV=000",
                              "contractId": "rip_bound_intermediate.1",
                              "guaranteedUnreachableByComposer": true }
  作用：把今天的隐式约定变成显式、可版本化、可被 RIP 校验的合同
```

这样既满足"本轮不新增 sidecar、不重做 RIP"的约束，又把隐式耦合转成受保护的显式合同。

**对 Q2 路径判断的影响**：路径 A（兼容既有 `WSV=000`）应从"待 RIP 证据"上调为**推荐路径**；路径 C（RGB 黑哨兵）维持 NO-GO；路径 B（W-only）降为备选（其 `255/255/255/0/255/255` 虽也合法可达，但至少不与"纯黑纹理"这一高频内容碰撞——仍需 RIP 侧确认是否够用）。

---

## 2.2 用户提出的两种"语义白"编码假设：逐一验算（v1.2 新增）

用户提出两种在固定六通道内表达"语义白"的假设。按 `black_is_print`（0=打印 / 255=不打印）逐字节解码如下。

### 2.2.1 假设 A：白 = `255/255/255/0/0/0`，黑 = `0/0/0/0/0/0`

**解码**：

| 值 | 物理含义 |
|---|---|
| 白 `255/255/255/0/0/0` | 无彩色墨 + **白墨打印 + 支撑打印 + 光油打印** |
| 黑 `0/0/0/0/0/0` | 彩色满墨 + **白墨打印 + 支撑打印 + 光油打印** |

**判定：技术上可行，但有三个代价，不建议作为首选。**

| # | 代价 | 说明 |
|---|---|---|
| 1 | **两者都含 `WSV=000`** | 该模式实际是"用 `WSV=000` 作【纹理区标记】、RGB 携带颜色"，而非"白区标记"。RIP 必须靠 RGB 再判断白/黑，标记本身不区分二者 |
| 2 | 🔴 **S=0、V=0 是真实材料指令** | 每个纹理像素都在声明"这里要打支撑和光油"。一旦 RIP 未在物理量化前完全剥离，会在**整个纹理面**喷支撑与光油。这是本方案最大风险 |
| 3 | **占用 W 通道的真实语义** | W=0 被并入标记后，无法再表达"这个纹理像素需要白墨打底"——真实白墨需求与标记不可区分 |
| 4 | 需要新增写出逻辑 | `WSV=000` 当前**结构性不可达**（§2.1）。采用此方案意味着要让 composer **主动写出**它，同时 §2.1.4 建议的 Writer 断言必须反向改造 |

### 2.2.2 假设 B：白 = `255/255/255/255/255/255`，黑 = `0/0/0/255/255/255`

**判定：❌ 不可行。存在一处硬冲突。**

```text
白 255/255/255/255/255/255 = 六通道全不打印
背景（模型外区域）          = 六通道全 255   ← 完全相同
```

后果有三：

| # | 后果 |
|---|---|
| 1 | **白区与"模型外空白"字节完全相同**，RIP 无法区分"这里是语义白"与"这里没有模型" |
| 2 | **会被材料闭环判为缺陷**：模型区域内出现全空像素，属 `expectedOccupiedDomain` 内的空隙，当前默认 `failOnGap=true` 会阻断出包 |
| 3 | 黑色部分 `0/0/0/255/255/255` 恰是当前**正常黑色纹理**的输出——即该假设只定义了白，没有为白提供任何可区分性 |

> 顺带说明：`ModelFillConfig` 有 `empty_allowed_in_production`（默认 `false`）可放开第 2 条，但放开后第 1 条依然无解——**字节相同就是字节相同**。

### 2.2.3 推荐方案：不用任何哨兵，用真实材料语义表达（P）

上述两个假设的共同问题是**试图用"非材料含义的字节组合"承载语义**。但"白"这件事，在 RGBWSV 里本来就有物理正解——区别只在于**白由什么材料构成**：

| 语义 | 六通道值 | 物理含义 | 与其他值冲突？ |
|---|---|---|---|
| **不透明白** | `255/255/255/`**`0`**`/255/255` | 无彩色墨 + **白墨打底** | ✅ 无冲突 |
| **透明白（清漆）** | `255/255/255/255/255/`**`0`** | 无彩色墨、无白墨 + **仅光油** | ✅ 无冲突 |
| **透明白（真空）** | `255/255/255/255/255/255` | 完全无材料 | ⚠️ 与背景同值（物理上本就等价）|
| 黑色纹理 | `0/0/0/255/255/255` | 彩色满墨 | ✅ 当前正常行为，不受影响 |
| 既有 RIP 标记 | `?/?/?/0/0/0` | —— | ✅ 仍保持不可达（§2.1）|

**这个方案的三条优点**：

```text
① 零哨兵：每个字节都是真实材料指令，不存在"需要被剥离的假值"
② 零碰撞：不透明白（W=0）与透明白（V=0）互不相同，也不与纯黑纹理碰撞
③ 零协议改动：不改通道数、顺序、位深、极性；composer 现有能力即可产出
```

**关于"透明白"选哪种**（需工艺确认）：

- 若透明区**实际会上一层清漆**，用 `V=0`（清漆版）——可与背景区分，推荐；
- 若透明区**真的什么都不打印**，那它与"模型外"在物理上本就等价，字节相同并非缺陷。此时需把该区域的 `modelFill.emptyAllowedInProduction` 置为 `true`，否则材料闭环会阻断。

**与用户第 2 点的衔接**：用户提出"透明/不透明由 RIP 阶段按后续定义字段处理"。本方案与之兼容且更稳妥——

```text
若切片侧能确定材料构成 → 直接写 W=0 或 V=0，RIP 无需猜测（信息最完整）
若切片侧无法确定        → 统一写 255/255/255/0/255/255（不透明白为默认）
                          由 RIP Profile 决定是否抑制 W 转为透明
两种做法都不需要哨兵，也不需要 sidecar
```

---

## 2.3 §2.2.3 推荐方案的实测验证（v1.3 新增 · A 级证据）

**2026-08-04：本节推荐的"用真实材料语义表达白"已获实测验证，并据此成立 Stage 15 专项。**
完整方案见 `docs/slice/DOC/DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md`。

### 2.3.1 实测故障（A）

用户对 `model/obj/小马物语/小马物语小指`（大面积纯白 + 红色区）使用
`samples/configs/material_process/obj_mtl_texture_rgb_only.json`（全实体 RGB）切片：

```text
slicer_cli scene error: SCENE_PRODUCTION_PACKAGE_INVALID:
  instance RGBWSV bytes do not close against material ownership;
  pixel=38085 values=255,255,255,255,255,255 ownership=1,0,0
SCENE_SLICE_PROCESS_FAILED: 退出码=2   （scene_composition 72%，耗时 3113ms）
```

这是 §2.2.2 假设 B（白 = 全 255）**致命性的直接实证**：该字节与背景完全相同，
模型所有权像素零材料，闭合校验拒绝出包。**假设 B 就此可以彻底排除，不再是待议选项。**

### 2.3.2 决定性发现：协议层已经允许 RGB 与 W 共存（A）

`SceneLayerComposer.cpp:250-291` Model 所有权分支：

```cpp
if (channel < kSupportChannel && value != protocol.empty_value) {
    modelMaterialPresent = true;
}
return modelMaterialPresent;
```

`kSupportChannel = 4` → `channel < 4` 覆盖 **R、G、B、W**。判据是"**至少一种材料非空**"，
**不是"恰好一种"**，代码中不存在任何 RGB/W 互斥检查。

因此 §2.2.3 表格中的"不透明白" `255/255/255/0/255/255` **不仅在语义上正确，
而且在现有 composer 闭合校验下直接合法** —— 无需修改协议、无需修改闭合规则。

### 2.3.3 W 与 V 的成本不对称（A · 对 §2.2.3 的重要补充）

§2.2.3 将"不透明白（W=0）"与"透明白（V=0）"并列为等价选项。**实测表明二者成本差一个数量级**：

| 通道 | 闭合要求 | 成本 |
|---|---|---|
| **W (idx 3)** | 归入 `channel < 4` 组，**无需任何 ownership 掩码配合** | 低 |
| **V (idx 5)** | 写非空值时**必须** `modelvarnishownership != 0` 或 `outervarnishownership != 0`，否则 `return false`（:278-283） | 高，需跨模块同步维护掩码 |

**结论修正**：对于显式选择“不透明白”的切片 Profile，优先采用 W=0。V=0（清漆透明白）虽语义可行，但需要额外的 ownership 掩码工程，不应作为 Stage 15 首选路径。Stage 15 明确只实现 W，并不替代透明 Profile 的后续产品决策。

### 2.3.4 对 Q2 问法的影响

原 Q2 问的是“RIP 能否接受带内哨兵”。Stage 15 证明了一个更窄的事实：**显式选择不透明白 Profile 时**，可以用真实材料语义 W=0 表达需打印的白，不需要带内哨兵，也不需要本仓库 RIP 改动。

这不等于 Q2 整体作废。12G 仍需回答：同一份全 RGB Package 是否继续由 RIP 在透明与不透明白之间切换，以及现有私有 `WSV=000` 信号如何迁移。

Q2 应追加向 RIP 确认：

> 切片侧对语义白像素输出 `255/255/255/0/255/255`（无彩色墨 + 白墨打底）。
> 请确认 RIP 按**常规白墨通道**处理即可，无需特殊识别逻辑；
> 该确认只适用于显式不透明白 Profile；同包透明/不透明复用仍按原 Q2/12G 单独评审。

### 2.3.5 Stage 15 自动证据回填（15E-03）

项目内统一 Gate 已对显式不透明白 Profile 的 W 载体路径完成以下验证：

```text
F-01 真实小马物语模型：切片 PASS，纯白载体像素 150581，validationFailures=[]；
F-04 纯策略像素差异：仅 W(idx 3) 从 255 变为 0，共 4 个像素；R/G/B/S/V 差异均为 0；
F-02 无严格纯白模型：候选载体像素为 0，新旧生产 TIFF 逐层 SHA-256 等价；
项目内 rip_reader_test --quiet：strict PASS；
既有 golden：28/28 SHA-256 一致，Quick CI PASS；
本仓库 RIP 源码：Stage 15 零改动。
```

证据真源：

```text
output/benchmarks/stage15/stage15_white_carrier_summary.json
output/benchmarks/stage15/pixel_diff_F04.csv
output/benchmarks/stage15/tiff_hash_F02.json
```

因此，**项目内兼容性结论**是：显式不透明白 Profile 可以在冻结的 RGBWSV 六通道协议内，
用 `255/255/255/0/255/255` 表达需打印白色；无需 sidecar、无需哨兵，也无需修改项目内严格
Reader。该证据不等于外部目标 RIP 或实物工艺已验收。G7 仍为 pending，候选 Profile 继续
保持 `enabled=false` / `productionSafety=diagnostic`。

15E-03 只完成 Q2 的证据追加：原 Q2 继续保留，12G 的“同一全 RGB Package 在透明与
不透明白之间复用”继续冻结，不得由本结论自动解冻。

---

## 3. 直接冲突的配置清单

以下 59 份配置明确使用黑色模型基线，并保持 W/V 空值为 255。它们不是都在每次运行中必然产生
候选像素，但其配置语义允许普通模型区域产生该值。

### 3.1 3MF、材料映射与场景

```text
samples/configs/3mf/three_mf_color_group_rgb.json
samples/configs/3mf/three_mf_mixed_color_texture.json
samples/configs/3mf/three_mf_multi_material_deflate.json
samples/configs/3mf/three_mf_multi_material_rgbwv.json
samples/configs/3mf/three_mf_multi_object_transform.json
samples/configs/3mf/three_mf_real_01.json
samples/configs/3mf/three_mf_real_01_support_shape.json
samples/configs/3mf/three_mf_real_02.json
samples/configs/3mf/three_mf_real_03.json
samples/configs/3mf/three_mf_single_rgb.json
samples/configs/3mf/three_mf_single_rgb_deflate.json
samples/configs/3mf/three_mf_single_rgb_stored.json
samples/configs/3mf/three_mf_texture2d_checker.json
samples/configs/material_mapping/obj_mtl_material_mapping_ignore.json
samples/configs/material_mapping/obj_mtl_material_mapping_rgbwv.json
samples/configs/material_mapping/obj_mtl_texture_material_mapping_rgbwv.json
samples/configs/scene/13b_07_texture2d_3mf_legacy.json
samples/configs/scene/13b_07_xiao_ma_legacy.json
samples/configs/scene/13b_07_yecan_legacy.json
```

### 3.2 材料策略与真实生产模板

```text
samples/configs/material_policy/textured_rgb_only.json
samples/configs/material_policy/textured_rgb_varnish_top2.json
samples/configs/material_policy/textured_rgb_white_underbase.json
samples/configs/material_policy/textured_rgb_white_varnish.json
samples/configs/material_process/nail_rgb_white_varnish_top1.json
samples/configs/material_process/nail_rgb_white_varnish_top2.json
samples/configs/material_process/nail_rgb_white_varnish_top2_regression.json
samples/configs/material_process/nail_rgb_white_varnish_top3.json
samples/configs/material_process/nail_varnish_only.json
samples/configs/material_process/nail_white_underbase_only.json
samples/configs/material_process/obj_mtl_texture_rgb_only.json
samples/configs/material_process/obj_mtl_texture_rgb_varnish.json
samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json
samples/configs/material_process/obj_mtl_texture_rgb_white_varnish_regression.json
samples/configs/material_process/three_mf_texture_rgb_white_varnish.json
samples/configs/obj_standard/standard_obj_texture_legacy.json
samples/configs/relief/relief_rgb_gray.json
```

### 3.3 支撑、存储、Golden 与 UI Fixture

```text
samples/configs/golden/material_process_top2_fixture.json
samples/configs/material_closure/real_model_diagnostic_template.json
samples/configs/storage_mode/storage_material_policy_rgbwv_stripped.json
samples/configs/storage_mode/storage_material_policy_rgbwv_tiled.json
samples/configs/storage_mode/storage_stripped_default.json
samples/configs/storage_mode/storage_tiled_compat.json
samples/configs/support/cross_section_material_stack_real_obj.json
samples/configs/support/support_bridge_gap_smoke.json
samples/configs/support/support_internal_void.json
samples/configs/support/support_outer_varnish_shell_1px.json
samples/configs/support/support_outer_varnish_shell_2px_with_support.json
samples/configs/support/support_placement_both.json
samples/configs/support/support_placement_full_vertical_projection.json
samples/configs/support/support_placement_lower.json
samples/configs/support/support_placement_upper.json
samples/configs/support/support_shape_smoke.json
samples/configs/support/support_surface_varnish_outer_inner.json
samples/configs/support/support_upper_surface_outer_varnish_shell.json
samples/configs/textured/textured_missing_texture_fallback.json
samples/configs/textured/textured_no_uv_fallback.json
samples/configs/textured/textured_relief_rgb.json
samples/configs/ui_smoke/ui_layer_preview.json
samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json
```

---

## 4. 黑色纹理回退的第二条碰撞路径

以下 32 份启用纹理的配置，其显式或默认 `fallbackRgb` 为 `[0,0,0]`。其中部分与 §3 重叠，
但碰撞原因不同：即使把 `modelMaterial.rgb` 改走，缺纹理、无 UV 或采样回退仍可产生同一候选值。

```text
samples/configs/3mf/three_mf_mixed_color_texture.json
samples/configs/3mf/three_mf_real_03.json
samples/configs/3mf/three_mf_texture2d_checker.json
samples/configs/golden/material_process_top2_fixture.json
samples/configs/material_closure/real_model_diagnostic_template.json
samples/configs/material_mapping/obj_mtl_texture_material_mapping_rgbwv.json
samples/configs/material_policy/textured_rgb_only.json
samples/configs/material_policy/textured_rgb_varnish_top2.json
samples/configs/material_policy/textured_rgb_white_underbase.json
samples/configs/material_policy/textured_rgb_white_varnish.json
samples/configs/material_process/nail_rgb_white_varnish_top1.json
samples/configs/material_process/nail_rgb_white_varnish_top2.json
samples/configs/material_process/nail_rgb_white_varnish_top2_regression.json
samples/configs/material_process/nail_rgb_white_varnish_top3.json
samples/configs/material_process/obj_mtl_texture_rgb_only.json
samples/configs/material_process/obj_mtl_texture_rgb_varnish.json
samples/configs/material_process/obj_mtl_texture_rgb_white_varnish.json
samples/configs/material_process/obj_mtl_texture_rgb_white_varnish_regression.json
samples/configs/material_process/three_mf_texture_rgb_white_varnish.json
samples/configs/obj_standard/standard_obj_texture_legacy.json
samples/configs/openvdb/surface_shell_nail_3mf_golden.json
samples/configs/openvdb/surface_shell_nail_obj_golden.json
samples/configs/scene/13b_07_texture2d_3mf_legacy.json
samples/configs/scene/13b_07_xiao_ma_legacy.json
samples/configs/scene/13b_07_yecan_legacy.json
samples/configs/storage_mode/storage_material_policy_rgbwv_stripped.json
samples/configs/storage_mode/storage_material_policy_rgbwv_tiled.json
samples/configs/support/cross_section_material_stack_real_obj.json
samples/configs/texture_fill_partition/global_surface_shell_unavailable.json
samples/configs/textured/textured_relief_rgb.json
samples/configs/ui_smoke/ui_layer_preview.json
samples/configs/ui_smoke/ui_overlay_rgbwv_preview.json
```

此外，现有 39 张模型/样例贴图中有 15 张包含 Alpha 非零的精确纯黑像素，包括爱神 5 张、
小马物语模型族及 checker fixture。真实黑色不是异常输入，不能通过调整默认配置彻底排除。

---

## 5. 是否可以替换或删除这些配置

### 5.1 可以治理，但不能用来证明哨兵安全

| 配置类别 | 是否可合并/删除 | 前置条件 | 与哨兵安全的关系 |
|---|---|---|---|
| 生产/UI Profile | 不应直接删除 | 先迁移场景索引、UI 默认、用户文档和既有作业 | 删除后真实黑贴图仍会碰撞 |
| Golden/回归 Fixture | 可在等价覆盖后合并 | 新 Fixture 必须覆盖原 schema、storage、材料闭环和错误用例 | 只能减少 JSON 数量，不能保留字节值 |
| 历史/重复样例 | 可经引用审计后归档 | `rg` 确认无脚本、CTest、文档和 UI 引用 | 不解决输入颜色碰撞 |
| 支撑/材料映射专项配置 | 不建议为白区协议删除 | 它们分别承担独立回归职责 | 与白区方案是不同测试维度 |

现有 30 个场景入口中，23 个引用直接黑基线或黑 fallback 配置。至少 4 个正常用户入口在模板层面受影响：

```text
textured_nail_rgb_white_lower_support
textured_nail_rgb_only_lower_support
textured_nail_rgb_varnish_lower_support
production_rgb_inspection
```

UI 的 effective config 可能覆盖个别材料字段，因此这表示“模板具备碰撞路径”，不表示每次运行都必然
输出候选值。但直接删除这些配置会影响当前用户工作流，不应把配置清理当作协议修复。

### 5.2 若坚持使用 RGB 黑哨兵，必须做的不是删配置

必须建立**代码级保留与转义合同**：

```text
1. 对贴图采样、MTL Kd、3MF ColorGroup、Texture2D、fallbackRgb、modelMaterial.rgb
   和多材料映射的所有合法纯黑统一转义，例如 0/0/0 -> 1/1/1；
2. 只有白区语义生成器可以写 0/0/0/255/255/255；
3. manifest 声明 sentinelContractId/version 和转义规则；
4. Writer 写出前扫描碰撞，Reader/RIP 按版本 fail-closed；
5. 更新颜色校准、Golden、预览与黑阶验收。
```

该方案会牺牲一个 RGB 黑阶值、改变真实黑色，并且仍要求 RIP 理解新合同。相比使用已有 W 通道，
收益不足，当前不推荐。

---

## 6. 不新增 sidecar 前提下的可行路径

### 路径 A：短期兼容既有 RIP 私有 `WSV=000`

适用于“先保持已完成 RIP 基本流程”的最小改动策略，但必须由 RIP 侧提供证据：

```text
packageClass = rip_bound_intermediate；
ripContractId/version 明确；
RIP 在 W/S/V 物理量化前拦截 WSV=000；
S=0 不会泄漏为真实支撑，V=0 不会泄漏为真实光油；
RIP 后输出提供可机读通道统计和像素映射证据；
未识别合同版本时 fail-closed。
```

优点是 RIP 改动最小；缺点是输入包不再是可脱离特定 RIP 自解释的物理 RGBWSV 生产包。
在取得上述证据前，它仍是 **PENDING CONFIRMATION**，不能被文档宣称为协议已闭环。

### 路径 B：固定六通道内显式使用 W，透明/不透明由 RIP Profile 决定

```text
白区像素：R/G/B=255/255/255，W=0，S/V=255；
不透明 Profile：使用 W 通道并量化为白墨；
透明 Profile：抑制 W 通道，按已确认的透明工艺输出；
真实黑色仍保留 0/0/0/255/255/255 的正常含义。
```

该路径保持六通道和单包复用，不需要 sidecar，也不占用 S/V 的物理含义。它需要 RIP 增加或确认
“Profile 可抑制/使用 W”的规则，并需要解冻 12G 后再实现切片白色分色。若白区只由作业级 Profile
区分，这是当前最清晰的目标方向。

### 路径 C：继续采用未转义 RGB 黑哨兵

**NO-GO**。除非接受 §5.2 的全链路保留/转义改造，否则无法区分合法纯黑与白区。

### 能力边界

如果同一层同一作业内必须逐像素混用“透明白”和“不透明白”，而 W 又不能由 Profile 解释，那么固定
六通道且不保留/转义字节值时不存在无歧义表达。该需求只能在后续独立协议决策中解决，不能靠删除
配置规避。

---

## 7. 对 RIP 侧 Q2 应重新确认的内容

本轮不再询问 sidecar。应一次性确认：

```text
1. 当前 RIP 实际识别的完整六通道白区字节值是什么？
2. 该识别发生在 W/S/V 墨滴量化之前还是之后？
3. WSV=000 中 S=0、V=0 是否可能进入真实支撑/光油输出？
4. 同一份包通过透明/不透明 Profile 复用时，逐通道映射表是什么？
5. RIP 是否接受“W=0 表示白区、Profile 决定使用或抑制 W”的六通道方案？
6. 若坚持 0/0/0/255/255/255，是否接受真实黑色全链路转义和版本化合同？
7. 未识别白区合同版本时，是否能 fail-closed 而不是按默认值继续输出？
```

---

## 8. 当前、目标、历史与待确认状态

### Current State

```text
固定六通道与 black_is_print 不变；
sidecar 不进入当前阶段；
现有配置和真实贴图允许产生纯黑 RGB；
切片仓库内未找到足以证明目标 RIP 精确映射顺序的实现证据。
```

### Target State

```text
优先保持固定六通道；
白区不得与合法 RGB、W、S、V 材料像素静默碰撞；
RIP 合同必须版本化、可校验、未知版本 fail-closed；
推荐评估 W=0 + RIP Profile 选择透明/不透明。
```

### Historical State

```text
“5 份配置冲突”是早期抽样结论；
“推荐 per-layer sidecar”已被当前阶段约束取代；
无版本 WSV=000 是已知业务事实，但尚未取得完整逐通道物化证据。
```

### Pending Confirmation

```text
目标 RIP 对 WSV=000 的拦截时序和逐通道结果；
是否接受 W-only 白区输入和 Profile 抑制规则；
同层是否需要逐像素混用透明白与不透明白；
白区合同的 owner、版本和错误处理。
```

---

## 9. 审计复现口径

配置审计范围：`samples/configs/**/*.json`，共 109 份可解析 JSON。

```text
直接冲突：modelMaterial.rgb == [0,0,0]
         且 effective whiteValue == 255
         且 effective varnishValue == 255

fallback 冲突：texture.enabled == true
             且 effective fallbackRgb == [0,0,0]
```

贴图审计范围：`model` 与 `samples/models` 下 PNG/JPG/BMP/TIFF，共 39 张；“可见纯黑”定义为
`A > 0 && R == 0 && G == 0 && B == 0` 至少出现一次。

---

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-03 | v1.0 | 将 Q2 从 sidecar 推荐改为固定六通道约束下的深入审查；完整审计 109 份配置、30 个场景与 39 张贴图；列出 59 份直接冲突配置和 32 份黑 fallback 配置；明确删除配置不能保留协议字节值；给出既有 WSV=000 兼容、W-only Profile 迁移及 RGB 黑哨兵转义三条路径 |
| 2026-08-04 | v1.6 | **路径 D 定案，本文降级为推导过程档案**：RIP 侧第二轮回复选定路径 D（废弃 `WSV=000`，改用 `W=0` 真实材料语义），Q2 闭合；§2.1.4 的保护一（Writer 断言 `PM-SLICER-CONTRACT-0060`）与保护二（manifest `ripBoundIntermediate`）标记为**已作废、禁止实现**；§2.1/§5/§6 降级为历史材料；权威条款移交 `DOC_DECISION_14_S2_RIP接口合同定案.md` |
| 2026-08-04 | v1.3 | 新增 §2.3 实测验证：小马物语小指 + 全实体 RGB 复现闭合失败；查明 W 可作为合法模型材料载体；据此成立 Stage 15 专项 |
| 2026-08-04 | v1.4 | 收紧 Stage 15 与 Q2/12G 边界：Stage 15 只证明显式不透明白 Profile 的 W 载体方案，不作废同包透明/不透明复用问题，不解冻 12G |
| 2026-08-04 | v1.5 | 完成 15E-03：回填 F-01/F-02/F-04、strict Reader、Golden 与 Quick CI 项目内证据；明确 G7、外部目标 RIP 和 12G 仍未关闭 |
