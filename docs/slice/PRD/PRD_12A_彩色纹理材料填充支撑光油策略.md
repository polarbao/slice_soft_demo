# PRD_12A_彩色纹理材料填充支撑光油策略

> 文档版本：v0.4
> 文档状态：PRD / Stage 12A
> 生成日期：2026-07-05
> 更新日期：2026-07-16
> 适用范围：彩色纹理模型、单材料模型、甲片类浮雕模型的模型层、填充层、支撑层、光油层产品语义

---

## 1. 背景

11B 之后，当前 demo 已经具备 OBJ/MTL/PNG、3MF、单材料、RGBWSV 输出、Qt 预览和 OpenVDB candidate 的基础能力。但用户在真实甲片模型切片时看到以下问题：

```text
1. UI 预览中白色、黑色、绿色、光油伪彩含义不够清楚；
2. 彩色纹理模型内部填充、支撑填充、表面纹理三者容易混淆；
3. 不规则浮雕模型的高 Z 局部区域是否需要支撑没有明确业务规则；
4. 中间镂空区域应当如何填支撑未形成稳定策略；
5. 外侧光油壳层只在设计边界中出现，尚未形成可验收产品能力；
6. 单材料和彩色纹理模型虽然共享部分 pipeline，但“效果一致”没有明确指标。
```

Stage 12A 的目标不是先写代码，而是把“每个像素为什么被打印、打印什么材料、属于模型还是支撑”定义清楚。

---

## 2. 产品目标

12A 必须把彩色纹理和单材料切片收敛到同一套材料语义：

```text
1. 每层输出都能区分模型真实数据和支撑数据；
2. 彩色纹理只表示模型表层颜色，不应被支撑或内部填充混淆；
3. 模型内部填充材料默认为白墨，可显式选择光油或后续扩展材料；生产 Profile 不允许内部填充为空；
4. 支撑材料可以按下表面、上表面、上下表面、悬空岛、内部镂空策略生成；
5. 外侧光油壳层按 mm 设置厚度，允许扩张模型 XY 尺寸，并按 42.3um/px 换算到像素；
6. 彩色纹理模型与单材料模型在几何轮廓、支撑生成、层顺序、协议输出上保持一致。
```

---

## 3. 术语定义

### 3.1 Model Real Data

模型真实数据指模型实体自身对应的像素集合，不包含支撑材料。

在 RGBWSV 中，模型真实数据可以写入：

```text
RGB：彩色表面纹理或 RGB 材料；
W：白墨填充、白墨底层或白墨模型材料；
V：光油填充、模型表面光油或模型材料；
```

模型真实数据不包含：

```text
S：支撑材料；
Empty：空白区域；
仅用于 UI 显示的 preview 背景色。
```

### 3.2 Texture Surface Layer

颜色层指模型表层纹理数据。

要求：

```text
1. 来源为 OBJ/MTL/PNG 或 3MF 纹理/颜色信息；
2. 只落在模型表层或配置指定的表面带内；
3. 不作为内部实体填充的默认含义；
4. UI 中应能以 true-color 方式显示；
5. report 中应统计 textureSurfacePixels。
```

补充要求：

```text
1. 色彩层可能出现在模型外表面和模型内表面；
2. 真实 RIP 横截面中可出现“模型表层色彩层 / 模型表层色彩层”两条带状结构；
3. 12A 实现不能只按单一顶面纹理带理解颜色层。
```

### 3.3 Model Fill Layer

模型填充层指模型实体内部、非表面纹理带的材料填充。

产品要求：

```text
1. 填充层属于模型真实数据；
2. 默认不应再隐式等同为 RGB 黑色，除非 Profile 明确选择 legacyRgbFill；
3. 彩色纹理生产 Profile 默认使用 whiteFill，可选择 varnishFill 或后续扩展材料；
4. 单材料模型也使用同一套填充策略，只是颜色来源可能是固定材料而非纹理；
5. 生产 Profile 不允许模型内部填充为空；
6. report 中应统计 modelFillPixels，并标明 fillMaterial。
```

建议枚举：

```text
modelFill.material = white | varnish | rgb | profile_default | material_role
modelFill.scope = solid_volume | below_texture_surface | all_model
modelFill.emptyAllowedInProduction = false
```

说明：

```text
模型内部填充层与支撑填充层不是同一概念。
模型内部填充层位于模型真实数据内部，默认写 W 白墨，也可写 V 光油或其他模型材料。
模型外部填充层只能写 S 支撑材料。
```

### 3.4 Support Fill Layer

支撑填充层指 S 通道支撑材料，不属于模型真实数据。

要求：

```text
1. 支撑只写 S 通道；
2. 模型本体与支撑冲突时保持 Model > Support，即模型像素不被支撑覆盖；
3. 支撑可按下表面、上表面、上下表面、悬空岛、内部镂空策略生成；
4. 支撑材料均为可剥离材料；
5. 默认支撑 placement = lower，只对下表面生成支撑；
6. 上表面支撑是模型外部支撑层，如果启用外侧光油，应生成在外侧光油壳层之外；
7. UI 中支撑应以可配置伪彩显示；
8. report 中应统计 supportPixels、supportPlacement、supportReason。
```

### 3.5 Internal Void Support

内部镂空支撑指在切片层中出现被模型轮廓包围、但当前像素不是模型实体的空洞区域时，根据配置写入 S 通道支撑。

要求：

```text
1. internalVoidSupport 生产默认开启；
2. 不应误填模型外部空白；
3. 必须能在 report 中解释为 internal_void，而不是 bottom_projection；
4. 内部镂空区域一律填充 S 支撑材料；
5. 用户示例 output/ui_sessions/dmz_20260705_003745/package/layers/layer_000169.tiff 可作为问题样例来源，但验收应使用可复现 fixture；
6. 真实横截面材料栈参考 docs/slice/DOC/DIAGRAM_12A_指甲模型横截面材料示意图.png；
7. docs/slice/DOC/DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg 仅保留为优先级概念图，不作为几何验收图。
```

### 3.6 Outer Varnish Shell

外侧光油壳层指模型外轮廓附近新增的 V 通道区域，用于在模型外侧覆盖一层可控厚度光油。

要求：

```text
1. 厚度按 mm 设置，默认厚度为 0mm；
2. 配置精度为 0.01mm；
3. 默认像素物理尺寸为 42.3um，可被 output dpi / pixelPitchUm 覆盖；
4. 外侧壳层不应改变模型 RGB/W/S 的核心语义；
5. 外侧光油层允许扩张模型 XY 尺寸；
6. 默认冲突优先级为 Model > OuterVarnishShell > Support > Empty；
7. 如果同时启用上表面支撑，上表面支撑应生成在外侧光油壳层之外；
8. report 中应统计 outerVarnishPixels、varnishThicknessPx、varnishThicknessMm。
```

### 3.7 Surface Varnish Layer

表面光油层指模型表面或内表面上的 V 通道光油材料带。它与 `OuterVarnishShell` 的区别如下：

| 类型 | 位置 | 是否扩张 XY | 主要用途 |
|---|---|---|---|
| SurfaceVarnishLayer | 模型外表面或内表面上 | 否 | 表面清漆/透明涂层 |
| OuterVarnishShell | 模型外轮廓之外 | 是 | 外侧加厚光油壳层 |

要求：

```text
1. 真实 RIP 横截面中存在“表面层光油”和“模型内表面光油层”两类光油带；
2. 12A 文档和实现不能把所有 V 通道都简化为外侧扩张壳层；
3. SurfaceVarnishLayer 可与外/内表面色彩层相邻；
4. OuterVarnishShell 用于向模型外侧扩张，仍按 thicknessMm 控制。
```

### 3.8 真实 RIP 横截面材料栈

以 `DIAGRAM_12A_指甲模型横截面材料示意图.png` 和真实 `slice.446.png` 为参考，12A 生产语义需要支持以下材料栈：

```text
从模型外侧/上表面向内：
1. 上表面支撑 S
2. 表面层光油 V
3. 模型表层色彩层 RGB
4. 模型内部填充层，默认白墨 W
5. 模型表层色彩层 RGB
6. 模型内表面光油层 V
7. 模型下表面支撑层 S
```

说明：

```text
1. 该材料栈描述的是指甲模型横截面上的带状材料关系，不是画布坐标的上下方向；
2. 上表面支撑和下表面支撑都是模型外部可剥离支撑；
3. 模型内部填充层位于两个表面色彩层之间，生产默认白墨；
4. 支撑可在模型内侧空腔或外侧区域形成连续承托，不能被理解为单个椭圆洞。
```

---

## 4. 用户故事

### US-12A-01 彩色纹理模型输出颜色层与填充层

作为操作人员，我希望导入彩色 OBJ/MTL/PNG 或 3MF 模型后，切片每层能区分表面纹理 RGB 和模型内部填充材料，这样我不会把黑色 RGB 误认为错误或空白。

验收：

```text
1. 表面纹理区域写 RGB；
2. 内部填充区域按 modelFill.material 写 W/V/RGB 或其他模型材料，生产 Profile 不允许为空；
3. 支撑区域只写 S；
4. LayerPreview 像素探针能显示该像素属于 TextureSurface / ModelFill / Support / Empty。
```

### US-12A-02 不规则浮雕模型支撑判定

作为工艺调试人员，我希望 `model/obj/aishen_fudiao` 这类局部高 Z 或不规则浮雕模型能识别悬空区域和需要支撑的区域，避免局部模型层没有支撑。

验收：

```text
1. support_report 标出 unsupported_island 或 high_z_overhang；
2. 支撑可向下投影到可打印底部；
3. 可配置最小岛面积、最小重叠率和 XY 膨胀；
4. 不规则浮雕不会导致支撑被光油或 RGB 填充吞掉。
```

### US-12A-03 中间镂空区域按策略填充支撑

作为用户，我希望甲片中间如果出现被模型外轮廓包围的空洞，可以选择用支撑材料填充，而不是在预览中显示为不可解释白色。

验收：

```text
1. 生产 Profile 默认启用 internalVoidSupport；
2. 中间镂空区域一律写 S；
3. 诊断/回归 Profile 如需关闭，必须在 report 中标明 internalVoidSupport=disabled；
4. UI 显示图例明确区分 Empty 与 Support；
5. 需要与外部空白区分，不能把模型外部空白误填为支撑。
```

### US-12A-04 支撑上下表面策略

作为工艺人员，我希望切片前选择支撑材料填充方式：仅下表面、仅上表面、上下表面、悬空岛、全投影；默认只对下表面填充支撑。

验收：

```text
support.placement = lower | upper | both | unsupported_only | full_vertical_projection
default = lower
```

说明：

```text
1. 当前 demo UI 一键 legacy 曾使用 full_vertical_projection；
2. 12A 生产默认改为 lower；
3. full_vertical_projection 应标记为高级/调试策略；
4. upper 支撑是模型外部可剥离支撑层；
5. 若启用 outerVarnish，上表面支撑必须生成在外侧光油壳层之外。
```

### US-12A-05 外侧光油壳层

作为工艺人员，我希望在模型外侧覆盖一层光油，并能按 mm 设置厚度。默认 1 像素约等于 42.3um，实际像素扩张由厚度换算得到。

验收：

```text
1. 可配置 outerVarnish.enabled；
2. 可配置 thicknessMm，默认 0mm，精度 0.01mm；
3. thicknessMm 与 pixelPitchUm 可互算为 thicknessPx；
4. 输出 V 通道，RGB/W/S 不被错误覆盖；
5. preview/report 均能显示外侧光油壳层；
6. 支持向模型 XY 外侧扩张；
7. 与支撑冲突时执行 Model > OuterVarnishShell > Support > Empty。
```

### US-12A-06 彩色和单材料一致性

作为维护者，我希望彩色纹理模型和单材料模型走同一套几何、支撑、填充、光油策略，避免两条 pipeline 长期分叉。

验收：

```text
1. 同一模型改成单材料 Profile 后，model mask、support mask、layerCount 保持可比较；
2. 几何轮廓、支撑逻辑、层顺序和通道统计逻辑应一致；
3. 差异只来自打印材料：彩色纹理模型为 RGB 颜色，单材料模型为单色材料；
4. slice_report 输出 consistency block。
```

---

## 5. 输出协议要求

12A 不改变生产协议：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

新增语义只能通过配置、report、preview 和内部策略实现，不得改变 TIFF 通道顺序。

---

## 6. 材料优先级

默认优先级：

```text
1. ModelTexture / ModelFill / ModelMaterial
2. OuterVarnishShell
3. SupportFill
4. Empty
```

说明：

```text
1. 模型像素可以同时写 RGB/W/V，取决于 MaterialPolicy；
2. 模型本体与支撑冲突时保持 Model > Support；
3. OuterVarnishShell 允许扩张模型 XY 尺寸；
4. 外侧光油壳层与支撑冲突时 OuterVarnishShell 优先；
5. 上表面支撑应在外侧光油壳层之外生成，避免覆盖光油壳层。
```

---

## 7. 非目标

12A 不做：

```text
1. 不改变 RIP 或设备输出；
2. 不引入半色调；
3. 不把 OpenVDB 设为默认引擎；
4. 不删除历史 samples/configs fixture；
5. 不把 UI preview PNG 当作生产 TIFF 真源；
6. 不承诺所有 3MF 高级材质特性。
```

---

## 8. 已确认默认策略

2026-07-06 已确认：

```text
1. 彩色纹理生产 Profile 的默认模型填充材料是 white；
2. 模型内部填充也可选择 varnish 或后续扩展材料；
3. 生产 Profile 不允许模型内部填充为空；
4. internalVoidSupport 默认开启；
5. 支撑 placement 默认由 full_vertical_projection 改为 lower；
6. 外侧光油壳层默认厚度为 0mm；
7. 外侧光油厚度配置单位是 mm，精度 0.01mm；
8. 默认像素换算为 1px = 42.3um；
9. 光油壳层与支撑冲突时，采用 Model > OuterVarnishShell > Support > Empty；
10. 如果启用上表面支撑，上表面支撑应生成在外侧光油壳层之外；
11. 单材料模型也默认启用同样的 internalVoidSupport；
12. 彩色纹理模型与单材料模型的一致性评价为几何轮廓、支撑、层顺序和通道统计逻辑一致。
```

---

## 9. 阶段完成标准

12A 完成需满足：

```text
1. PRD/DEV/DEMO/TASKS 完整；
2. 配置中存在显式 ModelFill / SupportPlacement / InternalVoidSupport / OuterVarnishShell 语义；
3. 彩色纹理模型与单材料模型均通过 fixture；
4. aishen_fudiao、nai_you_new 至少各有一组可复现验证输出；
5. reports 能解释每层 RGB/W/S/V 的像素来源；
6. UI 可以显示并说明这些策略。
```

---

## 10. 后续阶段 12E 补充关系

12A 已完成范围继续作为当前实现基线；以下新增需求进入 Stage 12E，不回写为 12A 已实现能力：

```text
1. Texture Surface Layer 按完整三维模型的表面距离定义 widthMm；
2. Model Fill Layer 是 ModelVolume - TextureSurfaceVolume 的严格补集；
3. widthMm 从工程最小值连续增加时，纹理区域单调增加、填充区域单调减少；
4. 达到模型动态全纹理阈值后，允许 modelFillPixels=0；
5. 合法 fill=0 必须同时证明 texture=model、overlap=0、unassigned=0；
6. 该能力不能用逐 layer 二维轮廓腐蚀实现；
7. Qt UI 需要新增宽度、动态阈值、覆盖率和 allTexture 状态。
```

12A 的“生产 Profile 不允许内部填充为空”仍用于阻止未分配模型区域。12E 只增加一个严格例外：填充补集因全模型已被纹理分区覆盖而自然为空，而不是通过禁用 `modelFill` 产生空材料。

正式入口：

```text
docs/slice/DOC/DOC_DECISION_12E_全局纹理表面层与模型填充互补策略.md
docs/slice/PRD/PRD_12E_全局纹理表面层与模型填充连续调节.md
docs/slice/DEV/DEV_12E_全局纹理壳层与模型填充分区设计.md
```

---

## 11. Stage 15 纯白纹理增量关系

12A 的默认生产语义保持不变：彩色纹理模型的内部填充材料默认仍为白墨，模型区域不得因
材料未分配而为空。Stage 15 只为 Legacy 全实体 RGB 的一个窄问题增加可选能力：当贴图
像素严格等于 `RGB(255,255,255)`，且所选 Profile 明确声明
`unprintable_white_underbase` 时，在同层同像素写入 `W=0` 作为可打印白色载体。

该增量不等于通用 Model Fill 策略，也不改变以下 12A 规则：

```text
1. 不改变 RGBWSV 六通道、uint8 或 black_is_print 协议；
2. 不写 S/V，不新增 Z 层，不修改支撑或光油优先级；
3. 不把所有模型内部统一改写为白墨，只处理命中的严格纯白纹理像素；
4. fail_closed 仍是默认行为，旧全实体 RGB Profile 保持兼容基线；
5. Profile 只有在实物 G7 通过后才允许从 disabled/diagnostic 翻转为 enabled/production。
```

当前状态：Stage 15 已于 2026-08-04 完成放行，按需补白 Profile 已注册为生产入口；上述
第 5 条继续作为后续同类 Profile 的准入规则。

正式入口：

```text
docs/slice/DOC/DOC_DECISION_15_纹理纯白区按需补白与材料闭合修复专项.md
docs/slice/PRD/PRD_15_纹理纯白区按需补白与材料闭合修复.md
docs/slice/DEV/DEV_15_纹理纯白区按需补白设计.md
docs/slice/REPORT/REPORT_15_纹理纯白区按需补白当前状态.md
```
