# REPORT_03D-05 兼容矩阵与共享 Package Gate

> 状态：COMPLETE / PASS
> 日期：2026-07-31
> 默认生产 Writer：handwritten（未切换）

## 1. 任务目标

03D-05 在不改变 `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、无压缩、
contiguous、Classic TIFF 和单 IFD 合同的前提下，完成 handwritten 与 LibTIFF
双后端的功能等价、严格读取、坏包和共享 Package 回归 Gate。

本任务不执行性能结论、不启用压缩、不切换默认 Writer。

## 2. 实现内容

### 2.1 Writer 等价矩阵

新增 `tiff_writer_equivalence_unit_tests`，在同一 LibTIFF 构建中显式调用两个 Writer，
覆盖 stripped/tiled 与以下六类确定性像素：

```text
全空；
RGB；
W；
S；
V；
RGBWSV 混合及 partial 8-bit 值。
```

每个用例校验：

```text
解码后 RGBWSV 逐字节等于输入；
两后端 channel checksum 与 channel stats 相同；
width/height/storage/rowsPerStrip/tile 尺寸相同；
required TIFF tags 存在；
Classic TIFF、单 IFD；
非整 tile 尺寸的有效像素不受 255 padding 影响。
```

TIFF 文件本身的字节序列和 SHA-256 不要求相同，因为合法 Writer 可以使用不同的
tag 顺序、SHORT/LONG 表达和 offset 布局。

### 2.2 严格 Reader 兼容修正

LibTIFF 在两个 tile 的 `TileByteCounts`/`TileOffsets` 等场景中，可以把两个 SHORT
值以内联 4 字节形式存入 IFD。项目 Reader 原先只把 `count == 1` 当作内联值，因而会
把合法的两个 SHORT 误读为文件 offset。

当前 Reader 按 TIFF 字段总字节数判断：总负载不超过 4 字节时从 IFD value 区逐项
解包；超过 4 字节时才按 offset 读取。这不放宽 tag 数值、存储模式或 RGBWSV 像素校验。

### 2.3 共享 Package Gate

新增 `scripts/Run03DTiffCompatibilityGate.ps1`，统一执行：

```text
handwritten 与 LibTIFF 的 Writer/等价测试；
RGBWSV production package writer；
Global Surface Shell production pipeline；
multi-model package writer；
multi-model production service；
Legacy stripped/tiled 实际 package；
现有 RIP Reader strict；
18 个固定坏包及稳定 ValidationErrorCode；
03D-04 原子发布与失败清理回归。
```

`multi_model_production_service_unit_tests` 的显式 build volume 已改为基于 fixture
实际包围盒构建，并把源模型归一到 lower-left 坐标系。该修正消除了测试夹具对源模型
绝对坐标的隐式假设，没有放宽生产 build-volume 准入。

## 3. 验证结果

执行命令：

```powershell
.\scripts\Run03DTiffCompatibilityGate.ps1 -Config Release
```

结果：

```text
handwritten：8/8 CTest PASS；
LibTIFF：8/8 CTest PASS；
LibTIFF Debug：8/8 CTest PASS；
handwritten Legacy stripped/tiled：package + RIP strict PASS；
LibTIFF Legacy stripped/tiled：package + RIP strict PASS；
18/18 bad package：预期错误码 PASS；
03D-05 compatibility gate：PASS。
```

运行日志位于忽略的本地证据目录：

```text
output/benchmarks/03d_05/compatibility-gate-release.log
```

补充运行 `scripts/run_ci_quick.ps1` 时，流程在既有 support bridge schema 断言
`bridge support_shape_report expected bridgedGaps` 处停止。该失败发生在支撑报告 fixture，
不属于本任务 TIFF Writer/Reader 改动的通过证据；本报告不把 Quick CI 记录为 PASS，后续需由
支撑专项单独复核该 Golden 期望与当前算法行为。

## 4. 协议与边界结论

```text
schema：p0.rgbwsv.2，未改变；
通道：R G B W S V，未改变；
位深：uint8，未改变；
极性：black_is_print，0 打印、255 不打印，未改变；
存储：uncompressed contiguous stripped/tiled，未改变；
默认 Writer：handwritten，未改变；
LibTIFF：仍为显式 opt-in；
历史非标准 tile：仍走 handwritten compatibility route。
```

## 5. 后续任务准备度

`03D-06 Release 性能矩阵` 的协议前置已满足，状态可由 PREPARED 提升为 READY。下一任务
必须在同 buffer、同机器、同磁盘、独立进程条件下比较 handwritten/LibTIFF 的
stripped/tiled p50、p95 和峰值内存，并形成 `GO DEFAULT / GO OPTIONAL / NO-GO` 结论。

在 03D-06 得出 `GO DEFAULT` 且用户再次明确授权前，不得执行 03D-07 默认切换。
