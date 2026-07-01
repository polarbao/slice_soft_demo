# DEV_10_OutputContract_TextureFidelity设计

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 10
> 生成日期：2026-07-01

---

## 1. 技术目标

10 阶段把输出包、manifest、report、layer summary 和 texture fidelity 信息定义成稳定技术契约。

本阶段不实现 RIP，只提供下游可消费的切片数据和解释信息。

---

## 2. Output Contract

建议定义：

```text
SlicingOutputContract
PackageContract
LayerSummaryContract
ChannelSummaryContract
TextureFidelityContract
MaterialProcessContract
DownstreamHandoffContract
```

最小字段：

```text
schema
packagePath
manifestPath
reportPath
previewPath
channelOrder
bitDepth
polarity
layerCount
resolution
pixelSize
layerHeight
materials
textureSources
fallbacks
diagnostics
productionAdmission
```

---

## 3. Layer Summary

每层建议包含：

```text
layerIndex
z
nonEmptyPixelCount
modelPixelCount
supportPixelCount
whitePixelStats
varnishPixelStats
rgbStats
textureFallbackCount
diagnosticOverlayCount
```

Layer summary 可来自 report 或独立 summary 文件，不能要求下游解析 UI 内部结构。

---

## 4. Texture Fidelity

建议指标：

```text
textureResolvedRate
uvCoverageRate
fallbackPixelRate
nearestTriangleHitRate
missingTextureCount
materialBindingCoverage
colorGroupCoverage
texture2DGroupCoverage
```

这些指标用于判断上游切片输出是否足够可靠，不代表最终打印色彩准确。

---

## 5. Downstream Handoff

下游交接包建议包含：

```text
RGBWSV package；
manifest；
report；
layer summary；
texture fidelity summary；
known limitations；
stable issue code list；
sample models；
expected output summary。
```

---

## 6. 模块边界

```text
slicer_core 生成输出契约 DTO / report；
apps/slicer_cli 输出 package/report/summary；
apps/slicer_debug_ui 可读取契约做展示；
下游 RIP 库或设备代码不进入 slicer_core 主依赖。
```

禁止：

```text
slicer_core -> RIP SDK；
slicer_core -> Qt；
output contract -> device command；
report -> production decision override。
```

---

## 7. 验证入口

建议新增：

```text
docs/slice/DEMO/DEMO_10_切片输出契约与纹理保真验证方案.md
scripts/run_10_output_contract_tests.ps1
tests/golden/expected/10_output_contract_summary.json
```
