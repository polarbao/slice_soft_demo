# DOC_PREP_03D-06 LibTIFF Release 性能矩阵准备

> 状态：READY
> 日期：2026-07-31
> 前置：03D-05 compatibility gate PASS

## 1. 任务目的

03D-06 只回答 handwritten 与 LibTIFF 在相同预生成 RGBWSV layer buffer 上的 Writer
性能和内存差异，不比较切片、合成、preview、JSON 或 RIP 时间，也不切换默认 Writer。

## 2. 已满足前置

```text
03D-01 handwritten Writer-only Release 基线已冻结；
03D-02 Debug/Release LibTIFF 依赖与 Runtime 已闭环；
03D-03/04 stripped+tiled、错误和原子发布已实现；
03D-05 像素/tag、RIP、坏包和共享 Package Gate 已通过；
默认 Writer 仍为 handwritten。
```

## 3. 开发范围

现有 `tiff_writer_benchmark` 仍硬编码 `backend=handwritten`，且 staging 估算按旧手写
整层布局计算。03D-06 应最小化改造为：

```text
从编译能力读取 configuredBackend、LibTIFF 版本和 Writer 能力；
报告真实 backend，不允许硬编码；
每个 backend/storage/case 在独立进程运行，避免累计峰值内存互相污染；
保留同一确定性预生成 buffer 和 exact decode；
分别运行 stripped/tiled，不把合法 TIFF 文件 SHA 当等价条件；
报告失败次数、输出字节、p50/p95、working set 和 peak working set；
新增 run_03d_libtiff_writer_matrix.ps1 统一生成和校验矩阵报告；
明确 warm/cold 输出目录，冷暖数据不得混合计算。
```

若无法从 LibTIFF API 稳定拆出 open/tag/write/close 子阶段，报告必须把这些字段标记为
`not_available`，不得伪造拆分时间；`tiffWriteMs` 仍必须使用同一计时边界。

## 4. 固定矩阵

| Case | 尺寸/语义 | Storage | 预热/正式轮次 |
|---|---|---|---:|
| 小 fixture | 确定性 RGBWSV | stripped | 1 + 10 |
| Reality 量级 | 单模型层尺寸 | stripped | 1 + 5 |
| 多模型量级 | 联合 Raster 层尺寸 | stripped | 1 + 5 |
| 非整 tile | 边界需要 255 padding | tiled | 1 + 5 |

每项必须分别运行 handwritten/LibTIFF，使用 Release、同机器、同磁盘、同输入字节。

## 5. 判定门槛

```text
GO DEFAULT 候选：
  03D-05 全部兼容 Gate 保持 PASS；
  stripped 主生产矩阵 p50 改善 >= 15%，或真实包 TIFF 保存总耗时改善 >= 10%；
  LibTIFF 峰值工作集 <= handwritten 的 1.10 倍；
  无稳定性、部署或 Reader 回归。

GO OPTIONAL：
  兼容 PASS，但性能优势不足；保留显式 LibTIFF 后端。

NO-GO：
  像素/tag/RIP/原子发布任一回归，或内存/稳定性不可接受。
```

即使结果为 `GO DEFAULT` 候选，也必须等待 03D-07 的独立用户授权才能切换默认 Writer。

## 6. 验证入口

03D-06 完成后应提供并运行：

```powershell
.\scripts\run_03d_libtiff_writer_matrix.ps1 `
  -HandwrittenBuildDir build-slicesoft/main `
  -LibTiffBuildDir build-slicesoft/03d-libtiff `
  -Config Release

.\scripts\Run03DTiffCompatibilityGate.ps1 -Config Release -SkipBuild
git diff --check
```

矩阵输出写入忽略的 `output/benchmarks/03d_06`，阶段报告必须记录原始 JSON 路径、
机器/编译器/LibTIFF 版本、每项比率和最终判定。

## 7. 非目标

```text
压缩、BigTIFF、多 IFD、planar separate；
切片核心、preview、RIP 性能优化；
层级并行；
修改生产 Profile；
默认后端切换。
```
