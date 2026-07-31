# DEMO_03D LibTIFF 兼容与性能验证方案

> 文档状态：VALIDATION PLAN READY
> 日期：2026-07-31

## 1. 验证目标

证明 LibTIFF 后端：

```text
生产像素和 TIFF 合同等价；
stripped/tiled 均可被现有 Reader 严格读取；
异常不会发布残缺 package；
Runtime 依赖完整；
性能结论由 Writer-only Release 数据支持。
```

## 2. 功能矩阵

| Case | Storage | 内容 | 验收 |
|---|---|---|---|
| D03-01 | stripped | 全空 RGBWSV | 像素、tag、统计等价 |
| D03-02 | stripped | RGB/W/S/V + partial | 六通道逐字节等价 |
| D03-03 | stripped | 最后 strip 非满 | byte count/Reader PASS |
| D03-04 | tiled | 非整 tile 尺寸 | padding=255、Reader PASS |
| D03-05 | tiled | 六通道混合 | 像素、tag、统计等价 |
| D03-06 | both | 单模型真实 package | RIP strict PASS |
| D03-07 | both | 多模型 scene package | layer list/RIP PASS |
| D03-08 | both | Legacy/Global 已准入 fixture | 共享 Writer 合同一致 |

## 3. 负向矩阵

```text
零尺寸；
buffer size 不匹配；
非法 storage 参数；
不可写目录；
模拟 strip/tile 写入失败；
缺失 Runtime DLL；
LibTIFF 输出被截断；
manifest storage 与 TIFF 不一致；
失败时已有正式 package 不被替换。
```

## 4. 性能矩阵

每项使用同一预生成内存层：

| Case | Backend | Storage | 轮次 |
|---|---|---|---:|
| 小 fixture | handwritten/libtiff | stripped | 1 warm-up + 10 |
| Reality 量级 | handwritten/libtiff | stripped | 1 warm-up + 5 |
| 多模型量级 | handwritten/libtiff | stripped | 1 warm-up + 5 |
| 非整 tile | handwritten/libtiff | tiled | 1 warm-up + 5 |

记录：

```text
p50/p95；
open/tag/write/close/total；
bytesWritten；
peakWorkingSetBytes；
CPU time；
失败次数；
机器、编译器、构建类型、磁盘路径、依赖版本。
```

## 5. 建议命令

以下命令是实现后的目标入口，当前准备阶段不得宣称已存在或已运行：

```powershell
cmake --build build-slicesoft/main --config Release --target `
  tiff_writer_equivalence_tests tiff_writer_benchmark rip_reader_test

ctest --test-dir build-slicesoft/main -C Release `
  -R "tiff_writer_(equivalence|negative)" --output-on-failure

.\scripts\run_03d_libtiff_writer_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release
```

随后执行项目回归：

```powershell
.\scripts\run_ci_quick.ps1
.\scripts\run_regression.ps1 -Mode full
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Debug
.\scripts\PrepareSliceSoftRuntime.ps1 -Config Release
```

## 6. GO/NO-GO

```text
GO DEFAULT：
  所有兼容/负向/Runtime Gate PASS；
  达到性能或维护切换门槛；
  用户授权默认切换。

GO OPTIONAL：
  兼容 PASS，但性能优势不足；
  LibTIFF 仅保留为可选后端。

NO-GO：
  像素/tag/RIP/原子发布任一失败；
  Runtime 部署不稳定；
  峰值内存明显回归且无可接受解释。
```
