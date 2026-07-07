# TASKS_12A_彩色纹理材料支撑光油任务清单

> 文档版本：v0.3
> 文档状态：Task List / Stage 12A
> 生成日期：2026-07-05
> 更新日期：2026-07-06

---

## 任务边界

12A 只处理彩色纹理/单材料模型的材料语义、模型填充、支撑填充、内部镂空支撑、外侧光油壳层和相关 report/preview。
不处理 OpenVDB 替代结论，不处理 UI 大布局重构，不改变 RGBWSV 协议。

---

## 原子任务

### Task 12A-01 需求术语确认

状态：DONE

内容：

```text
确认 Model Real Data、TextureSurface、ModelFill、SupportFill、InternalVoidSupport、OuterVarnishShell 定义。
已确认：
1. 模型内部填充默认 white，不允许生产 Profile 为空；
2. 模型外部填充只能是 S 支撑；
3. internalVoidSupport 默认开启且内部镂空一律填支撑；
4. support.placement 默认 lower；
5. outerVarnish 默认 thicknessMm=0.0，单位 mm，精度 0.01mm；
6. 优先级为 Model > OuterVarnishShell > Support > Empty；
7. 上表面支撑位于外侧光油壳层之外。
```

验证：

```powershell
git diff --check
```

### Task 12A-02 配置 schema 占位

状态：DONE

内容：

```text
新增 modelFill / support.internalVoid / outerVarnish 配置字段，默认兼容旧行为。
生产默认：
modelFill.material=white
modelFill.emptyAllowedInProduction=false
support.internalVoid.enabled=true
support.placement=lower
outerVarnish.thicknessMm=0.0
outerVarnish.thicknessStepMm=0.01
outerVarnish.pixelPitchUm=42.3
outerVarnish.conflictPolicy=varnish_shell_wins
```

完成记录：

```text
已在 SliceConfig 中新增 modelFill、support.placement、support.internalVoid、support.upper、outerVarnish 占位字段；
已接入 JSON 解析、核心配置校验、UI 配置校验和代表样例配置；
当前任务不改变切片输出组合行为，后续 Task 12A-03/04 再接 report 与实际策略。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
```

### Task 12A-03 LayerSemanticReport

状态：DONE

内容：

```text
在 slice_report 或 layer summary 中统计 textureSurfacePixels、modelFillPixels、supportPixels、internalVoidSupportPixels、outerVarnishPixels。
```

完成记录：

```text
已在 slice_report.totals、slice_report.layers[]、manifest layer summary 中写入 textureSurfacePixels、modelFillPixels、supportPixels、internalVoidSupportPixels、outerVarnishPixels。
已新增 semantic 节点和 materialSemantics 配置说明，用于解释 12A 当前配置语义。
当前 internalVoidSupportPixels 和 outerVarnishPixels 仍为 0；实际像素生成留给 Task 12A-05 / 12A-07。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
PowerShell ConvertFrom-Json 检查 slice_report.totals 和 slice_report.layers[0] 均包含五个 12A 语义字段
cmake --build build --config Debug --target rip_reader_test
.\build\Debug\rip_reader_test.exe --package output\ObjMtlTextureRgbWhiteVarnish
```

### Task 12A-04 ModelFillPolicy

状态：DONE

内容：

```text
实现 white / varnish / rgb / profile_default / material_role / legacyRgbFallback。
生产 Profile 禁止 none/empty 模型内部填充；诊断 fixture 如需空填充必须标记 non-production。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
ctest --test-dir build -C Debug -R experimental_config_unit_tests --output-on-failure

# model/obj 多模型临时验证配置：
# output/12a04_validation/configs/nai_you_white.json
# output/12a04_validation/configs/aishen_varnish.json
# output/12a04_validation/configs/titian_rgb.json
# output/12a04_validation/configs/xiao_ma_profile_default.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\nai_you_white.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\aishen_varnish.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\titian_rgb.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\xiao_ma_profile_default.json

Python 临时 TIFF 探针读取关键层 RGBWSV，验证：
nai_you_white layer=0 => W printPixels=8
aishen_varnish layer=0 => V printPixels=96
titian_rgb layer=0 => R/G/B printPixels=10/10/10
xiao_ma_profile_default layer=0 => resolvedProfileDefaultMaterial=white, W printPixels=10

.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
cmake --build build --config Debug --target rip_reader_test
.\build\Debug\rip_reader_test.exe --package output\ObjMtlTextureRgbWhiteVarnish
git diff --check
```

### Task 12A-05 InternalVoidSupportPolicy

状态：DONE

内容：

```text
实现 all_internal_voids 内部镂空支撑，生产默认开启。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_internal_void.json

Python 临时 TIFF 探针读取 output\SupportInternalVoid\layers\layer_000002.tiff：
layer=2 S_printPixels=576
borderSPrintPixels=0

PowerShell ConvertFrom-Json 检查：
slice_report.totals.internalVoidSupportPixels = 2304
support_report.supportTypeStats.internal_void = 2304
support_report.internalVoid.reason = internal_void

model/obj 多模型验证：
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\nai_you_white.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\aishen_varnish.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\titian_rgb.json
.\build\Debug\slicer_cli.exe --config output\12a04_validation\configs\xiao_ma_profile_default.json

结果摘要：
nai_you_white           internalVoidSupportPixels=646009
aishen_varnish          internalVoidSupportPixels=419602
titian_rgb              internalVoidSupportPixels=518095
xiao_ma_profile_default internalVoidSupportPixels=517676

cmake --build build --config Debug --target rip_reader_test experimental_config_unit_tests
.\build\Debug\rip_reader_test.exe --package output\SupportInternalVoid --summary
.\build\Debug\experimental_config_unit_tests.exe
cmake --build build --config Debug --target support_shape_unit_tests
.\build\Debug\support_shape_unit_tests.exe
ctest --test-dir build -C Debug -R "experimental_config_unit_tests|support_shape_unit_tests" --output-on-failure
git diff --check
```

### Task 12A-06 SupportPlacementPolicy

状态：DONE

内容：

```text
梳理 lower / upper / both / unsupported_only / full_vertical_projection，并把 full_vertical_projection 标记为 advanced/debug。
默认 placement=lower。
upper 是模型外部可剥离支撑，如果启用 outerVarnish，upper 支撑必须在外侧光油壳层之外。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests
cmake --build build --config Debug --target slicer_debug_ui

.\build\Debug\slicer_cli.exe --config samples\configs\support\support_placement_lower.json
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_placement_upper.json
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_placement_both.json
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_placement_unsupported_only.json
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_placement_full_vertical_projection.json

PowerShell ConvertFrom-Json 检查 support_report：
SupportPlacementLower                  effective=lower, bottom_projection=16520, upper_projection=0
SupportPlacementUpper                  effective=upper, bottom_projection=0, upper_projection=24780
SupportPlacementBoth                   effective=both, bottom_projection=16520, upper_projection=24780
SupportPlacementUnsupportedOnly        effective=unsupported_only, unsupported_island=12672
SupportPlacementFullVerticalProjection effective=full_vertical_projection, full_vertical_projection=16520, advancedDebug=true

.\build\Debug\rip_reader_test.exe --package output\SupportPlacementLower --summary
.\build\Debug\rip_reader_test.exe --package output\SupportPlacementUpper --summary
.\build\Debug\rip_reader_test.exe --package output\SupportPlacementBoth --summary
.\build\Debug\rip_reader_test.exe --package output\SupportPlacementUnsupportedOnly --summary
.\build\Debug\rip_reader_test.exe --package output\SupportPlacementFullVerticalProjection --summary

model/obj 多模型临时配置验证：
.\build\Debug\slicer_cli.exe --config output\12a06_validation\configs\nai_you_lower.json
.\build\Debug\slicer_cli.exe --config output\12a06_validation\configs\aishen_upper.json
.\build\Debug\slicer_cli.exe --config output\12a06_validation\configs\titian_both.json
.\build\Debug\slicer_cli.exe --config output\12a06_validation\configs\xiao_ma_full_vertical.json

真实模型结果摘要：
nai_you_lower         effective=lower, bottom_projection=9410802
aishen_upper          effective=upper, upper_projection=8103398
titian_both           effective=both, bottom_projection=8764576, upper_projection=5182640
xiao_ma_full_vertical effective=full_vertical_projection, full_vertical_projection=8761964, advancedDebug=true

.\build\Debug\rip_reader_test.exe --package output\12a06_validation\packages\nai_you_lower --summary
.\build\Debug\rip_reader_test.exe --package output\12a06_validation\packages\aishen_upper --summary
.\build\Debug\rip_reader_test.exe --package output\12a06_validation\packages\titian_both --summary
.\build\Debug\rip_reader_test.exe --package output\12a06_validation\packages\xiao_ma_full_vertical --summary

.\build\Debug\experimental_config_unit_tests.exe
.\build\Debug\support_shape_unit_tests.exe
ctest --test-dir build -C Debug -R "experimental_config_unit_tests|support_shape_unit_tests" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check

experimental_config_unit_tests 覆盖：
legacy 配置保持 placement implicit；
12A 配置解析显式 placement；
12A-07 前 upper/both + outerVarnish.thicknessMm>0 会被拒绝，避免上表面支撑生成到错误边界。
```

### Task 12A-07 OuterVarnishShellPolicy

状态：DONE

内容：

```text
实现外侧光油壳层 thicknessMm/thicknessStepMm/pixelPitchUm/thicknessPx 换算和 XY 扩张。
默认 thicknessMm=0.0，单位 mm，精度 0.01mm，默认 1px=42.3um。
冲突优先级为 Model > OuterVarnishShell > Support > Empty。
```

完成记录：

```text
已实现：
1. outerVarnish.thicknessMm 按 ceil(thicknessMm * 1000 / pixelPitchUm) 换算为 thicknessPx；
2. allowXYExpansion=true 时，输出 grid XY 边界按 thicknessPx 扩张；
3. 按每层 model mask 生成外侧 V 壳层，只填充与画布边界连通的模型外部区域，不覆盖内部镂空；
4. compose_layer 执行 Model > OuterVarnishShell > Support > Empty；
5. support 与 outerVarnish 同像素冲突时清除 support 像素，保证生产 TIFF 与 support 统计一致；
6. slice_report.totals.materialSemantics.outerVarnish 输出 thicknessMm、thicknessPx、effectiveThicknessMm、pixelPitchUm、allowXYExpansion、conflictPolicy、value、printPixels；
7. 12A-06 的 upper/both + outerVarnish 临时拒绝已解除，experimental_config_unit_tests 改为验证该组合可解析。
```

验证：

```text
V 通道壳层宽度等于配置厚度换算后的像素宽度，report 输出 thicknessMm、thicknessPx、effectiveThicknessMm。
```

验证记录：

```text
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests rip_reader_test
build\Debug\experimental_config_unit_tests.exe
build\Debug\slicer_cli.exe --config samples\configs\support\support_outer_varnish_shell_1px.json
build\Debug\slicer_cli.exe --config samples\configs\support\support_outer_varnish_shell_2px_with_support.json
build\Debug\rip_reader_test.exe --package output\SupportOuterVarnishShell1px --summary
build\Debug\rip_reader_test.exe --package output\SupportOuterVarnishShell2pxWithSupport --summary

Python validation:
output\SupportOuterVarnishShell1px: thicknessPx=1, effectiveThicknessMm=0.0423, V=1520, preview layer 2 V-to-model distance=1..1
output\SupportOuterVarnishShell2pxWithSupport: thicknessPx=2, effectiveThicknessMm=0.0846, V=3072, preview layer 2 V-to-model distance=1..2

model/obj 多模型验证：
output/12a07_validation/xiao_ma_lower_1px/package: PASS, V=112889, S=4706837, thicknessPx=1, effectiveThicknessMm=0.0423
output/12a07_validation/yecan_both_2px/package: PASS, V=293205, S=9325887, thicknessPx=2, effectiveThicknessMm=0.0846, upperOutsideBoundary=outer_varnish_shell
output/12a07_validation/nai_you_lower_2px/package: PASS, V=293199, S=5046873, thicknessPx=2, effectiveThicknessMm=0.0846
output/12a07_validation/aishen_lower_1px/package: PASS, V=153438, S=4123061, thicknessPx=1, effectiveThicknessMm=0.0423
```

### Task 12A-08 SurfaceVarnishPolicy

状态：DONE

内容：

```text
实现表面光油策略，区分外表面光油、内表面光油和外侧扩张光油壳层。
真实 RIP 横截面中“表面层光油”和“模型内表面光油层”都应能被 report/preview 解释。
```

完成记录：

```text
已实现：
1. 新增 surfaceVarnish 配置，默认 disabled，不改变旧配置输出；
2. surfaceVarnish.outerSurface 在模型像素邻接外部空白时写 V；
3. surfaceVarnish.innerSurface 在模型像素邻接内部镂空空白时写 V；
4. surfaceVarnish.thicknessPx 控制模型内表面带像素宽度，默认 1px；
5. surfaceVarnish.source 支持 explicit / material_policy，默认 explicit；
6. SurfaceVarnishLayer 写在模型像素自身，不扩张 XY，与 OuterVarnishShell 的外扩 V 壳层保持独立；
7. slice_report.totals、layer summary 和 semantic block 输出 outerSurfaceVarnishPixels / innerSurfaceVarnishPixels；
8. materialSemantics.surfaceVarnish 输出 enabled、outerSurface、innerSurface、thicknessPx、source、value、outerSurfacePrintPixels、innerSurfacePrintPixels。
```

验证：

```text
outerSurfaceVarnishPixels 和 innerSurfaceVarnishPixels 可统计；
它们与 outerVarnishPixels 语义不同。
```

验证记录：

```text
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests rip_reader_test
build\Debug\experimental_config_unit_tests.exe
build\Debug\slicer_cli.exe --config samples\configs\support\support_surface_varnish_outer_inner.json
build\Debug\rip_reader_test.exe --package output\SupportSurfaceVarnishOuterInner --summary

Python validation:
output\SupportSurfaceVarnishOuterInner:
outerSurfaceVarnishPixels=1488
innerSurfaceVarnishPixels=400
outerVarnishPixels=0
varnishPrintPixels=1888

model/obj 多模型验证：
output/12a08_validation/xiao_ma_outer_only/package: PASS, outerSurface=109727, innerSurface=0, outerShell=0, V=109727
output/12a08_validation/yecan_outer_inner/package: PASS, outerSurface=141229, innerSurface=16358, outerShell=0, V=157587
output/12a08_validation/nai_you_outer_inner/package: PASS, outerSurface=142762, innerSurface=12336, outerShell=0, V=155098
output/12a08_validation/aishen_outer_inner/package: PASS, outerSurface=148261, innerSurface=8762, outerShell=0, V=157008
```

### Task 12A-09 UpperSurfaceSupportPolicy

状态：DONE

内容：

```text
实现上表面支撑生成逻辑。
如果同时启用 outerVarnish，先生成外侧光油壳层，再在光油壳层之外生成上表面支撑。
```

完成记录：

```text
已实现：
1. upper support 仍由 support.placement=upper/both 或 support.upper.enabled 显式启用；
2. 当 support.upper.outside=outer_varnish_shell 且 outerVarnish.enabled=true/thicknessPx>0 时，upper support 使用 model envelope + outerVarnish shell 作为上表面支撑边界；
3. upper support 生成仍从边界 column 的 upper_layer+1 开始，避免覆盖模型或光油壳层本体；
4. 支撑与 outerVarnish 同像素冲突时清除支撑，执行 Model > OuterVarnishShell > Support > Empty；
5. slice_report / support_report 输出 upperSurfaceSupportPixels、upperBoundarySource、upperBoundaryIncludesOuterVarnishShell、outerVarnishSupportOverlapPixelsCleared；
6. 新增 support_upper_surface_outer_varnish_shell fixture，验证 upper support 与 outer varnish shell 共存。
```

验证：

```text
upper surface fixture 中 outerVarnishPixels > 0 且 upperSurfaceSupportPixels > 0；
同像素冲突执行 Model > OuterVarnishShell > Support > Empty。
```

验证记录：

```text
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests rip_reader_test
build\Debug\experimental_config_unit_tests.exe
build\Debug\slicer_cli.exe --config samples\configs\support\support_upper_surface_outer_varnish_shell.json
build\Debug\rip_reader_test.exe --package output\SupportUpperSurfaceOuterVarnishShell --summary

Python validation:
output\SupportUpperSurfaceOuterVarnishShell:
outerVarnishPixels=3072
upperSurfaceSupportPixels=18056
outerVarnishSupportOverlapPixelsCleared=0
upperBoundarySource=model_envelope_plus_outer_varnish_shell
layer 6/7 upper support = 9028px = model footprint 8260px + outer varnish shell footprint 768px

model/obj 多模型验证：
output/12a09_validation/xiao_ma_upper_1px/package: PASS, outerVarnish=112889, upperSurface=2443480, support=2701181, overlapCleared=0
output/12a09_validation/yecan_both_2px/package: PASS, outerVarnish=293205, upperSurface=3264721, support=9532444, overlapCleared=112989
output/12a09_validation/nai_you_upper_2px/package: PASS, outerVarnish=293199, upperSurface=3159579, support=3491455, overlapCleared=0
output/12a09_validation/aishen_both_1px/package: PASS, outerVarnish=153438, upperSurface=3996421, support=8119482, overlapCleared=58757
```

### Task 12A-10 真实 RIP 横截面材料栈对齐

状态：DONE

内容：

```text
基于 DIAGRAM_12A_指甲模型横截面材料示意图.png 和真实 RIP slice.446.png，
建立横截面材料栈 golden summary。
材料栈包括：
上表面支撑、表面光油、表面色彩、模型内部填充、表面色彩、内表面光油、下表面支撑。
```

完成记录：

```text
已实现：
1. 新增 reports/cross_section_material_stack_report.json；
2. manifest.reports.crossSectionMaterialStack 指向该报告；
3. slice_report.totals.crossSectionMaterialStack 内嵌同一份材料栈摘要；
4. 材料栈顺序固定为：
   UpperSurfaceSupport > SurfaceVarnish > SurfaceColor > ModelFill > SurfaceColor > InnerSurfaceVarnish > LowerSupport；
5. 报告显式记录真实参考图、真实 RIP slice.446.png、旧概念图 geometry acceptance=deprecated；
6. 报告区分 surfaceVarnish 与 outerVarnishShell，避免把模型表面光油和外侧扩张光油壳层混淆；
7. 报告如实标记当前 legacy pipeline 尚未拆分 outer/inner RGB 独立 mask，当前二者均由 textureSurfacePixels 解释；
8. 新增 tests/golden/expected/12a_cross_section_material_stack_summary.json；
9. 新增 scripts/run_12a_cross_section_material_stack_tests.ps1，默认验证主用例，可用 -IncludeRealObjMatrix 验证 model/obj 多模型。
```

验证：

```text
report/preview 能解释真实 RIP 横截面材料栈；
旧概念图不再作为几何验收依据。
```

验证记录：

```text
cmake --build build --config Debug --target slicer_cli rip_reader_test
powershell -ExecutionPolicy Bypass -File .\scripts\run_12a_cross_section_material_stack_tests.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_12a_cross_section_material_stack_tests.ps1 -IncludeRealObjMatrix

默认真实 OBJ fixture：
output/CrossSectionMaterialStackRealObj: PASS
upper=5912297 outerV=223934 rgb=761260 fill=2505888 innerV=18946 lower=9250391

model/obj 多模型验证：
output/12a10_validation/nai_you/package: PASS
upper=5912297 outerV=223934 rgb=761260 fill=2505888 innerV=18946 lower=9250391

output/12a10_validation/aishen/package: PASS
upper=7903050 outerV=233123 rgb=636654 fill=2119881 innerV=14746 lower=7106481

output/12a10_validation/yecan/package: PASS
upper=6045142 outerV=222674 rgb=878709 fill=1249933 innerV=25942 lower=11245167
```

### Task 12A-11 彩色/单材料一致性 fixture

状态：DONE

内容：

```text
对同一甲片模型建立彩色 Profile 与单材料 Profile 对比。
除打印材料一个是彩色、一个是单色外，几何轮廓、支撑、通道统计逻辑和层顺序应一致。
```

验证：

```text
layerCount/model mask/support mask/channel-statistics logic 可比较，差异只来自材料通道。
```

完成记录：

```text
已实现：
1. slice_report.totals.singleMaterialConsistency 输出成对比较 hint；
2. 新增 tests/golden/expected/12a_single_material_consistency_summary.json；
3. 新增 scripts/run_12a_single_material_consistency_tests.ps1；
4. 脚本基于同一 model/obj 模型生成 color_texture 与 single_material 两个 Profile；
5. 硬一致校验：
   grid.widthPx / grid.heightPx / layerCount / layerThicknessMm；
   每层 zMm / modelPixels / supportPixels / supportTypeStats；
   outerVarnishPixels / outerSurfaceVarnishPixels / innerSurfaceVarnishPixels / upperSurfaceSupportPixels；
   totals.modelPixels / supportPrintPixels / supportTypeStats；
6. 允许差异：
   textureSurfacePixels / modelFillPixels / rgbPrintPixels / whitePrintPixels / varnishPrintPixels / texture_report.*；
7. 每个 case 输出 output/12a11_validation/<case>/consistency_report.json 和总 summary。
```

验证记录：

```text
cmake --build build --config Debug --target slicer_cli rip_reader_test experimental_config_unit_tests
powershell -ExecutionPolicy Bypass -File .\scripts\run_12a_single_material_consistency_tests.ps1
build\Debug\experimental_config_unit_tests.exe

model/obj 多模型验证：
output/12a11_validation/nai_you/summary: PASS
layer=126 grid=290x573 model=3267148 support=15808697 outerV=457256 surfaceV=223934/18946 upper=5912297 colorRGB=761260 singleW=3267148

output/12a11_validation/aishen/summary: PASS
layer=145 grid=287x535 model=2756535 support=15429133 outerV=478380 surfaceV=233123/14746 upper=7903050 colorRGB=636654 singleW=2756535

output/12a11_validation/titian/summary: PASS
layer=118 grid=288x554 model=2002266 support=14244510 outerV=376863 surfaceV=184320/15190 upper=5099630 colorRGB=691264 singleW=2002266

output/12a11_validation/xiao_ma_damuzhi/summary: PASS
layer=112 grid=288x554 model=1733924 support=13681367 outerV=353341 surfaceV=172071/15088 upper=4539520 colorRGB=691443 singleW=1733924

output/12a11_validation/xiao_ma_shizhi/summary: PASS
layer=111 grid=288x627 model=1973130 support=15277642 outerV=399938 surfaceV=194821/16754 upper=4957789 colorRGB=781232 singleW=1973130

output/12a11_validation/xiao_ma_wumingzhi/summary: PASS
layer=111 grid=288x627 model=1973130 support=15277642 outerV=399938 surfaceV=194821/16754 upper=4957789 colorRGB=781452 singleW=1973130

output/12a11_validation/xiao_ma_xiaozhi/summary: PASS
layer=111 grid=288x627 model=1973130 support=15277642 outerV=399938 surfaceV=194821/16754 upper=4957789 colorRGB=740757 singleW=1973130

output/12a11_validation/xiao_ma_zhongzhi/summary: PASS
layer=111 grid=288x627 model=1973130 support=15277642 outerV=399938 surfaceV=194821/16754 upper=4957789 colorRGB=781447 singleW=1973130

output/12a11_validation/yecan/summary: PASS
layer=117 grid=287x713 model=2128642 support=18329906 outerV=458289 surfaceV=222674/25942 upper=6045142 colorRGB=747456 singleW=2128642

本地未跟踪真实模型临时验证：
output/12a11_validation/local_meigui_fudiao_04/summary: PASS
layer=139 grid=288x722 model=3690056 support=21697848 outerV=606089 surfaceV=296104/16984 upper=9387446 colorRGB=918463 singleW=3690056

说明：meigui_fudiao 目录当前为未跟踪本地模型数据，不写入 committed golden；脚本已新增 -ExtraModelPath / -SkipGoldenCases 以支持本地真实模型临时验证。
```

### Task 12A-12 UI preview 图例与像素探针联动

状态：PENDING

内容：

```text
让 LayerPreview/OverlayPreview 能显示 semantic/sourcePolicy。
```

验证：

```text
UI smoke，手动点击关键像素显示 RGB/W/S/V 与语义。
```

---

## 完成标准

```text
1. 12A 所有 P0/P1 任务完成；
2. 文档、配置、report、preview 一致；
3. 典型真实模型通过；
4. 旧 fixture 默认兼容；
5. 状态报告更新。
```
