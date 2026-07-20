# 10 输出包、报告与 RIP 验证

## 1. 生产包结构

典型 legacy 包：

~~~text
SlicePackage/
  manifest.json
  layers/
    layer_00000.tif
    layer_00001.tif
    ...
  preview/
    ...
  reports/
    package_report.json
    model_report.json
    slice_report.json
    repair_report.json
    support_report.json
    support_shape_report.json
    preview_report.json
    texture_report.json
    material_policy_report.json
    material_process_report.json
    cross_section_material_stack_report.json
    material_closure_report.json
    material_role_mapping_report.json
    obj_mtl_material_report.json
    three_mf_report.json
    contour_report.json
    relief_report.json
~~~

具体文件以 `manifest.reports` 和当前 writer 为准；某些配置即使 report 内容为 unavailable，也仍会保留稳定路径。

## 2. Manifest 关键内容

`manifest.json` 包含：

- schema/schemaVersion；
- source config/model/format；
- width/height/layerCount；
- DPI、pixel size、layer thickness、origin；
- slicing mode；
- TIFF channel/order/bitDepth/storage/polarity；
- 有序 layer list；
- reports 路径；
- preview 文件列表。

层顺序必须以 manifest 的 layerIndex/list 为准，不要依赖文件系统字典序猜测。

## 3. TIFF 层

每个像素连续六个 8 位 sample，顺序固定 R/G/B/W/S/V。`stripped` 和 `tiled` 是存储组织差异，不改变材料语义。

检查 TIFF 时至少验证：

- 图像尺寸与 manifest grid 一致；
- sample count=6；
- bit depth=8；
- planar config=contiguous；
- storage 与 manifest 一致；
- stripped 的 rowsPerStrip 合法；
- tiled 的 tileSize 合法；
- 文件数量与 layerCount 一致。

## 4. 关键报告怎么读

| 报告 | 首先看什么 |
|---|---|
| model | format、BBox、三角形、退化面、UV、材质、3MF warning/error |
| slice | grid、像素 totals、通道统计、semantic、timing |
| texture | loaded/missing、sampled/fallback、UV out-of-range |
| support | mode、placement、SupportType stats、island、internal void |
| material process | Profile、生效策略、要求通道和验收 |
| material closure | source/confidence/status/productionAcceptance/remaining gap |
| preview | 生成层、格式、伪彩和真实 layerIndex |
| package | report base、source、schema |

报告是解释层，不是生产像素的替代品。报告说 PASS 但 TIFF/manifest 不合法时，包仍失败。

## 5. RIP Reader 验证

~~~powershell
.\build\Debug\rip_reader_test.exe --package output\SlicePackage --summary
~~~

输出摘要包含 schema、storage、grid、bitDepth、channelOrder、各通道 print pixel 和 warnings。

负向测试：

~~~powershell
.\build\Debug\rip_reader_test.exe --package <bad-package> --expect-error --expect-code E_...
~~~

reader 的错误类型覆盖 package/manifest/schema/channel/bitDepth/polarity/grid/layer/TIFF/storage 等问题。

## 6. Preview 为什么会“颜色相反”

生产 TIFF 使用 `black_is_print`。人眼预览通常把打印强度转成可见亮度，并对 W/S/V 使用伪彩。于是：

- 原始通道 0 表示打印；
- 直接当普通灰度图看会显得黑；
- UI 可能反相或用绿色/蓝色/灰色表示材料；
- preview 颜色不能被当作 TIFF 原始值。

相关专题可继续读 [TIFF RGB 与 UI 预览差异](../user_guides/TIFF_RGB与UI预览白色黑色差异分析.md)。

## 7. Hash 与不变性

当任务声明“repair disabled 不改变生产输出”或“legacy 回归必须不变”时：

1. 按 manifest 层顺序读取文件；
2. 对每层 TIFF 做 SHA-256；
3. 比较完整清单；
4. 报告文件允许因诊断新增而不同；
5. 不用文件时间、目录大小或预览图代替 TIFF hash。

## 8. 一个包何时可称为成功

至少满足：

~~~text
CLI 成功退出
完整 TIFF layer list 存在
manifest 与 TIFF 一致
固定协议不变
rip_reader_test strict PASS
材料/闭环/准入没有 blocker
输出来源与 mode 可追溯
~~~

Global 模式的 preview、diagnostic JSON 或内存 mask 成功，不满足这一定义。

## 9. 常见排查顺序

1. 检查配置 `packageDir` 是否与脚本期望一致；
2. 检查 CLI 退出码和最后一个 progress phase；
3. 检查 `manifest.json` 是否存在；
4. 用 reader 获取第一个稳定错误码；
5. 检查 model/texture/material/support/closure 报告；
6. 只在协议有效后分析视觉或工艺问题。
