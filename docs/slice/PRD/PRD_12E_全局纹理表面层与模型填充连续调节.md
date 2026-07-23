# PRD_12E 全局纹理表面层与模型填充连续调节

> 文档版本：v0.1
> 文档状态：PRD / Stage 12E Planning
> 生成日期：2026-07-16
> 上游文档：PRD_12A_彩色纹理材料填充支撑光油策略.md、PRD_12D_横截面材料无缝闭环验收与修复.md
> 实现状态：PARTIAL；12E-01..07、12E-08A/08B/08C、R1/R2/R3/R4 与 12E-08D-01..04 COMPLETE；restricted Global Profile GO；普通 Global 支撑/光油/0.01mm 等价 NO-GO

## 1. 背景

12A 已定义 `Texture Surface Layer` 和 `Model Fill Layer`，并提出：

```text
model fill mask = model mask - texture surface mask
```

但当前产品定义和 UI 尚未把两者收敛成一个可连续调节、可证明互补的组合策略：

```text
1. 没有正式的纹理表面宽度 mm 配置；
2. 没有模型特定的全纹理上限；
3. 没有定义宽度增大时填充区域如何单调缩小；
4. 当前 legacy 纹理分类仍可能依赖顶面/列/layer 判断；
5. UI 中纹理策略和模型填充材料是分开的，但缺少分区覆盖率和全纹理状态。
```

## 2. 产品目标

12E 要提供一个从“薄纹理表层 + 大面积模型填充”连续过渡到“全模型纹理 + 零模型填充”的产品能力。

目标：

```text
1. 用户按 mm 设置 Texture Surface Layer 向模型内部延伸的宽度；
2. Model Fill Layer 自动成为剩余模型体积，不再单独配置几何宽度；
3. 宽度从最小值到模型动态最大值连续可调；
4. 达到最大值时只有纹理层数据，没有模型填充层数据；
5. 任意宽度下模型实体都必须被纹理或填充完整覆盖；
6. 分类基于完整三维模型，而不是逐切片二维近似；
7. UI、report、preview 和 production semantic mask 使用同一份分区结果。
```

## 3. 产品术语

### 3.1 Global Texture Surface Volume

全局纹理表面体积是完整三维模型内部，距离任一闭合模型表面不超过 `widthMm` 的体积。

包括：

```text
外表面；
已被模型拓扑定义为闭合边界的内腔表面；
薄壁两侧相遇后合并的纹理区域。
```

不包括：

```text
模型外部扩张区域；
OuterVarnishShell；
Support；
按单层轮廓临时生成的二维边带。
```

### 3.2 Complementary Model Fill Volume

互补模型填充体积是模型实体中未被全局纹理表面体积占用的剩余区域。

填充材料继续由 `modelFill.material` 决定：

```text
white | varnish | rgb | profile_default | material_role
```

几何范围由互补关系决定，不再由独立的填充宽度决定。

### 3.3 All-Texture Threshold

全纹理阈值是当前模型内部到闭合表面的最大距离，按 UI 步长向上取整后的值。

达到该阈值后：

```text
textureSurfaceCoverage = 100%
modelFillCoverage = 0%
unassignedModelCoverage = 0%
```

## 4. 宽度要求

### 4.1 最小宽度

首版工程建议：

```text
base minimum = 0.10 mm
UI step = 0.01 mm
effective minimum = max(0.10 mm, 2 × 最粗分类分辨率)
```

当当前输出分辨率或 SDF voxel 使有效最小值大于 0.10 mm 时，UI 必须使用计算后的更大值，并说明来源。

### 4.2 最大宽度

最大宽度必须在模型导入、变换和全局距离分析后动态计算。

```text
allTextureThresholdMm = max(
  effectiveMinimumWidthMm,
  ceil(maxInteriorDistanceMm / 0.01) * 0.01)
```

要求：

```text
1. 未加载有效闭合模型前，最大值不可用，控件不得假装已有固定范围；
2. 加载模型后显示 allTextureThresholdMm；
3. 用户将宽度调到该值时，Model Fill coverage 必须为 0%；
4. 模型变化、缩放、方向或输出分辨率变化后重新计算；
5. 重新计算不得静默保留越界值，应 clamp 并在 effective config/report 中记录。
```

### 4.3 连续与单调

对任意 `w1 < w2`：

```text
TextureSurfaceVolume(w1) ⊆ TextureSurfaceVolume(w2)
ModelFillVolume(w2) ⊆ ModelFillVolume(w1)
```

离散输出允许相邻请求值产生相同 mask，但不允许出现纹理覆盖率随宽度增大反而下降的情况。

## 5. 整体三维处理要求

12E 的“表面”必须从整个模型出发判断。

验收口径：

```text
1. 先对完整变换后模型建立三维 inside/outside 和 surface distance；
2. 再生成 3D texture/fill masks；
3. 最后将 3D masks 投影或采样到每个输出 layer；
4. 禁止逐 layer 单独腐蚀轮廓后宣称为 global surface shell；
5. 凹面、斜面、侧壁、内腔和薄壁必须由同一个三维距离定义处理。
```

## 6. 模型填充为空的合法条件

生产模式只有同时满足以下条件时，才允许 `modelFillPixels=0`：

```text
texture.applyMode = global_surface_shell
requestedWidthMm >= allTextureThresholdMm
textureSurfacePixels = modelPixels
unassignedModelPixels = 0
overlapTextureFillPixels = 0
partitionPass = true
materialClosure exact diagnosis = PASS 或满足该阶段明确的准入门槛
```

以下仍属于错误：

```text
通过禁用 modelFill 制造空洞；
texture/fill 均未覆盖模型像素；
只根据 RGB 通道非空推断全模型已分配；
把 preview 视觉闭合当作 production PASS。
```

## 7. 用户故事

### US-12E-01 调节纹理表面宽度

作为工艺人员，我希望以 0.01 mm 步长调节纹理层向模型内部的宽度，并立即看到纹理/填充覆盖率变化。

验收：

```text
1. UI 显示 requested/effective width；
2. 显示模型动态范围和全纹理阈值；
3. 宽度增大时 texture coverage 不下降，fill coverage 不上升；
4. 预览能分别显示 Texture Surface 和 Model Fill。
```

### US-12E-02 全纹理模型

作为工艺人员，我希望把宽度调到模型上限，使模型全部使用纹理数据而没有内部填充层数据。

验收：

```text
modelFillPixels = 0；
textureSurfacePixels = modelPixels；
unassignedModelPixels = 0；
report 明确 allTexture=true，而不是 modelFill 配置错误。
```

### US-12E-03 薄壁和局部消失

作为工艺人员，我希望薄壁区域在纹理壳层从两侧相遇时自动成为全纹理，而厚区域仍保留填充。

验收：

```text
局部 fill 可以先于全模型阈值消失；
不产生负宽度、重叠或未分配区域；
report 记录 thinRegionMergedPixels/Voxels 或等价统计。
```

### US-12E-04 全局而非逐层判断

作为维护者，我希望斜面和侧壁的纹理厚度由三维距离决定，避免不同层的二维轮廓宽度导致锯齿或厚度漂移。

验收：

```text
球体/斜面/凹面 fixture 的三维厚度误差在报告阈值内；
逐层二维近似不能作为 production backend 通过准入。
```

## 8. UI 需求

材料设置中新增或调整：

```text
纹理体积策略：全局三维表面层 / 现有兼容模式；
纹理表层宽度：slider + QDoubleSpinBox，单位 mm，步长 0.01；
有效最小值：只读；
全纹理阈值：只读，模型分析后显示；
覆盖率：Texture Surface % / Model Fill %；
全纹理状态：达到阈值时显示 allTexture=true；
模型填充材料：仅在 fill coverage > 0 时参与输出，但配置值保留。
```

交互要求：

```text
1. 普通用户不选择距离后端；
2. OpenVDB 仍只在高级/诊断区域显示 candidate 状态；
3. 控件变化写入 session effective config，不覆盖原始 fixture；
4. 未完成模型分析时禁用宽度控件或显示 pending，不使用虚假固定最大值；
5. 宽度达到最大值时不得自动把 modelFill.enabled 改为 false。
```

## 9. Report 与预览需求

报告至少输出：

```text
requestedWidthMm；
effectiveMinimumWidthMm；
effectiveWidthMm；
maxInteriorDistanceMm；
allTextureThresholdMm；
allTexture；
classificationBackend；
classificationResolutionMm；
textureSurfacePixels/Voxels；
modelFillPixels/Voxels；
overlapTextureFillPixels/Voxels；
unassignedModelPixels/Voxels；
partitionPass；
quantizationErrorMm；
medialAxisTieCount 或等价诊断。
```

每层必须满足：

```text
textureSurfacePixels + modelFillPixels = modelPixels
overlapTextureFillPixels = 0
unassignedModelPixels = 0
```

预览至少支持：

```text
Texture Surface；
Model Fill；
Texture + Fill Partition；
距离/厚度诊断（高级模式）；
```

## 10. 兼容性

```text
1. 老配置缺少 texture.surfaceShell 时保持原输出；
2. global_surface_shell 必须显式选择，不能由 top_surface_band 自动迁移；
3. surface_shell_from_sdf 仍是 experimental config，不自动升级为 production；
4. 12A 现有 RGB + white/varnish/RGB Profile 保留；
5. 新增全纹理 Profile 必须在 12E production gate 通过后才进入普通用户默认集。
```

## 11. 非目标

12E 不做：

```text
1. 不改变 RGBWSV 协议；
2. 不把 OpenVDB 设为默认依赖；
3. 不用逐层二维 shell 冒充全局三维结果；
4. 不实现 RIP、半色调或设备标定；
5. 不在未通过拓扑准入的模型上生成 production 全局壳层；
6. 不在本阶段重新定义 OuterVarnishShell；
7. 不在文档阶段修改当前 12D 原子任务或生产输出。
```

## 12. 阶段完成标准

```text
1. PRD/DEV/DEMO/ROADMAP/TASKS/CODEX_PROMPT/DECISION 完整；
2. 全局 3D texture/fill partition contract 固化；
3. 最小值、动态最大值和全纹理条件可验证；
4. 至少一个默认 OFF production candidate backend 通过正确性、性能和内存 gate；
5. OpenVDB ON 只作为可选交叉验证，默认 OFF 不受影响；
6. UI 设置、effective config、preview、report 一致；
7. 12D exact semantic masks 可证明分区闭环；
8. 真实甲片模型、薄壁、内腔、非流形 blocker 和全纹理用例通过；
9. 最终生成 REPORT_12E，明确 production admission 结论。
```

真实 OBJ 的生产准入必须先满足 `PRD_12E_08C_真实模型拓扑修复与严格准入.md`。修复专项允许对复杂模型给出
`manual_repair_required`，但该状态不得计入本 PRD 的真实模型 production PASS。

## 13. 双切片模式与统一生产输出补充需求

12E 最终产品必须允许用户在 UI 或配置文件中显式选择两条端到端切片流水线：

```text
legacy：现有生产切片流水线，保持默认和兼容行为；
global_surface_shell：全局三维纹理壳层与互补模型填充流水线。
```

正式配置入口为 `slicePipeline.mode`，取值仅允许 `legacy` 或 `global_surface_shell`。该字段与
`slicingMode` 的几何类别、`texture.applyMode` 的材料策略以及 OpenVDB/CPU 距离后端相互独立，
不得复用旧字段造成语义混淆。缺少该字段的历史配置按 `legacy` 解释。

无论选择哪条流水线，只要运行结果被标记为生产成功，就必须满足同一输出合同：

```text
生成完整 RGBWSV TIFF layer list；
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
保留当前 preview、材料通道预览、report 和 manifest 能力；
通过 RIP Reader strict 校验。
```

`global_surface_shell` 在生产准入前只能生成诊断 mask、preview 和 report；不得把诊断完成宣称为
生产切片成功，也不得静默回退到 `legacy` 后继续写包。若全局流水线不可用或被拓扑门禁阻断，UI 和 CLI
必须返回明确状态与稳定错误码，让用户主动改选 `legacy`。

UI 最终必须提供“传统切片”和“全局纹理壳层”两个中文选项，并显示 requested/effective pipeline、
准入状态和失败原因。`global_surface_shell` 普通用户入口只有在 12E-08D production admission 通过后启用。

## 14. 模型预检、正向开发与材料角色补充

R3-04 NO-GO 不取消 12E 的 Texture Surface/Model Fill 产品目标。12E-08D 前新增 R4 专项：

```text
正常闭合 OBJ/3MF 用于继续验证 minimum/intermediate/allTexture 和 UI；
三个 required OBJ 保留为真实准入集，不能被正常 fixture 替换；
导入后先完成 fast/full preflight，再按当前 pipeline mode 给出准入；
global 对 self-intersection/open/non-manifold/incomplete audit fail-closed；
legacy 保持兼容能力，但必须显示风险且不得冒充 global PASS；
任何一键入口均不得绕过 fresh preflight。
```

Model Fill 材料补充：

```text
white、varnish、RGB/custom 继续映射到现有 W/V/RGB；
C/M/Y/K 作为 MaterialProcessProfile material role，由工艺 Profile 解析到 RGBWSV；
当前协议没有独立 C/M/Y/K 通道，未配置映射时 UI 不得伪造可用选项；
allTexture 时保留材料选择配置，但 Model Fill mask 合法为零。
```

推荐 `baseMinimumWidthMm=0.10`、`step=0.01` 保持不变；实际 UI 下限使用
`max(0.10mm, 2 * classificationResolutionMm)`，上限来自模型动态 `allTextureThresholdMm`。
