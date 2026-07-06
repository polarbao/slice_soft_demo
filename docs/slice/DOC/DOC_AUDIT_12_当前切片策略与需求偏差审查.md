# DOC_AUDIT_12_当前切片策略与需求偏差审查

> 文档版本：v0.2
> 文档状态：Audit / Stage 12
> 生成日期：2026-07-05
> 更新日期：2026-07-06

---

## 1. 审查目标

本审查回答三个问题：

```text
1. 当前切片策略是什么；
2. 当前实现与正式 PRD / DEV 是否偏离；
3. 用户提出的彩色纹理、填充、支撑、光油、引擎和 UI 问题应如何拆成专项任务。
```

---

## 2. 当前状态

### 2.1 生产协议

当前生产输出协议稳定：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

该部分未发现偏差。

### 2.2 legacy 切片策略

当前 legacy 生产路径：

```text
1. 读取模型和配置；
2. 执行 transform / autoOrient；
3. 生成 grid / layerCount；
4. 根据 relief_heightfield 或 closed mesh scanline 生成 model mask；
5. 根据 texture.applyMode 判断 RGB 纹理应用层；
6. 根据 nonSurfaceRgbPolicy 写非表面 RGB；
7. 根据 support.mode 生成 S 通道支撑；
8. 根据 MaterialPolicy 写 RGB / W / V；
9. Model > Support，模型像素优先，支撑只写到非模型区域；
10. 写 RGBWSV TIFF / manifest / reports / preview。
```

### 2.3 当前默认 UI 一键配置

当前 UI `导入模型并切片` 生成配置的关键策略：

```text
slicingMode = relief_heightfield
autoOrient.enabled = true
autoOrient.maxHeightMm = 6.0
texture.applyMode = top_surface_band
texture.topSurfaceLayers = 50
texture.nonSurfaceRgbPolicy = model_material
modelMaterial.rgb = [0, 0, 0]
support.enabled = true
support.mode = full_vertical_projection
relief.fillMode = intersection_range
preview.channels = texture_rgb + support
```

### 2.4 当前 OpenVDB candidate

当前 OpenVDB candidate：

```text
需要 OpenVDB ON build；
UI 通过“导入模型并 OpenVDB 候选切片”触发；
texture.applyMode = surface_shell_from_sdf；
support.enabled = false；
strict_closed admission；
failurePolicy = non_production_only；
当前真实模型仍可能 non-production；
当前不能替代 legacy。
```

---

## 3. 需求偏差矩阵

| 用户需求 | 当前实现 | 偏差判断 | Stage |
|---|---|---|---|
| 彩色纹理模型每层有真实模型数据，支撑不混入模型真实数据 | RGBWSV 分通道已有，report 有 model/support stats | 基础正确，但缺少显式 ModelLayerSemantic / FillLayerSemantic | 12A |
| 颜色层为表层纹理 RGB | top_surface_band 已有 | 基础满足，但表层厚度/外表面定义需产品化 | 12A |
| 填充层可选白墨或光油 | MaterialPolicy 可 W/V all_model/top_n_layers | 部分满足，缺少显式模型填充材料策略 | 12A |
| 不规则浮雕/高 Z 区域要判断是否加支撑 | unsupported_only / island diagnostics 已有基础 | 需要甲片业务规则、阈值和验收图层 | 12A |
| 中间镂空应按支撑策略填充 | full_vertical_projection 可填部分列 | 未形成 internal void support / envelope fill 语义 | 12A |
| 支撑可选择上/下/上下填充，默认下表面 | 当前有 bottom/full/unsupported | 缺少 upper surface / both surface 明确策略 | 12A |
| 模型外侧覆盖可控厚度光油 | VarnishGeometryPolicy 有 AdditiveGrow 枚举 | 未实现生产外侧增厚光油壳层 | 12A |
| 彩色纹理和单材料切片效果一致 | 同一 legacy compose_layer 基础存在 | 需要一致性 fixture 和报告阈值 | 12A |
| OpenVDB 提速 | 当前 benchmark 显示更慢且语义不可比 | 不能证明提速，需重新评估用途 | 12B |
| 只比较切片耗时，不含保存图片 | core-only benchmark 已有基础 | 需要 Release、真实模型、同语义扩展 | 12B |
| UI 选项更详细 | 已补部分 tooltip / 手册 | 仍需系统化帮助和设置页 | 12C |
| 配置文件过多需收敛 | scenario visibility 已有 | 需 Profile/fixture 分层和设置页替代 JSON 暴露 | 12C |
| 三个预览入口整合 | 当前仍分层预览/叠加/原始 | 需统一预览工作区设计 | 12C |
| 报告/曲线位置优化 | 当前中心/右侧混合 | 需工作台布局收口 | 12C |

---

## 4. 当前是否偏离项目方向

结论：没有偏离总方向，但当前进入了“策略隐式组合过多”的阶段。

说明：

```text
1. 正式 PRD 目标就是 UV 彩色多材料切片；
2. 当前实现已经覆盖 RGB/W/S/V 基础通道；
3. 但用户真实业务需要的是可选择、可解释、可验收的材料/支撑/光油语义；
4. 当前很多行为散落在 texture.applyMode、nonSurfaceRgbPolicy、modelMaterial、MaterialPolicy、SupportPolicy 中；
5. 因此需要 Stage 12 做语义收敛，而不是继续只加配置项。
```

---

## 5. 12A 已确认需求基线

2026-07-06 已对 12A 的关键开放问题形成以下确认结论：

| 编号 | 问题 | 确认结论 |
|---|---|---|
| 1 | “填充层”含义 | 填充层有两个含义：模型内部填充层指两个颜色层或表面层之间的模型内部材料，如白墨、光油或其他可选材料；模型外部填充层指用 S 通道支撑材料支撑模型打印，防止缺支撑导致塌陷。 |
| 2 | 默认填充材料 | 模型内部填充材料默认使用白墨，可选择光油或后续扩展的其他模型材料；模型外部填充材料只能是可剥离支撑材料。 |
| 3 | 内部镂空区域 | 内部镂空区域一律填充支撑材料，生产 Profile 默认开启 internalVoidSupport。示意图见 `DOC/DIAGRAM_12A_内部镂空支撑与外侧光油支撑关系.svg`。 |
| 4 | 上表面支撑 | 上表面支撑是模型外部的可剥离支撑材料层；如果同时启用外侧光油层，则先生成模型外侧光油壳层，再在光油壳层外添加上表面支撑。 |
| 5 | 外侧光油扩张 | 外侧光油层允许扩张模型 XY 尺寸。 |
| 6 | 光油厚度单位 | 外侧光油厚度按 mm 配置，默认厚度为 0mm；配置精度为 0.01mm；像素换算默认使用 1px = 42.3um。 |
| 7 | 冲突优先级 | 模型本体与支撑冲突时保持 Model > Support；外侧光油壳层与支撑冲突时采用 Model > OuterVarnishShell > Support > Empty，并让上表面支撑生成在外侧光油壳层之外。 |
| 8 | 彩色/单材料一致性 | 除打印材料一个是彩色、一个是单色外，几何轮廓、支撑逻辑、通道统计逻辑和层顺序应一致。 |

由此可得到 12A 的生产默认值：

```text
modelFill.material.default = white
modelFill.material.emptyAllowedInProduction = false
support.internalVoid.enabled.default = true
support.placement.default = lower
outerVarnish.thicknessMm.default = 0.0
outerVarnish.thicknessMm.step = 0.01
outerVarnish.pixelPitchUm.default = 42.3
semanticPriority = Model > OuterVarnishShell > Support > Empty
singleMaterialAndColorConsistency = geometry/support/channel-statistics comparable
```

---

## 6. 建议

```text
1. 先执行 12A-0/12A-1：需求确认和语义模型文档；
2. 再执行 12A fixture：用 aishen_fudiao、nai_you_new、单材料甲片建立验收模型；
3. 12B 暂不承诺 OpenVDB 提速，先做同语义 Release benchmark；
4. 12C 将 UI 设置页作为 12A 策略入口，不再暴露大量测试 JSON 给普通用户；
5. 不要在 12A 未完成前把 OpenVDB 设为默认引擎。
```
