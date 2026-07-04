# PRD_12A_彩色纹理材料填充支撑光油策略

> 文档版本：v0.1
> 文档状态：PRD / Stage 12A
> 生成日期：2026-07-05
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
3. 模型内部填充材料可以显式选择白墨、光油、RGB 或不填；
4. 支撑材料可以按下表面、上表面、上下表面、悬空岛、内部镂空策略生成；
5. 外侧光油壳层可按像素或物理尺寸设置厚度；
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

### 3.3 Model Fill Layer

模型填充层指模型实体内部、非表面纹理带的材料填充。

产品要求：

```text
1. 填充层属于模型真实数据；
2. 默认不应再隐式等同为 RGB 黑色，除非 Profile 明确选择 legacyRgbFill；
3. 彩色纹理生产 Profile 应优先提供 whiteFill / varnishFill / none 三种用户可见选择；
4. 单材料模型也使用同一套填充策略，只是颜色来源可能是固定材料而非纹理；
5. report 中应统计 modelFillPixels，并标明 fillMaterial。
```

建议枚举：

```text
modelFill.material = white | varnish | rgb | none | profile_default
modelFill.scope = solid_volume | below_texture_surface | all_model
```

### 3.4 Support Fill Layer

支撑填充层指 S 通道支撑材料，不属于模型真实数据。

要求：

```text
1. 支撑只写 S 通道；
2. 默认仍保持 Model > Support，即模型像素不被支撑覆盖；
3. 支撑可按下表面、上表面、上下表面、悬空岛、内部镂空策略生成；
4. UI 中支撑应以可配置伪彩显示；
5. report 中应统计 supportPixels、supportPlacement、supportReason。
```

### 3.5 Internal Void Support

内部镂空支撑指在切片层中出现被模型轮廓包围、但当前像素不是模型实体的空洞区域时，根据配置写入 S 通道支撑。

要求：

```text
1. 仅在配置启用 internalVoidSupport 时生成；
2. 不应误填模型外部空白；
3. 必须能在 report 中解释为 internal_void，而不是 bottom_projection；
4. 用户示例 output/ui_sessions/dmz_20260705_003745/package/layers/layer_000169.tiff 可作为问题样例来源，但验收应使用可复现 fixture。
```

### 3.6 Outer Varnish Shell

外侧光油壳层指模型外轮廓附近新增的 V 通道区域，用于在模型外侧覆盖一层可控厚度光油。

要求：

```text
1. 厚度可按 pixel 或 mm 设置；
2. 默认像素物理尺寸为 42.3um，可被 output dpi / pixelPitchUm 覆盖；
3. 外侧壳层不应改变模型 RGB/W/S 的核心语义；
4. 默认冲突优先级为 Model > Support > OuterVarnishShell > Empty；
5. report 中应统计 outerVarnishPixels、varnishThicknessPx、varnishThicknessMm。
```

---

## 4. 用户故事

### US-12A-01 彩色纹理模型输出颜色层与填充层

作为操作人员，我希望导入彩色 OBJ/MTL/PNG 或 3MF 模型后，切片每层能区分表面纹理 RGB 和模型内部填充材料，这样我不会把黑色 RGB 误认为错误或空白。

验收：

```text
1. 表面纹理区域写 RGB；
2. 内部填充区域按 modelFill.material 写 W/V/RGB 或空；
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
1. 默认 Profile 可选择是否启用 internalVoidSupport；
2. 启用后中间镂空区域写 S；
3. 禁用后保持 Empty，并在 report 中标明 internalVoidSupport=disabled；
4. UI 显示图例明确区分 Empty 与 Support。
```

### US-12A-04 支撑上下表面策略

作为工艺人员，我希望切片前选择支撑材料填充方式：仅下表面、仅上表面、上下表面、悬空岛、全投影；默认只对下表面填充支撑。

验收：

```text
support.placement = lower | upper | both | unsupported_only | full_vertical_projection
default = lower
```

说明：当前 demo UI 一键 legacy 使用 `full_vertical_projection`，12A 产品默认应重新确认。如果保持 demo 默认，需要在 UI 中明确它是“全竖向投影/调试策略”，不是生产默认。

### US-12A-05 外侧光油壳层

作为工艺人员，我希望在模型外侧覆盖一层光油，并能设置厚度。例如 1 像素约等于 42.3um。

验收：

```text
1. 可配置 outerVarnish.enabled；
2. 可配置 thicknessPx 或 thicknessMm；
3. thicknessMm 与 pixelPitchUm 可互算；
4. 输出 V 通道，RGB/W/S 不被错误覆盖；
5. preview/report 均能显示外侧光油壳层。
```

### US-12A-06 彩色和单材料一致性

作为维护者，我希望彩色纹理模型和单材料模型走同一套几何、支撑、填充、光油策略，避免两条 pipeline 长期分叉。

验收：

```text
1. 同一模型改成单材料 Profile 后，model mask、support mask、layerCount 保持可比较；
2. 差异只来自材料通道策略；
3. slice_report 输出 consistency block。
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
2. SupportFill
3. OuterVarnishShell
4. Empty
```

说明：

```text
1. 模型像素可以同时写 RGB/W/V，取决于 MaterialPolicy；
2. SupportFill 只在非模型像素写 S；
3. OuterVarnishShell 默认不覆盖支撑；
4. 如果后续需要光油覆盖支撑，必须作为单独 Profile 显式启用。
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

## 8. 开放确认项

进入实现前需要确认：

```text
1. 彩色纹理生产 Profile 的默认模型填充材料是 white 还是 profile_default；
2. internalVoidSupport 默认是否开启；
3. 支撑 placement 默认是否由当前 full_vertical_projection 改为 lower；
4. 外侧光油壳层默认厚度是否为 1px；
5. 光油壳层与支撑冲突时是否永远支撑优先；
6. 单材料模型是否也默认启用同样的 internalVoidSupport。
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
