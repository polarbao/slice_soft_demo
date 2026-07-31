# REPORT_03D LibTIFF 兼容迁移准备状态

> 状态：PREPARATION COMPLETE / IMPLEMENTATION NOT STARTED
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

## 3. 现有性能证据

当前报告中的 TIFF 保存不是主要切片计算，但在真实 package 中不可忽略：

```text
13C-04 小 fixture Debug：约 78-81 ms；
13F Reality Release：约 820-1310 ms；
```

这些数字来自不同模型、构建和矩阵，不能直接用来承诺 LibTIFF 改善比例。03D-01 必须重新
建立同 buffer、同机器、同磁盘的 Writer-only 基线。

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
| 代码实现 | NOT STARTED |

## 5. 下一步

唯一下一任务为 `03D-01 当前合同与 Writer-only 基线`。它不改依赖、不改默认 Writer，
完成后再由用户决定是否进入 03D-02。
