# REPORT_03D-06 LibTIFF Release 性能矩阵与判定

> 状态：COMPLETE / GO_OPTIONAL
> 日期：2026-08-03
> 默认 Writer：handwritten（未改变）

## 1. 结论

03D-06 已在同一台 Windows 机器、同一磁盘、Release 配置和相同确定性 RGBWSV
预生成像素输入上，以独立进程比较 handwritten 与 LibTIFF Writer。功能兼容 Gate
保持 PASS，但 LibTIFF 未达到默认切换所需的稳定 p50 改善门槛，因此结论为：

```text
GO_OPTIONAL
```

LibTIFF 继续作为显式可选后端保留；默认生产 Writer 不切换，03D-07 不进入默认切换开发。

## 2. 实现范围

```text
tiff_writer_benchmark：
  从编译能力读取 configured/effective backend 和 LibTIFF 版本；
  支持单独运行 stripped 或 tiled；
  记录 wall/CPU p50、p95、输出字节、失败数、working set 和进程累计峰值；
  按后端估算项目自有 staging buffer；
  open/tag/write/close 无可靠拆分时显式记录 not_available；
  保持 03D-01 JSON schema 向后兼容。

run_03d_libtiff_writer_matrix.ps1：
  每个 backend/storage/case/cache-condition 启动独立进程；
  先运行 03D-05 双后端兼容 Gate；
  校验 Release、真实后端、exact decode、内存、版本和报告字段；
  汇总 GO_DEFAULT_CANDIDATE / GO_OPTIONAL 判定；
  不修改默认后端。
```

## 3. 测试环境

| 项目 | 值 |
|---|---|
| 机器 | LAP-COM |
| 操作系统 | Microsoft Windows 10.0.26200 |
| CPU | Intel64 Family 6 Model 170 Stepping 4 |
| 编译 | Release / MSVC 1951.195136252 |
| LibTIFF | 4.7.1 |
| handwritten build | `build-slicesoft/main` |
| LibTIFF build | `build-slicesoft/03d-libtiff` |

原始矩阵位于本机忽略输出：

```text
output/benchmarks/03d_06/20260803_104021_490/tiff_writer_matrix.json
schema=slicesoft.tiff_writer_matrix.03d.1
```

`warm` 表示新输出目录加一次不计时预热；`cold_output_directory` 表示新输出目录且不预热，
但没有强制清空操作系统磁盘缓存，因此不得解释为物理冷盘测试。

## 4. Writer-only 结果

正数改善率表示 LibTIFF 更快，负数表示 LibTIFF 更慢。

| Case | 条件 | 存储 | handwritten p50 | LibTIFF p50 | p50 改善 | handwritten p95 | LibTIFF p95 | 峰值内存比 |
|---|---|---|---:|---:|---:|---:|---:|---:|
| small_fixture | warm | stripped | 3.002 ms | 4.707 ms | -56.770% | 8.132 ms | 5.509 ms | 0.8985 |
| reality_single | warm | stripped | 3.241 ms | 5.151 ms | -58.937% | 4.554 ms | 6.027 ms | 0.9242 |
| multi_model | warm | stripped | 5.945 ms | 6.514 ms | -9.557% | 7.482 ms | 7.627 ms | 0.6784 |
| non_integral_tile | warm | tiled | 3.825 ms | 4.719 ms | -23.366% | 4.467 ms | 5.026 ms | 0.9560 |
| small_fixture | cold output dir | stripped | 3.318 ms | 4.555 ms | -37.250% | 4.986 ms | 7.190 ms | 0.9078 |
| reality_single | cold output dir | stripped | 2.601 ms | 5.263 ms | -102.388% | 4.706 ms | 6.730 ms | 0.9230 |
| multi_model | cold output dir | stripped | 5.921 ms | 6.693 ms | -13.044% | 7.816 ms | 7.934 ms | 0.6782 |
| non_integral_tile | cold output dir | tiled | 4.398 ms | 5.078 ms | -15.460% | 5.321 ms | 8.861 ms | 0.9553 |

默认切换判定只使用 `reality_single` 与 `multi_model` 两个 warm stripped 主生产量级用例：

```text
最低 p50 改善：-58.937%，要求 >= 15%；未通过；
最大峰值内存比：0.9242，要求 <= 1.10；通过；
03D-05 compatibility：通过；
最终：GO_OPTIONAL。
```

## 5. 验证证据

```powershell
.\scripts\Run03DTiffWriterBaseline.ps1 `
  -BuildDir build-slicesoft/main -Config Release -Iterations 5 -SkipBuild

.\scripts\run_03d_libtiff_writer_matrix.ps1 `
  -HandwrittenBuildDir build-slicesoft/main `
  -LibTiffBuildDir build-slicesoft/03d-libtiff `
  -Config Release -SkipBuild
```

结果：

```text
03D-01 handwritten baseline PASS；
handwritten 兼容 CTest 8/8 PASS；
LibTIFF 兼容 CTest 8/8 PASS；
两后端 Legacy stripped/tiled Package + RIP strict PASS；
18/18 bad package 预期错误码 PASS；
16 个独立 Writer-only 测量进程全部 exact decode，failureCount=0；
矩阵脚本 PASS，decision=GO_OPTIONAL。
```

## 6. 边界与下一步

```text
p0.rgbwsv.2、RGBWSV、uint8、contiguous、black_is_print 未改变；
未启用压缩、BigTIFF、多 IFD、planar separate 或并行写层；
未修改生产 Profile；
默认 Writer 继续为 handwritten；
LibTIFF Runtime/许可证和显式构建轨道继续保留。
```

03D-07 只能按 `GO_OPTIONAL` 路径做阶段收口：记录可选后端、回滚和使用边界，不得执行
默认切换。若未来要重新申请默认切换，必须在固定参考机上重新获得性能 Gate，并再次取得
用户明确授权。
