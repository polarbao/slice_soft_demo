# DEV_03D LibTIFF Writer 双后端迁移设计

> 文档状态：IMPLEMENTATION DESIGN READY
> 日期：2026-07-31

## 1. 当前实现盘点

```text
src/slicer_core/tiff_io.cpp：
  手写 Classic TIFF Writer/Reader；
  Writer 支持 stripped/tiled、contiguous、uint8、六通道、无压缩；
  Reader 是 RIP strict 和 UI TIFF Source 的底层之一。

src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.cpp：
  构造 TiffImageSpec；
  调用 write_rgbwsv_tiff；
  负责 staging、manifest、preview/report 和原子发布。

vcpkg.json：
  已列出 tiff；
  未锁定 builtin-baseline；
  当前 CMake 未查找或链接 TIFF target。
```

LibTIFF 迁移只替换 Writer 实现。现有严格 Reader 在 R1-R4 保持独立，以避免“同一库写、
同一库读”掩盖错误。

## 2. 模块设计

建议新增：

```text
src/slicer_core/output/tiff/ITiffWriter.h
src/slicer_core/output/tiff/TiffWriterTypes.h
src/slicer_core/output/tiff/HandwrittenTiffWriter.cpp
src/slicer_core/output/tiff/LibTiffWriter.cpp
src/slicer_core/output/tiff/TiffWriterFactory.cpp
```

公共请求：

```text
path；
TiffImageSpec；
span<const uint8_t> RGBWSV pixels；
backend；
```

公共结果：

```text
backend；
bytesWritten；
storageMode；
timing；
diagnosticCode；
```

`RgbwsvPackageWriter` 只依赖 Writer 接口，不包含 LibTIFF 头文件。

## 3. LibTIFF tag 映射

| 当前合同 | LibTIFF 设置 |
|---|---|
| width | `TIFFTAG_IMAGEWIDTH` |
| height | `TIFFTAG_IMAGELENGTH` |
| 8 bits x 6 | `TIFFTAG_BITSPERSAMPLE=8` |
| RGBWSV 六通道 | `TIFFTAG_SAMPLESPERPIXEL=6` |
| RGB photometric | `TIFFTAG_PHOTOMETRIC=PHOTOMETRIC_RGB` |
| 无压缩 | `TIFFTAG_COMPRESSION=COMPRESSION_NONE` |
| contiguous | `TIFFTAG_PLANARCONFIG=PLANARCONFIG_CONTIG` |
| unsigned integer | `TIFFTAG_SAMPLEFORMAT=SAMPLEFORMAT_UINT` |
| 3 extra samples | `TIFFTAG_EXTRASAMPLES={UNSPECIFIED,UNSPECIFIED,UNSPECIFIED}` |
| stripped | `TIFFTAG_ROWSPERSTRIP` + encoded strip writes |
| tiled | `TIFFTAG_TILEWIDTH/TILELENGTH` + encoded tile writes |
| 描述 | `TIFFTAG_IMAGEDESCRIPTION=RGBWSV` |
| 软件 | `TIFFTAG_SOFTWARE=slice_soft_demo p0` |

不得自动让 LibTIFF 选择 strip 大小、压缩或额外 tag；配置值是生产合同的一部分。

## 4. 写入算法

### 4.1 Stripped

```text
校验 spec 和 buffer；
打开 sibling 临时文件；
设置全部固定 tag；
逐 strip 计算源 span；
直接调用 TIFFWriteEncodedStrip；
关闭并检查错误；
原子重命名到目标层文件。
```

不再构造整层 `strip_data` 副本。

### 4.2 Tiled

```text
只分配一个 tile scratch buffer；
每 tile 先填 255；
复制有效图像区域；
调用 TIFFWriteEncodedTile；
复用 scratch 直到完成。
```

不再构造全部 tile 的整层缓冲。

LibTIFF 4.7.1 写入接口要求 `tileWidth/tileHeight` 为 16 的正整数倍。当前生产 Profile
使用 `256 x 256`，直接走 LibTIFF；历史合同 fixture 的 `8 x 4` 等非标准 tile 不得静默
改写配置，迁移期由 Factory 显式路由到 handwritten compatibility backend。零尺寸仍交由
LibTIFF Writer 校验并返回稳定输入错误，不得被兼容路由掩盖。

## 5. 错误和日志

LibTIFF 错误必须转换为项目稳定错误：

```text
tiff_open_failed；
tiff_tag_setup_failed；
tiff_strip_write_failed；
tiff_tile_write_failed；
tiff_close_failed；
tiff_output_validation_failed。
```

Writer 公共错误模型另外包含 `tiff_invalid_input` 和 `tiff_publish_failed`，分别覆盖进入
LibTIFF 前的合同校验，以及 sibling 临时文件完成后原子替换失败。错误 handler 必须通过
`TIFFOpenOptionsSetErrorHandlerExtR` 绑定到单个 handle，禁止安装进程级可变 handler。

不得把全局错误 handler 的可变状态暴露给多线程。优先使用带 client data 的 re-entrant
错误接口；若目标版本接口不足，必须在 Writer 层串行化 handler 安装并补并发测试。

## 6. CMake/vcpkg

迁移期建议：

```cmake
set(SLICESOFT_TIFF_BACKEND "handwritten" CACHE STRING "...")
set_property(CACHE SLICESOFT_TIFF_BACKEND PROPERTY STRINGS handwritten libtiff)
```

仅 `libtiff` 后端：

```text
find_package(TIFF REQUIRED)；
target_link_libraries(slicer_core PRIVATE TIFF::TIFF)；
定义 SLICESOFT_USE_LIBTIFF_WRITER=1。
```

vcpkg manifest 将 `tiff` 写为：

```json
{
  "name": "tiff",
  "default-features": false
}
```

正式改动时必须锁定 `builtin-baseline`。不得修改共享 vcpkg 根中其他项目的 classic 安装。

## 7. 兼容验证器

新增独立 `tiff_writer_equivalence` 测试工具：

```text
生成确定性 layer buffers；
分别写 handwritten/libtiff；
用现有 read_rgbwsv_tiff 解码；
比较 spec、像素、channel checksum/stats；
读取并比较必需 tag；
运行 rip_reader_test；
记录文件大小和 timing。
```

需要覆盖：

```text
全空；
单 RGB；
W/S/V 单通道；
多通道同像素；
partial value；
非整 tile 尺寸和 255 padding；
最后 strip 不满 rowsPerStrip；
非法尺寸/buffer；
写入目录不可用；
中途失败不得污染正式 package。
```

## 8. 性能工具

新增 `tiff_writer_benchmark`，输入一次生成后重复使用：

```text
小 fixture；
Reality 单模型 184 层量级；
多模型联合 Raster；
stripped/tiled；
handwritten/libtiff；
warm/cold output directory。
```

计时不包含 slice、preview、JSON 和 RIP。报告 schema 建议为
`slicesoft.tiff_writer_benchmark.03d.1`。

## 9. 线程与并发边界

首版不同时引入层级并行。先验证单线程 Writer 等价和单层内存优化。后续若增加受控并发，
必须独立评估：

```text
磁盘队列；
峰值内存；
LibTIFF handler 线程安全；
package 层号顺序；
取消和原子发布。
```

## 10. 回滚

```text
CMake 默认后端切回 handwritten；
不回滚 package schema、配置或报告；
LibTIFF 文件不作为唯一 Golden 二进制 hash；
保留 equivalence fixture，防止未来再次漂移。
```
