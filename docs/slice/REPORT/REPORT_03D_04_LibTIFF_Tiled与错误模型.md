# REPORT_03D-04 LibTIFF Tiled 与错误模型

> 状态：COMPLETE
> 日期：2026-07-31
> 下一任务：03D-05 READY

## 1. 任务目标

本任务在不切换默认 Writer 的前提下，补齐显式 LibTIFF 轨道的 tiled 写入和失败收口：

```text
单 tile scratch buffer；
非整 tile 图像边界以 255 填充；
稳定、可分类的 Writer 错误；
per-handle re-entrant LibTIFF error handler；
sibling 临时文件、原子替换和失败清理。
```

固定生产协议没有变化：`p0.rgbwsv.2`、RGBWSV、uint8、contiguous、无压缩、
`black_is_print`、Classic TIFF、单 IFD。

## 2. 实现结果

### 2.1 LibTIFF tiled

`LibTiffWriter` 现在根据 `TiffImageSpec.storage_mode` 写 stripped 或 tiled：

```text
设置 TILEWIDTH/TILELENGTH；
只分配一个 tile scratch；
每个 tile 写入前重置为 255；
逐行复制有效 RGBWSV 区域；
通过 TIFFWriteEncodedTile 写入并复用 scratch。
```

LibTIFF 4.7.1 要求 tile 宽高为 16 的正整数倍。生产 `256 x 256` 配置走 LibTIFF；
历史 `8 x 4` 等非标准合同 fixture 明确走 handwritten compatibility route，不会被静默改写。

### 2.2 稳定错误

新增 `TiffWriterErrorCode` 和 `TiffWriterException`，当前稳定标识包括：

```text
tiff_invalid_input；
tiff_open_failed；
tiff_tag_setup_failed；
tiff_strip_write_failed；
tiff_tile_write_failed；
tiff_close_failed；
tiff_output_validation_failed；
tiff_publish_failed。
```

LibTIFF 错误和 warning 通过 `TIFFOpenOptionsSetErrorHandlerExtR` / per-handle client data
捕获，不修改进程级全局 handler。

### 2.3 原子文件发布

LibTIFF Writer 先写目标同目录下的唯一 sibling 临时文件，flush/close 成功后再原子替换
正式 TIFF。Windows 使用 `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`。输入校验失败、
打开失败、写入失败或发布失败都会保留已有正式文件，并由 RAII 清理临时文件。

## 3. 兼容边界

```text
handwritten 仍是默认构建和生产后端；
slicesoft-libtiff 是显式 opt-in 轨道；
合法 16 对齐 tiled 走 LibTIFF；
历史非标准 tiled fixture 走 handwritten compatibility；
手写 Writer 的既有错误文本由显式 handwritten 合同测试继续冻结；
LibTIFF 使用新的稳定错误码，不要求与手写错误文本逐字相同。
```

## 4. 验证证据

### 4.1 TDD

先扩展测试，确认因缺少 `TiffWriterError.h` 编译失败；随后实现错误模型和 tiled Writer。

### 4.2 定向 CTest

handwritten Release、LibTIFF Release、LibTIFF Debug 均通过：

```text
tiff_writer_contract_unit_tests；
tiff_backend_build_info_unit_tests；
tiff_writer_backend_unit_tests。
```

测试覆盖：

```text
LibTIFF stripped/tiled decoded RGBWSV 逐字节一致；
13 x 5 图像写入 16 x 16 tile，原始边界 padding 全部为 255；
16 对齐 tiled 选择 LibTIFF，8 x 4 非标准 fixture 选择 handwritten compatibility；
无效 tiled 输入返回 tiff_invalid_input；
已有正式输出在失败后保持不变；
发布到目录触发 tiff_publish_failed，且 sibling temp 无残留；
build-info 报告 stripped=true、tiled=true。
```

### 4.3 真实 package/RIP

LibTIFF Release 使用 `samples/configs/storage_mode/storage_tiled_compat.json` 生成 20 层
`256 x 256` tiled package，随后执行：

```powershell
build-slicesoft/03d-libtiff/Release/rip_reader_test.exe `
  --package output/StorageTiledCompat --quiet
```

结果：`PASS output/StorageTiledCompat`。

能力自检结果：

```text
configuredBackend=libtiff；
libtiffVersion=4.7.1；
libtiffStrippedWriterImplemented=true；
libtiffTiledWriterImplemented=true。
```

## 5. 03D-05 准备度

03D-04 前置 Gate 已关闭。现有 PRD/DEV/DEMO/TASKS 已明确 03D-05 的范围：

```text
decoded pixel exact 和 required tags；
handwritten/libtiff、stripped/tiled 正向矩阵；
RIP strict 与 bad-package 负向矩阵；
Legacy/Global/scene package；
package 原子发布和失败保留。
```

因此 `03D-05 等价、坏包与共享 Package Gate` 状态由 `PREPARED / WAIT 03D-04`
更新为 `READY`。03D-06 性能矩阵仍等待 03D-05，不在本任务提前执行；03D-07 默认切换
仍需要性能 GO 和用户再次明确授权。
