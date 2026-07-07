# REPORT_12A_彩色纹理材料支撑光油当前状态

生成日期：2026-07-07

## 1. 阶段目标

12A 阶段用于把彩色纹理模型、单材料模型、模型内部填充、模型外支撑、表面光油和外侧光油壳层统一到可解释的 RGBWSV 语义链路中。

本阶段不改变 RGBWSV 基础协议：通道顺序仍为 R G B W S V，位深为 uint8，极性为 black_is_print，0 表示打印，255 表示不打印。

## 2. 当前已完成成果

1. 配置语义已补齐：
   - `modelFill.material/scope/emptyAllowedInProduction`；
   - `support.placement/internalVoid/upper`；
   - `outerVarnish.enabled/thicknessMm/thicknessStepMm/pixelPitchUm/allowXYExpansion/conflictPolicy`；
   - 表面光油与外侧光油壳层在语义上区分。

2. 材料优先级已固化：
   - 默认语义优先级为 `Model > OuterVarnishShell > Support > Empty`；
   - 支撑与模型材料冲突时保留模型；
   - 支撑与外侧光油壳层冲突时保留外侧光油壳层，并在 report 中记录清除统计。

3. 模型内部填充已落地：
   - 彩色纹理表面写 RGB；
   - 非表面实体区域按 `modelFill.material` 写白墨、光油或 RGB；
   - 生产 Profile 不允许内部填充为空。

4. 支撑策略已扩展：
   - 支持 lower / upper / both / full_vertical_projection 等模式；
   - 内部镂空区域可按 internal void support 填支撑；
   - 上表面支撑在启用外侧光油时可使用 `model_envelope_plus_outer_varnish_shell` 作为外侧边界。

5. 光油策略已扩展：
   - `outerVarnish` 支持按 mm 配置厚度，并按 pixel pitch 换算为像素厚度；
   - 表面光油支持 outer / inner surface 统计；
   - `surfaceVarnish` 与 `outerVarnishShell` 在 report 中分开统计，避免混淆。

6. 报告与验证已增强：
   - `slice_report.layers[].semantic` 输出 texture surface、model fill、support、internal void support、outer varnish、surface varnish 等计数；
   - `slice_report.totals.materialSemantics` 输出 sourcePolicy；
   - `cross_section_material_stack_report.json` 固化真实横截面材料栈摘要；
   - 单材料与彩色纹理一致性验证脚本已覆盖多个 `model/obj` 真实模型。

7. UI 可解释性已完成：
   - LayerPreview 状态栏显示当前层 semantic 与 sourcePolicy；
   - LayerPreview 生产 RGB 像素探针显示 RGBWSV、semantic、sourcePolicy；
   - OverlayPreview 状态栏显示当前层语义摘要和全局 sourcePolicy；
   - UI smoke 已覆盖 semantic/sourcePolicy 显示。

## 3. 新增真实模型样例

`model/obj/meigui_fudiao` 已作为真实 OBJ 样例纳入仓库，包含：

```text
model/obj/meigui_fudiao/04.obj
model/obj/meigui_fudiao/04.mtl
model/obj/meigui_fudiao/zhongzhi1(4).png
```

该模型可用于后续真实浮雕、真实纹理和复杂指甲模型切片验证。

## 4. 当前验证记录

12A-12 本次验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_layer_preview.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeLayerPreview
```

结果：

```text
PASS layer-preview-load layers=25 channels=production_rgb,rgb,white,support,varnish,occupancy,diagnostic
PASS overlay-load-real images=47 channels=rgb,support,varnish,white modes=RGB + W 白墨,RGB + V 光油,RGB + S 支撑
```

历史 12A 任务记录显示：

```text
12A-10 已生成并验证真实横截面材料栈摘要；
12A-11 已完成彩色纹理与单材料一致性 fixture；
12A-11 扩展验证已覆盖 model/obj 下多种真实 OBJ；
meigui_fudiao 曾通过 ExtraModelPath 临时验证，当前已提交为仓库真实模型样例。
```

## 5. 与设计目标的符合情况

当前 12A 实现总体符合 PRD / DEV 中对彩色纹理材料、内部填充、支撑、光油和 UI 可解释性的设计：

```text
表面颜色层：已支持 RGB 纹理/颜色输出。
模型内部填充层：已支持按 modelFill.material 写入 W/V/RGB。
模型外支撑：已支持 lower / upper / both / internal void / full vertical 等支撑来源统计。
内部镂空支撑：已支持并输出 internalVoidSupportPixels。
外侧光油壳层：已支持按 mm 配置厚度并允许 XY 扩张。
语义报告：已输出 layer/totals semantic 与 sourcePolicy。
UI 检查：已能在 LayerPreview / OverlayPreview 查看语义和策略来源。
单材料一致性：已有一致性验证脚本和真实模型矩阵。
```

## 6. 后续真实模型切片前的注意事项

1. UI 像素探针的 `semantic` 是根据 RGBWSV 通道值与 report 策略推断的解释结果；当前 TIFF 未保存逐像素 source mask，因此 V 通道像素在部分情况下只能解释为光油相关语义，而不能 100% 区分每个像素来自表面光油、外侧光油或光油填充。

2. `model/obj/meigui_fudiao` 已提交，但尚未加入 committed golden 的固定矩阵。后续如果要作为正式验收样例，建议新增专门 fixture 或把它纳入 12A/真实模型回归脚本的长期 golden。

3. 当前 12A 生产切片仍基于 legacy/traditional slicing pipeline；OpenVDB 路径仍不应作为默认生产切片引擎。

4. 真实模型切片验收建议继续同时检查：
   - `layers/*.tiff` 的 RGBWSV 六通道；
   - `preview` 的语义叠加显示；
   - `reports/slice_report.json` 的 layer/totals semantic；
   - `reports/cross_section_material_stack_report.json`；
   - 单材料与彩色模型几何轮廓、支撑和通道统计一致性。

## 7. 阶段结论

12A 阶段的 P0/P1 设计目标已经基本完成，可以进入真实模型切片处理与验收验证阶段。

下一步建议以 `model/obj` 下真实模型为主，建立正式的真实模型切片验收矩阵；其中 `meigui_fudiao` 应作为复杂浮雕纹理样例，重点检查表面 RGB、模型内部填充、内部镂空支撑、上下表面支撑和外侧光油壳层的组合行为。
