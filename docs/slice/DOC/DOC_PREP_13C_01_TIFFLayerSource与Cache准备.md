# DOC_PREP_13C-01 TIFF Layer Source 与 Cache 准备

> 文档状态：READY FOR DEVELOPMENT / SCHEDULE AFTER IDENTITY WAVE
> 版本：v1.0
> 日期：2026-07-27
> 对应任务：13C-01

## 1. 目标

建立以 package manifest 和 RGBWSV TIFF 为权威的数据源，提供异步加载、同层身份、取消和有界
LRU。13C-01 不实现伪彩合成、不删除旧 Preview Panel，也不修改生产 TIFF。

## 2. 当前代码事实

```text
slicer_core::read_rgbwsv_tiff 已支持 stripped/tiled、六通道 uint8；
LayerPreviewDataProvider 已从 manifest 建立 layerIndex；
LayerPreviewPanel::RenderProductionRgb 和像素探针会各自同步解码 TIFF；
W/S/V 与 Overlay 仍主要依赖 preview PNG；
当前没有统一 layer buffer、cache、取消或 request generation；
UI 主线程存在重复 TIFF 解码风险。
```

## 3. 模块边界

建议新增：

```text
src/slicer_core/preview/ProductionLayerRef.h/.cpp；
src/slicer_core/preview/TiffLayerSource.h/.cpp；
src/slicer_core/preview/TiffLayerCache.h/.cpp；
apps/slicer_debug_ui/services/TiffLayerLoadWorker.h/.cpp。
```

依赖方向：

```text
TiffLayerSource -> manifest DTO + tiff_io；
TiffLayerCache -> 纯 STL buffer；
Qt Worker -> TiffLayerSource/Cache；
Widget -> Qt Worker result；
slicer_core 不依赖 QImage、QObject、QString 或 Qt event loop。
```

## 4. 核心合同

`ProductionLayerRef`：

```text
packageidentity；
manifesthash；
layerindex；
zmm；
path；
width/height；
storage；
checksum；
dpix/dpiy。
```

`RgbwsvLayerBuffer`：

```text
sourceidentity；
layerindex/zmm；
width/height；
连续 RGBWSV pixels；
channelstats；
channelchecksums；
decodedbytes。
```

Public API：

```text
IndexPackage(manifestPath)；
LoadLayer(layerRef, cancellationRequested)；
FindLayer(layerIndex)；
ClearPackage(packageIdentity)。
```

所有 Public 接口使用 Doxygen；类和函数 PascalCase；成员变量 `m_xxx`；C++ Allman。

## 5. Manifest 与协议校验

只允许读取 manifest 列出的层。加载时必须校验：

```text
schema=p0.rgbwsv.2；
channelOrder=R G B W S V；
bitDepth=8；
polarity=black_is_print；
samplesPerPixel=6；
width/height 与 manifest/grid 一致；
layerIndex 唯一且顺序可映射；
TIFF 路径位于 package 范围；
stripped/tiled 为 Reader 支持模式。
```

缺层或坏层不得跨层寻找替代文件。

## 6. Cache

P0 采用有界 LRU：

```text
默认 maxLayers=5；
默认 maxBytes=256 MiB；
key=packageIdentity+manifestHash+layerIndex+checksum；
value=原始六通道 buffer；
缓存命中不重复解码；
单层超过 maxBytes 时允许返回给当前请求，但不进入 cache；
切换 package、manifest hash 改变或 checksum 改变时失效；
不得一次性预载全部层。
```

LRU 只负责 buffer 生命周期，不持有 QObject 或 Widget。

## 7. 异步与 stale

Qt Worker 请求至少包含：

```text
packageIdentity；
layerIndex；
requestGeneration；
consumerId；
cancellation token。
```

规则：

```text
用户快速滑层时允许取消旧任务；
无法中止底层文件读时，完成后按 generation 丢弃；
切换 package 立即递增 generation 并清空对应 cache；
关闭窗口时停止投递结果；
结果回主线程后再次比对 packageIdentity/layerIndex/generation；
不得把旧层图像显示为当前层。
```

## 8. 稳定错误

至少冻结：

```text
TIFF_LAYER_PACKAGE_NOT_FOUND；
TIFF_LAYER_MANIFEST_INVALID；
TIFF_LAYER_NOT_LISTED；
TIFF_LAYER_PATH_ESCAPE；
TIFF_LAYER_FILE_MISSING；
TIFF_LAYER_PROTOCOL_MISMATCH；
TIFF_LAYER_DIMENSION_MISMATCH；
TIFF_LAYER_READ_FAILED；
TIFF_LAYER_CANCELLED；
TIFF_LAYER_STALE_RESULT。
```

错误包含 package/layer/path/source code；UI 再映射中文文案。

## 9. 测试与 fixture

建议新增：

```text
tests/unit/tiff_layer_source/Main.cpp；
tests/unit/tiff_layer_cache/Main.cpp。
```

复用现有 writer/read fixture，覆盖：

```text
stripped/tiled；
首层/中间层/末层；
非连续但 manifest 合法的 layerIndex；
坏 schema/bitDepth/channelOrder/polarity；
缺文件/路径逃逸/尺寸不一致；
cache hit/miss/eviction；
单层大于 maxBytes；
package 切换；
checksum/manifest 变化；
cancel/stale generation；
600/600 和 635/600 元数据保持。
```

## 10. 验证命令

实现任务至少运行：

```powershell
cmake --build build --config Debug --target tiff_layer_source_unit_tests tiff_layer_cache_unit_tests
ctest --test-dir build -C Debug -R "tiff_layer_(source|cache)_unit_tests" --output-on-failure
cmake --build build --config Debug --target rip_reader_test
git diff --check
```

本准备任务只生成文档，未运行上述未来代码验证。

## 11. 后续 Gate

```text
13C-01 PASS -> 13C-02 MaterialPreviewComposer；
13C-02 PASS -> 13C-03 统一生产预览；
13C-03 PASS -> 允许 12E-09A-05 和 12E-10A；
13C-04 才允许默认关闭重复生产 preview PNG；
13C-05 才宣称 13C 收口。
```

