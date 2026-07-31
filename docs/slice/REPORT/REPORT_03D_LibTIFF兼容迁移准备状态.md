# REPORT_03D LibTIFF 兼容迁移当前状态

> 状态：03D-01/02/03 COMPLETE / 03D-04 READY
> 日期：2026-07-31
> 当前优先级：P0

## 1. 当前事实

```text
生产 Writer：统一 Writer 接口；默认 handwritten，显式 libtiff 轨道支持 stripped；
支持：stripped/tiled、contiguous、uint8、RGBWSV、无压缩；
Reader：仍由项目严格解析并服务 RIP/UI；
vcpkg.json：已锁定 baseline，tiff 4.7.1 且 default-features=false；
CMake：handwritten 默认轨道不查找 LibTIFF，libtiff 轨道链接 TIFF::TIFF；
Runtime：libtiff 轨道部署 DLL、许可证、版本和 SHA-256；
生产 Writer：默认仍使用手写实现；LibTIFF tiled 尚未实现并回退手写后端。
```

本机 `VCPKG_ROOT` 指向 `D:\Program Files Tools\vcpkg`。2026-07-31 只读检查显示本机
port metadata 为 `tiff 4.7.1`。03D-02 已在独立 manifest 构建目录完成安装和链接，
关闭 JPEG/LZMA/ZIP 默认 feature，没有修改共享 vcpkg 根的 classic 安装状态。

## 2. 准备产物

```text
DOC_DECISION；
PRD；
DEV；
DEMO；
ROADMAP；
TASKS；
CODEX_PROMPT；
本准备状态报告。
```

已冻结：

```text
双后端迁移；
像素/tag 等价而非文件 SHA 等价；
现有严格 Reader 作为独立验证方；
Writer-only Release benchmark；
R5 前不切换默认；
失败可回滚；
LibTIFF 为选定目标，OpenImageIO 不采用。
```

## 3. 03D-01 实现与性能证据

03D-01 已新增：

```text
tiff_writer_contract_unit_tests：冻结 required tags、stripped/tiled 布局、边界 tile 255 padding、
decoded RGBWSV 像素和当前错误文本；
tiff_writer_benchmark：只计 write_rgbwsv_tiff 调用，不包含像素生成、Reader 校验和 package/preview；
Run03DTiffWriterBaseline.ps1：Release >=5 次、合同测试、benchmark JSON 和 RIP strict Gate；
CTest/CMake target：均已接入。
```

2026-07-31 本机 Release 5 次基线：

| 后端 | 存储 | 输入 | p50 | p95 | 文件字节 | 写后工作集 | Writer staging 估算 |
|---|---|---:|---:|---:|---:|---:|---:|
| handwritten | stripped | 1024 x 2048 x 6 | 9.574 ms | 11.217 ms | 12,583,406 | 16,982,016 B | 12,582,912 B |
| handwritten | tiled | 1024 x 2048 x 6 | 24.057 ms | 43.504 ms | 12,583,418 | 17,043,456 B | 12,582,912 B |

原始 JSON 写入被忽略的运行输出：

```text
output/benchmarks/03d_01/tiff_writer_handwritten_baseline.json
schema=slicesoft.tiff_writer_benchmark.03d.1
scope=writer_only
```

工作集采样发生在测量写入完成、Reader 校验开始之前；JSON 中的
`peakWorkingSetBytes` 是整个 benchmark 进程累计峰值，只作诊断，不作为每个 case 的独立峰值。
03D-06 若比较后端峰值，必须让各后端/存储 case 运行在独立进程中。

当前报告中的 TIFF 保存不是主要切片计算，但在真实 package 中不可忽略：

```text
13C-04 小 fixture Debug：约 78-81 ms；
13F Reality Release：约 820-1310 ms；
```

这些 package 数字与 03D-01 的 Writer-only 数字口径不同，不能直接互相替代。后续 03D-06
必须在同 buffer、同机器、同磁盘上重新比较 handwritten 与 LibTIFF。

## 4. 03D-02 依赖与 Runtime 证据

```text
SLICESOFT_TIFF_BACKEND=handwritten|libtiff；
默认 handwritten；
LibTIFF Debug/Release build-info CTest PASS；
handwritten/LibTIFF Runtime 部署 PASS；
LibTIFF version=4.7.1；
tiff.dll 与 licenses/libtiff.txt 已进入隔离 Runtime；
runtime_manifest.json 已记录后端、实现状态、版本、SHA-256 和许可证。
```

03D-02 没有实现或路由 LibTIFF Writer；能力自检明确输出
`libtiffWriterImplemented=false`。

## 5. 03D-03 Writer 与 stripped 证据

```text
ITiffWriter、Factory 和 handwritten adapter 已实现；
LibTIFF stripped 逐 strip 直接写入，不构造整层 strip_data 副本；
handwritten Release、LibTIFF Debug/Release 定向 CTest PASS；
LibTIFF GoldenMaterialProcessTop2 package 与 RIP strict PASS；
Runtime manifest：writer=true、stripped=true、tiled=false；
默认 handwritten 未改变。
```

LibTIFF 对部分小整数 tag 选择合法 `SHORT`，手写后端固定为 `LONG`。当前严格 Reader
按 TIFF 合同接受 `SHORT/LONG` 并继续校验相同数值、必需 tag 和逐字节像素，不改变
RGBWSV、uint8、`black_is_print` 或无压缩协议。

## 6. 准备度

| 项目 | 状态 |
|---|---|
| 需求边界 | READY |
| 固定协议 | READY |
| 候选比较 | READY |
| CMake/vcpkg 方案 | READY |
| Runtime/许可证方案 | READY |
| 功能/负向矩阵 | READY |
| 性能 Gate | READY |
| 依赖修改授权 | GRANTED / COMPLETE |
| 03D-01 合同 fixture | COMPLETE |
| 03D-01 Writer-only Release 基线 | COMPLETE |
| 03D-02 vcpkg/CMake/Runtime | COMPLETE |
| 03D-03 Writer 接口与 stripped | COMPLETE |
| LibTIFF stripped 代码实现 | COMPLETE |
| 03D-04 tiled 与错误模型准备 | READY |

## 7. 下一步

下一原子任务为 `03D-04 LibTIFF tiled 与错误模型`。保持 handwritten 默认，实现单 tile
scratch、边界 255 padding、稳定错误码、临时文件和失败清理；不得提前执行完整性能 Gate、
压缩或默认切换。
