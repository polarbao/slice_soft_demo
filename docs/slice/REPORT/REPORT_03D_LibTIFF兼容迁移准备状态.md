# REPORT_03D LibTIFF 兼容迁移当前状态

> 状态：03D-01 COMPLETE / 03D-02 READY
> 日期：2026-07-31
> 当前优先级：P0

## 1. 当前事实

```text
生产 Writer：src/slicer_core/tiff_io.cpp 手写实现；
支持：stripped/tiled、contiguous、uint8、RGBWSV、无压缩；
Reader：仍由项目严格解析并服务 RIP/UI；
vcpkg.json：已声明 tiff；
CMake：没有 find_package(TIFF)，没有链接 TIFF::TIFF；
默认 Runtime：没有部署 LibTIFF Writer DLL；
因此当前没有使用 LibTIFF 生成生产 TIFF。
```

本机 `VCPKG_ROOT` 指向 `D:\Program Files Tools\vcpkg`。2026-07-31 只读检查显示本机
port metadata 为 `tiff 4.7.1`，默认 feature 包含 JPEG/LZMA/ZIP；专项设计要求关闭这些
当前不需要的 feature。该检查不等于项目已安装或链接依赖。

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

## 4. 准备度

| 项目 | 状态 |
|---|---|
| 需求边界 | READY |
| 固定协议 | READY |
| 候选比较 | READY |
| CMake/vcpkg 方案 | READY |
| Runtime/许可证方案 | READY |
| 功能/负向矩阵 | READY |
| 性能 Gate | READY |
| 依赖修改授权 | NOT GRANTED |
| 03D-01 合同 fixture | COMPLETE |
| 03D-01 Writer-only Release 基线 | COMPLETE |
| 03D-02 依赖修改授权 | NOT GRANTED |
| LibTIFF 代码实现 | NOT STARTED |

## 5. 下一步

`03D-01 当前合同与 Writer-only 基线` 已完成。下一候选任务为 `03D-02
vcpkg/CMake/Runtime 依赖接入`，必须等待用户明确授权；在此之前不得安装、链接或部署
LibTIFF，也不得切换默认 Writer。
