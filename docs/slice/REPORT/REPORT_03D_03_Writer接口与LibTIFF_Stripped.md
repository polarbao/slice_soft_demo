# REPORT_03D-03 Writer 接口与 LibTIFF Stripped 当前状态

> 状态：COMPLETE
> 日期：2026-07-31
> 下一任务：03D-04 READY

## 1. 本任务边界

本任务只实现以下内容：

```text
提取 RGBWSV TIFF Writer 接口；
把既有手写 Writer 封装为 adapter；
在显式 libtiff 构建轨道实现 stripped Writer；
逐 strip 直接读取调用方像素 span，不构造整层 strip_data 副本；
保持 tiled 输出使用手写后端；
保持默认生产后端为 handwritten。
```

未实现：

```text
LibTIFF tiled Writer；
统一稳定错误码和失败注入；
sibling 临时文件、失败清理和原子替换；
完整正负向/多场景等价矩阵；
后端性能比较；
默认后端切换。
```

## 2. 实现结果

新增的 Writer 边界：

```text
ITiffWriter；
TiffWriterBackend；
CreateTiffWriter；
ResolveTiffWriterBackend；
WriteRgbwsvTiffWithConfiguredBackend；
HandwrittenTiffWriter；
LibTiffWriter。
```

生产入口 `write_rgbwsv_tiff` 已改为通过 Factory 路由。当前行为为：

| 构建轨道 | stripped | tiled |
|---|---|---|
| handwritten | handwritten | handwritten |
| libtiff | LibTIFF | handwritten fallback |

LibTIFF stripped Writer 固定：

```text
Classic TIFF；
RGBWSV 六通道；
uint8；
black_is_print；
COMPRESSION_NONE；
PHOTOMETRIC_RGB；
PLANARCONFIG_CONTIG；
SAMPLEFORMAT_UINT；
3 个 EXTRASAMPLE_UNSPECIFIED；
显式 rowsPerStrip；
ImageDescription=RGBWSV；
Software=slice_soft_demo p0。
```

写入循环按 strip 计算源偏移和有效行数，将调用方像素 span 直接传给
`TIFFWriteEncodedStrip`。最后一个非满 strip 使用真实字节数，不补整 strip。

## 3. 兼容修正

LibTIFF 4.7.1 会在数值能由 16 bit 表达时，把部分合法 TIFF 整数字段写为 `SHORT`，
例如 `ImageWidth`、`ImageLength`、`RowsPerStrip` 和较小的 `StripByteCounts`。既有手写
Writer 对这些字段固定写 `LONG`。

TIFF 合同允许这些字段使用 `SHORT` 或 `LONG`，因此当前严格 Reader 和 tag contract
测试已改为接受两种无符号整数编码，再比较数值和像素。该修正：

```text
不改变 p0.rgbwsv.2；
不改变通道顺序、位深、极性和压缩方式；
不放宽必需 tag、storage 或像素校验；
只消除对合法整数物理类型的手写后端特有假设。
```

## 4. 能力元数据

LibTIFF 构建的能力自检和 Runtime manifest 现在输出：

```text
configuredBackend=libtiff；
libtiffDependencyAvailable=true；
libtiffWriterImplemented=true；
libtiffStrippedWriterImplemented=true；
libtiffTiledWriterImplemented=false；
libtiffVersion=4.7.1。
```

handwritten 构建继续输出：

```text
configuredBackend=handwritten；
libtiffDependencyAvailable=false；
libtiffWriterImplemented=false。
```

## 5. 验证证据

### 5.1 定向 CTest

handwritten Release：

```text
tiff_writer_contract_unit_tests PASS；
tiff_backend_build_info_unit_tests PASS；
tiff_writer_backend_unit_tests PASS。
```

LibTIFF Release：

```text
tiff_writer_contract_unit_tests PASS；
tiff_backend_build_info_unit_tests PASS；
tiff_writer_backend_unit_tests PASS。
```

LibTIFF Debug：

```text
tiff_writer_contract_unit_tests PASS；
tiff_backend_build_info_unit_tests PASS；
tiff_writer_backend_unit_tests PASS。
```

`tiff_writer_backend_unit_tests` 同时验证：

```text
手写 adapter 像素精确回读；
LibTIFF stripped 像素精确回读；
最后 strip 非满；
03D-03 tiled 仍回退 handwritten；
未链接 LibTIFF 时不得创建 LibTIFF Writer。
```

### 5.2 Package/RIP

使用 LibTIFF Release `slicer_cli` 生成
`samples/configs/golden/material_process_top2_fixture.json`：

```text
20 层 package 生成 PASS；
rip_reader_test --quiet PASS；
严格 Reader 可读取 LibTIFF stripped 输出。
```

### 5.3 Runtime

隔离 Runtime 部署 PASS：

```text
tiff.dll 已部署；
licenses/libtiff.txt 已部署；
Runtime 内 slicer_cli 能力自检 PASS；
Runtime 内 rip_reader_test PASS；
manifest 中 stripped=true、tiled=false。
```

一次复用部署目录时发生 Windows 目录原子移动 `Access denied`，换用全新隔离 Runtime
目录后通过。该问题属于 Runtime 发布目录占用诊断项，不是 Writer 功能失败；03D-04 的
失败清理设计不得把它与 TIFF 文件原子发布混为一谈。

## 6. 默认与回滚

`slicesoft-main` 仍配置 `SLICESOFT_TIFF_BACKEND=handwritten`。LibTIFF 只能通过
`slicesoft-libtiff` 或显式 CMake cache 启用。当前不存在默认生产切换。

回滚方式仍为：

```text
使用 handwritten 构建轨道；
Factory 对 stripped/tiled 均选择 HandwrittenTiffWriter；
package schema、配置和 Reader 无需回滚。
```

## 7. 03D-04 准备度

03D-04 的前置 Gate 已关闭，现有 DEV/DEMO/TASKS 已明确：

```text
单 tile scratch；
边界 tile 255 padding；
稳定错误码；
sibling 临时文件；
写入失败清理；
正式文件原子替换；
handwritten 默认保持不变。
```

因此 `03D-04 LibTIFF tiled 与错误模型` 准备状态为 `READY`。其代码尚未开始，
不得把本报告视为 tiled、原子失败或完整负向矩阵已完成。
