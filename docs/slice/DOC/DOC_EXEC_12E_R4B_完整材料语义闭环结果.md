# DOC_EXEC_12E-R4B 完整材料语义闭环结果

> 文档状态：12E-08B COMPLETE / DIAGNOSTIC ONLY
> 日期：2026-07-20
> 前置任务：12E-08A classification-to-raster COMPLETE
> 后续任务：12E-08C Release 真实模型预算与 legacy regression

## 1. 执行结论

12E-08B 已建立从 12E 最终 raster ownership 到 12D 完整材料闭环诊断的只读适配层。
支撑、内部空洞支撑、表面光油和外侧光油不再沿用 12E-07 的零 mask `not_evaluated`
占位，而是以与真实 `layerIndex/zMm` 对齐的 semantic sidecar 进入 12D 检测。

本任务仍为 diagnostic-only：不修复像素、不写 TIFF、不写 manifest、不发布 package，
`productionAcceptance` 保持 `not_evaluated`。

## 2. 输入合同

每个目标 raster 层必须同时提供：

```text
TextureSurfaceMask；
ModelFillMask；
ModelMaterialMask；
SupportFillMask；
InternalVoidSupportMask；
SurfaceVarnishMask；
OuterVarnishShellMask；
ModelEnvelopeMask；
SupportRequiredMask；
固定顺序 R G B W S V 的 uint8 RGBWSV 像素；
真实 layerIndex 和 zMm。
```

适配器拒绝以下输入：

```text
raster mapping 不可用或 partitionPass=false；
层数、尺寸、layerIndex 或 zMm 不对齐；
mask 非二值或尺寸不一致；
RGBWSV 通道顺序不是 R G B W S V；
texture/fill 不是 model 的精确互补；
internal-void support 不在 envelope 内或与 model 重叠；
最终 mask 违反 Model > OuterVarnishShell > Support > Empty。
```

## 3. 期望占用域

诊断域按最终语义证据确定：

```text
ExpectedOccupiedDomain = ModelEnvelope OR SupportRequired OR OuterVarnishShell
LayerEmpty = all(R,G,B,W,S,V == 255)
```

该定义用于检查所有应有材料但六通道均为空的像素。它不使用 preview PNG，也不通过
图像缩放或颜色外观推断材料。

## 4. 完整闭环判定

每层输出：

```text
expectedDomainGapPixels；
modelDomainGapPixels；
supportRequiredGapPixels；
outerVarnishGapPixels；
unexpectedOccupiedPixels；
supportChannelMismatchPixels；
varnishChannelMismatchPixels；
ColorFillGap；
ModelSupportGap；
ColorSupportGap；
InternalVoidGap；
VarnishSupportGap；
closurePass。
```

整包诊断分别给出 `modelClosureStatus`、`supportClosureStatus`、
`varnishClosureStatus` 和 `fullClosurePass`。支撑或光油 mask 与最终 S/V 通道不一致时，
对应材料状态必须失败，不能仅依赖“像素非空”误判为闭环。

## 5. allTexture 语义

`allTexture=true` 时：

```text
ModelFillMask 允许为空；
colorFillApplicability=not_applicable；
allTextureReason=all_texture_partition；
模型域、支撑域和光油域仍必须独立通过闭环检查。
```

## 6. 稳定错误码

```text
E_12E_FULL_CLOSURE_INPUT_INVALID
E_12E_FULL_CLOSURE_LAYER_ORDER_INVALID
E_12E_FULL_CLOSURE_MASK_INVALID
E_12E_FULL_CLOSURE_CHANNEL_ORDER_INVALID
E_12E_FULL_CLOSURE_PRIORITY_CONFLICT
E_12E_FULL_CLOSURE_SEMANTIC_MISMATCH
E_12E_FULL_CLOSURE_GAP_DETECTED
E_12E_FULL_CLOSURE_UNEXPECTED_MATERIAL
```

## 7. Report 合同

`slicesoft.texture_fill_partition.12e.1` 新增 `fullClosureLinkage`，完整序列化上述
材料状态、汇总计数、逐层计数、issues 和 `analysisMs`。`performance.fullClosureMs`
记录该只读诊断耗时；证据缺失时保持 `null`，不会伪造 0 ms 或 PASS。

## 8. Generated Fixture

单元测试使用 `11 x 9 x 1` 的确定性 raster fixture，同时覆盖：

```text
纹理表面与模型填充互补；
模型内部空洞支撑；
模型外支撑桥；
模型表面光油；
模型外侧光油壳层；
真实 RGB/W/S/V 打印值；
空白通道值 255。
```

共 16 个 adapter case：完整通过、internal void、allTexture、四类 gap、优先级冲突、
支撑/光油通道缺失、意外光油通道、合法 V 模型填充、层序错误、非二值 mask、域外材料和重复确定性。

## 9. 实际验证

```text
cmake --build build --config Debug：PASS；
ctest --test-dir build -C Debug --output-on-failure：19/19 PASS；
OpenVDB ON 定向 build：PASS；
OpenVDB ON adapter/report CTest：2/2 PASS；
run_material_closure_tests.ps1 -Mode RepairDisabled：PASS；
RIP Reader：baseline/diagnostic 均 PASS；
30 层 production TIFF SHA-256 invariant：PASS。
```

OpenVDB ON 配置阶段仍出现既有 CMake `CMP0167/FindBoost` developer warning，不影响本任务
目标或测试结果。

## 10. 未覆盖范围

12E-08B 没有完成：

```text
真实 OBJ/3MF Release 性能和峰值内存预算；
旧 Profile 全量回归；
12E production composer/writer 接入；
production package、manifest 或 RIP admission；
Qt 诊断 UI；
任何自动修复。
```

## 11. 安全边界

```text
p0.rgbwsv.2 未修改；
R G B W S V 未修改；
uint8 未修改；
black_is_print 未修改；
OpenVDB 保持 optional/OFF；
legacy slicer_cli production path 未被替代；
repairAttempted=false；
productionOutputWritten=false；
productionAcceptance=not_evaluated。
```

## 12. 下一 Gate

本文完成时的下一原子任务为 12E-08C。该任务现已完成取证，结果见
`DOC_EXEC_12E_R4C_默认OFFRelease真实模型与Legacy回归结果.md`；真实 OBJ topology 仍阻断
Release budget。12E-08D 仍属于生产路径变更，必须先关闭预算阻断并再次取得用户明确确认。

12E-09 准备文档已完整。12E-09A diagnostic UI 现已 READY，09B production Profile 继续被
08D 阻断。
