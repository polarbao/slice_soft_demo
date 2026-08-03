# REPORT_03E-01 PackBits 压缩与读写性能矩阵

> 状态：COMPLETE / NO_GO_DEFAULT  
> 日期：2026-08-03  
> 证据：`output/benchmarks/03e_01/20260803_114312_477/tiff_compression_matrix.json`

## 1. 当前 TIFF 是否压缩

03E 修改前以及当前生产默认均未压缩：

```text
handwritten Writer: Compression(259)=1
LibTIFF Writer: COMPRESSION_NONE
TiffImageSpec 默认: compression_mode=None
```

本次新增 PackBits 可选能力，`Compression(259)=32773`。它已可由 Writer 核心和 benchmark
生成，但尚未接入生产 Profile/UI，正常生产写包仍输出未压缩 TIFF。

## 2. 实现结果

- handwritten Writer 支持 stripped/tiled PackBits；
- LibTIFF Writer 支持 stripped/tiled PackBits；
- 项目严格 Reader 支持 none/PackBits，并拒绝未知压缩值和畸形 PackBits 数据；
- 255 tile padding、六通道像素、checksum 和 stats 保持一致；
- benchmark 支持 `--compression`、`--pixel-pattern production_sparse` 和 `--measure-read`；
- 新增 `scripts/Run03ETiffCompressionMatrix.ps1`。

## 3. 测量口径

```text
Build: MSVC Release
Pattern: production_sparse，六通道、255 空白、模型 RGB/W、外围 S
Warmup: 1 次，不强制清空 OS 文件缓存
Write: handwritten / LibTIFF 两种 Writer
Read: 两种 Writer 输出均使用同一个项目 strict Reader
```

因此“读速度”表示不同 Writer 文件布局由项目 Reader 读取的速度，不是 handwritten Reader 与
LibTIFF Reader 的实现对比；当前项目没有 LibTIFF Reader backend。

## 4. PackBits 与未压缩对比

| Writer | Case | 存储 | 体积缩减 | 写 p50 变化 | 读 p50 改善 |
|---|---|---|---:|---:|---:|
| handwritten | reality_single | stripped | 58.847% | 慢 126.388% | 慢 6.083% |
| handwritten | multi_model | stripped | 58.786% | 慢 39.528% | 快 16.253% |
| handwritten | non_integral_tile | tiled | 66.930% | 慢 49.974% | 快 56.686% |
| LibTIFF | reality_single | stripped | 58.805% | 慢 14.302% | 快 32.426% |
| LibTIFF | multi_model | stripped | 58.740% | 慢 75.885% | 快 52.969% |
| LibTIFF | non_integral_tile | tiled | 66.894% | 快 13.408% | 快 54.486% |

结论：PackBits 稳定地显著减小文件；五组用例加快当前严格 Reader，一组小图 handwritten
用例受短时测量噪声影响反而慢 6.083%。压缩编码在五组用例增加写入耗时，仅 LibTIFF tiled
用例写入更快，因此不能将“文件更小”等价为“切片保存更快”。

## 5. PackBits 双 Writer p50

| Case | handwritten 写/读 ms | LibTIFF 写/读 ms | 判断 |
|---|---:|---:|---|
| reality_single | 7.9109 / 11.2059 | 7.3760 / 6.0363 | 本轮 LibTIFF 写快约 6.8%，其文件读取更快 |
| multi_model | 14.8888 / 66.6266 | 12.1299 / 44.2818 | 本轮 LibTIFF 写快约 18.5%，其文件读取更快 |
| non_integral_tile | 6.5255 / 5.9838 | 6.4429 / 5.9119 | 两者接近 |

本轮 PackBits 数据中 LibTIFF 均未落后，但此前同机预跑存在 handwritten tiled 更快的结果，
说明毫秒级写入容易受缓存和调度影响。当前证据足以否决“默认开启压缩”，不足以据此替换默认 Writer。
03D 的默认 handwritten 判定不因本次实验改变。

## 6. 决策

```text
兼容性：PASS
最小体积缩减：58.740%
最小读取 p50 改善：-6.083%
最大写入 p50 增幅：126.388%
决策：NO_GO_DEFAULT
生产默认压缩：未改变
生产默认 Writer：handwritten
```

建议下一步先使用真实完整 Package 和目标 RIP 验证，再决定是否增加生产配置开关。若目标优先级是
缩短写包时间，PackBits 暂不应默认启用；若优先级是减小磁盘、网络传输和后续重复读取成本，可继续
推进 03E-02。PackBits 原型仍作为可选实验能力保留，`NO_GO_DEFAULT` 不表示删除该能力。
