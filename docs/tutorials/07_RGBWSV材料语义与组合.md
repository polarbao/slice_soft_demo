# 07 RGBWSV 材料语义与组合

## 1. 固定协议

当前生产协议不可随普通功能改动：

| 字段 | 固定值 |
|---|---|
| schema | `p0.rgbwsv.2` |
| channel order | `R G B W S V` |
| sample count | 6 |
| bit depth | 8 |
| planar config | contiguous |
| polarity | `black_is_print` |
| print value | 0 |
| empty value | 255 |
| storage | stripped 或 tiled |

`CurrentRgbwsvProtocol()` 是代码中的协议描述入口；writer、manifest、reader、UI 和 tests 必须保持一致。

## 2. 六个通道

| 通道 | 语义 | 常见来源 |
|---|---|---|
| R/G/B | 彩色或 RGB 材料平面 | 纹理、材质 diffuse、fallback、模型默认色 |
| W | 白墨/白色模型填充 | underbase、all_model、model fill |
| S | 支撑材料 | bottom/unsupported/internal void/upper 等支撑 |
| V | 光油/透明材料 | model fill、top layers、surface/outer varnish |

RGB 通道不是普通显示器图像。因为协议采用 `black_is_print`，查看原始像素时必须结合极性；UI 为人眼显示时会做可见化或伪彩。

## 3. 值与“打印量”

协议只明确：

~~~text
0   = print
255 = empty
~~~

不要自行推断 128 就等价于某个已校准物理墨量。中间值如何被下游 RIP/设备解释，需要独立契约和实测。本项目当前的核心验收主要围绕通道、极性、像素统计和材料语义。

## 4. 材料来源的三个层次

### 输入角色

`MaterialRoleMapping` 将输入材质名映射到：

~~~text
rgb / white / varnish / ignore / support_candidate / support
~~~

`role=support` 只有在 `allowInputSupportMaterial=true` 时允许。

### 材料策略

`MaterialPolicy` 决定 RGB/White/Varnish 是否启用、覆盖模式和值，并固定 conflict policy。

### 工艺 Profile

`MaterialProcessProfile` 面向具体打印方案，描述：

- RGB 来源；
- 白墨 mode/coverage/expand/shrink；
- 光油 mode/coverage/top layers；
- 是否期望支撑；
- 结果必须包含哪些通道像素；
- 最大意外重叠数量。

这三个层次不能互相替代：输入“是什么”、策略“怎么分配”、Profile“面向什么工艺并如何验收”。

## 5. Model Fill

`modelFill` 明确纹理表层以下用何种材料填充：

- `material=white|varnish|rgb`；
- `scope=below_texture_surface` 或 12E 目标的 complementary scope；
- `value`；
- 是否允许生产域为空；
- 是否允许 legacy RGB fallback。

模型内部默认留空在视觉上可能不明显，但在制造上可能造成结构缺失，因此 `emptyAllowedInProduction=false` 是重要安全语义。

## 6. 优先级

当前报告固化的总体语义优先级为：

~~~text
Model > OuterVarnishShell > Support > Empty
~~~

模型内具体 RGB/W/V 由材料策略继续组合。支撑与模型重叠时模型优先；外侧光油与支撑重叠时按显式策略清理支撑。优先级必须：

- 在 composer/policy 层实现；
- 在 semantic sidecar 中可解释；
- 在报告中可审计；
- 用重叠 fixture 验证。

## 7. SupportType 不进入 TIFF

`SupportType` 可区分：

~~~text
bottom_projection
unsupported_island
full_vertical_projection
internal_void
upper_projection
~~~

但这些原因只写 report/debug map。S 通道仍只是“此像素是否使用支撑材料”的平面，不能用不同灰度偷偷编码原因，否则会破坏下游契约。

## 8. 单材料与多材料

单材料不代表可以绕过六通道协议。即使只打印 W 或 V，输出仍保持六通道顺序，未使用通道写 empty value。这样 RIP reader、UI 和下游接口保持统一。

## 9. 检查一层材料是否合理

1. layerIndex/zMm 是否正确；
2. 外部背景是否六通道 255；
3. 模型域是否至少有允许的 RGB/W/V；
4. 支撑是否只在预期域写 S；
5. outer varnish 是否符合扩张与冲突策略；
6. 通道顺序和极性是否固定；
7. semantic counts 与 TIFF stats 是否能对应；
8. closure 是否 exact，是否还有 remaining gap；
9. preview 是否只是显示，不被当作生产证据。
