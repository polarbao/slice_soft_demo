# DOC_PREP 13C-02 MaterialPreviewComposer 准备

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.0
> 日期：2026-07-28
> 对应任务：13C-02
> 前置：13C-01 COMPLETE

## 1. 目标

在 `slicer_core` 建立无 Qt 的确定性材料显示合成器。输入只接受 13C-01 已解码的同一层
`RgbwsvLayerBuffer`，输出连续 RGBA 显示像素、统计和六通道探针。13C-02 不接入 Widget、不读取
preview PNG、不改变生产 TIFF。

## 2. 当前代码事实

```text
TiffLayerSource 已按 manifest 真实 layerIndex/zMm/dpiX/dpiY 解码 RGBWSV；
TiffLayerCache 已缓存不可变六通道 buffer；
TiffLayerLoadWorker 已提供 consumer/generation/cancel/stale；
现有 LayerPreviewPanel 的 production RGB 直接显示 TIFF R/G/B 值；
现有 W/S/V 和 Overlay 仍主要依赖 preview PNG；
当前没有共享 MaterialPreviewComposer，也没有 RGB+S+W+V。
```

## 3. 模块边界

新增：

```text
src/slicer_core/preview/MaterialPreviewComposer.h/.cpp；
tests/unit/material_preview_composer/Main.cpp。
```

依赖方向：

```text
MaterialPreviewComposer -> RgbwsvLayerBuffer；
UI adapter -> MaterialPreviewResult -> QImage；
slicer_core 不依赖 QColor、QImage、QString、QObject。
```

## 4. Public DTO

计划冻结：

```text
MaterialPreviewMode：
  R / G / B / W / S / V；
  RGB；
  RGB+W / RGB+S / RGB+V；
  RGB+S+W+V；
  Occupancy；
  Empty。

PreviewColor：
  r/g/b/a uint8；

MaterialPreviewPalette：
  empty=(255,255,255,255)；
  R/G/B=对应通道色；
  W=(0,170,255,默认alpha)；
  S=(0,255,0,默认alpha)；
  V=(127,127,127,默认alpha)；
  occupancy=(80,80,80,255)。

MaterialPreviewRequest：
  mode + palette；

MaterialPreviewResult：
  sourceIdentity/layerIndex/zMm/width/height/dpiX/dpiY；
  连续 RGBA pixels；
  renderStats。

MaterialPixelProbe：
  x/y；
  R/G/B/W/S/V 原始生产值；
  hasRgb/hasW/hasS/hasV/isEmpty/multipleMaterials。
```

所有 Public API 使用 Doxygen；类与函数 PascalCase；成员变量 `m_xxx`；C++ Allman。

## 5. 固定像素语义

打印判断：

```text
hasR = R < 255；
hasG = G < 255；
hasB = B < 255；
hasRgb = hasR || hasG || hasB；
hasW = W < 255；
hasS = S < 255；
hasV = V < 255；
isEmpty = 六通道全部等于 255。
```

禁止：

```text
把 black_is_print 解释为整张 RGB 图反相；
把 RGB=(255,255,255) 单独解释为打印；
从 TIFF 推断 Texture Surface / Model Fill / Partition；
用其他 layerIndex 补当前层。
```

## 6. 合成规则

### 6.1 RGB

RGB 模式沿用已验证的生产显示：

```text
hasRgb=true：显示 TIFF 原始 (R,G,B)；
hasRgb=false：显示 Empty 背景；
不对生产 RGB 做全图反相。
```

### 6.2 单通道

通道打印覆盖率：

```text
coverage = 255 - channelValue；
effectiveAlpha = round(pseudoColor.alpha * coverage / 255)。
```

在 Empty 背景上按 straight-alpha 合成对应伪彩。值 0 表示完整伪彩，值 255 表示不显示。

### 6.3 组合模式

固定顺序：

```text
RGB -> W -> S -> V。
```

`RGB+W/S/V` 只叠加指定材料；`RGB+S+W+V` 按上述固定顺序叠加全部材料。显示顺序不等于
生产材料冲突优先级，像素探针始终保留全部原始通道。

### 6.4 Occupancy 与 Empty

```text
Occupancy：任一通道 <255 时显示 occupancy 色，否则显示 Empty；
Empty：六通道全 255 时显示 Empty 色，否则显示 occupancy 色。
```

## 7. 错误合同

至少冻结：

```text
MATERIAL_PREVIEW_BUFFER_INVALID；
MATERIAL_PREVIEW_DIMENSION_INVALID；
MATERIAL_PREVIEW_PIXEL_OUT_OF_RANGE；
MATERIAL_PREVIEW_MODE_INVALID。
```

错误不得被转换为白图成功结果。

## 8. 统计

每次合成记录：

```text
pixelCount；
rgbPixels；
whitePixels；
supportPixels；
varnishPixels；
occupiedPixels；
emptyPixels；
multiMaterialPixels。
```

统计基于生产通道，不受伪彩颜色和 alpha 影响。

## 9. 自动化验证

新增 `material_preview_composer_unit_tests`，覆盖：

```text
R/G/B/W/S/V；
RGB；
RGB+W、RGB+S、RGB+V；
RGB+S+W+V；
Empty/Occupancy；
0/partial/255 通道值；
同像素多材料和固定覆盖顺序；
六通道探针；
sourceIdentity/layerIndex/zMm/dpiX/dpiY 透传；
坏尺寸、坏字节数和探针越界；
输入 buffer 不被修改。
```

实现后至少运行：

```powershell
cmake --build build --config Debug --target material_preview_composer_unit_tests
ctest --test-dir build -C Debug -R "^material_preview_composer_unit_tests$" --output-on-failure
cmake --build build --config Debug --target tiff_layer_source_unit_tests tiff_layer_cache_unit_tests rip_reader_test
git diff --check
```

## 10. 非目标与后续 Gate

```text
13C-02 不创建 QImage；
不接入 LayerPreviewPanel/PreviewOverlayPanel；
不实现滑层 Worker 生命周期；
不删除旧 preview PNG；
不新增 Texture/Fill 诊断语义；
不改变 p0.rgbwsv.2。
```

`13C-02 PASS -> 13C-03 Unified Production Preview`。
