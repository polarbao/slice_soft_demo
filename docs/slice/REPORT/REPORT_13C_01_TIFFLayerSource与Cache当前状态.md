# REPORT 13C-01 TIFF Layer Source 与 Cache 当前状态

> 文档状态：COMPLETE / 13C-02 READY
> 日期：2026-07-28
> 前置：13B-07 FUNCTIONAL MATRIX COMPLETE
> 下一任务：13C-02 MaterialPreviewComposer

## 1. 阶段结论

13C-01 已建立以 `manifest.json` 和生产 RGBWSV TIFF 为权威来源的无 Qt 核心层数据源、默认
5 层/256 MiB 有界 LRU，以及可供后续 UI 接线复用的 Qt 异步 Worker。

本阶段没有实现伪彩材料合成、没有替换现有 Preview Panel、没有关闭 preview PNG，也没有改变
生产 TIFF、`p0.rgbwsv.2` 或 RIP 协议。

## 2. 核心实现

新增核心 DTO：

```text
ProductionLayerRef；
ProductionPackageIndex；
RgbwsvLayerBuffer；
TiffLayerLoadControl / TiffLayerLoadResult。
```

`TiffLayerSource` 现在负责：

```text
只索引 manifest 根层表和 tiff.layers 一致列出的 TIFF；
保留真实 layerIndex、zMm、dpiX/dpiY 和 stripped/tiled；
严格校验 p0.rgbwsv.2、R G B W S V、uint8、black_is_print；
拒绝 package 路径逃逸、缺失文件、维度和 TIFF 协议不一致；
调用共享 read_rgbwsv_tiff 解码连续六通道数据；
不跨层查找替代 TIFF；
在 manifest、package、文件身份或 request generation 变化时 fail-closed；
取消或 stale 结果不进入 cache。
```

稳定错误码已冻结：

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

## 3. Cache 与异步边界

`TiffLayerCache`：

```text
默认 maxLayers=5；
默认 maxBytes=256 MiB；
key=packageIdentity+manifestHash+layerIndex+file identity；
value=不可变原始 RGBWSV buffer；
命中时不重复解码；
超出单层字节预算时只服务当前请求，不写入 cache；
按层数和字节数共同执行 LRU 驱逐；
切换或重新索引 package 时清理旧身份。
```

`TiffLayerLoadWorker`：

```text
通过 Qt 全局线程池执行核心 TIFF 解码；
每个请求携带 consumerId、真实 layerIndex 和 generation；
新请求会取消旧逻辑请求；
Worker 完成和回到 UI 线程时都会检查 stale；
窗口析构后不再投递结果；
错误码与中文 UI 文案保持分层。
```

该 Worker 本阶段仅建立可复用服务边界；把生产预览 Widget 接到该 Worker 属于 13C-03。

## 4. 自动化覆盖

新增：

```text
tiff_layer_source_unit_tests；
tiff_layer_cache_unit_tests。
```

覆盖：

```text
stripped/tiled；
非连续真实 layerIndex；
635/600 DPI 元数据；
六通道原始字节、统计和 checksum；
首次 decode / 再次 cache hit；
层数和字节双预算 LRU；
超大单层不入 cache；
切换 package 和 manifest 变化失效；
坏 schema/bitDepth/polarity；
路径逃逸、缺文件、尺寸不一致；
取消和 stale generation 不污染 cache；
缺层不跨层兜底。
```

## 5. 实际验证

2026-07-28 实际运行：

```powershell
cmake --build build --config Debug --target tiff_layer_source_unit_tests tiff_layer_cache_unit_tests rip_reader_test slicer_debug_ui
ctest --test-dir build -C Debug -R "tiff_layer_(source|cache)_unit_tests" --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
powershell -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1 -BuildDir build -Config Debug
```

结果：

```text
13C-01 定向 CTest：2/2 PASS；
Debug 全量构建：PASS；
Debug 全量 CTest：74/74 PASS；
Qt UI self-test：PASS；
Quick CI：PASS，包含 regression、golden、UI self-test 和真实 Overlay smoke；
RIP Reader target：PASS。
```

## 6. 未完成与下一 Gate

仍未实现：

```text
R/G/B/W/S/V 和组合伪彩合成；
RGB+S+W+V；
生产预览 Widget 统一；
从 TIFF 驱动现有 UI 层滑块和像素探针；
默认关闭重复 preview PNG；
无 preview 目录 UI smoke。
```

下一原子任务为 `13C-02 MaterialPreviewComposer`。其输入合同、缓存和异步身份前置已满足；
13C-03 才负责把合成结果接入统一生产预览。
