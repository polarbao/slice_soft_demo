# REPORT_03D-01 手写 TIFF Writer 合同与性能基线

> 状态：COMPLETE
> 日期：2026-07-31
> 范围：当前手写 Writer 的合同冻结与 Writer-only Release 基线

## 1. 任务边界

本任务只建立迁移前基线，不接入 LibTIFF，不修改生产 Writer 行为，不切换默认后端。

固定生产协议：

```text
channelOrder=R,G,B,W,S,V
bitDepth=8
planarConfig=contiguous
polarity=black_is_print
printValue=0
emptyValue=255
compression=none
```

## 2. 新增验证资产

| 资产 | 作用 |
|---|---|
| `tiff_writer_contract_unit_tests` | 解析实际 TIFF tag，验证 stripped/tiled、边界 padding、像素和错误文本 |
| `tiff_writer_benchmark` | 对预生成的同一 RGBWSV buffer 只计 Writer 调用耗时 |
| `Run03DTiffWriterBaseline.ps1` | 统一执行 Release build、CTest、5 次 benchmark 和 RIP strict |

合同测试冻结的主要 tag：

```text
ImageWidth/ImageLength
BitsPerSample=8 x 6
Compression=1
Photometric=RGB
SamplesPerPixel=6
PlanarConfiguration=contiguous
ExtraSamples=unspecified x 3
SampleFormat=unsigned integer x 6
ImageDescription=RGBWSV
Software=slice_soft_demo p0
StripOffsets/RowsPerStrip/StripByteCounts
TileWidth/TileLength/TileOffsets/TileByteCounts
```

## 3. Release 基线

命令：

```powershell
.\scripts\Run03DTiffWriterBaseline.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -Iterations 5
```

输入为预先生成的 `1024 x 2048 x 6` uint8 buffer，共 `12,582,912` 字节。计时不包含
buffer 生成、Reader 解码校验、preview、report 和 package 写入。

| 存储方式 | p50 | p95 | min | max | 输出字节 | 写后工作集 |
|---|---:|---:|---:|---:|---:|---:|
| stripped | 9.574 ms | 11.217 ms | 8.468 ms | 11.217 ms | 12,583,406 | 16,982,016 B |
| tiled | 24.057 ms | 43.504 ms | 23.777 ms | 43.504 ms | 12,583,418 | 17,043,456 B |

这些数字只代表本机本次 Release 样本，不是跨机器性能承诺。03D-06 必须使用同一 benchmark
重新采集两个后端，才能决定 LibTIFF 是否适合作为默认 Writer。工作集在全部测量写入之后、
Reader 校验之前采样；进程累计峰值仍写入 JSON，但因后续 case 会继承前序峰值，不作为独立
case 的比较值。03D-06 的峰值 Gate 必须用独立进程隔离每个后端和存储方式。

## 4. 验证结果

```text
Release build：PASS
tiff_writer_contract_unit_tests：PASS
stripped decodedPixelsExact：PASS
tiled decodedPixelsExact：PASS
边界 tile 255 padding：PASS
GoldenMaterialProcessTop2 package：生成 PASS
rip_reader_test strict：PASS
git diff --check：PASS
```

## 5. 当前结论

03D-01 完成，手写 Writer 仍是唯一生产默认后端。03D-02 已具备文档准备，但涉及 vcpkg、
CMake link、Runtime DLL 和许可证部署，必须获得明确授权后才能实施。
