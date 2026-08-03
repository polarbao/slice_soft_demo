# TASKS_03D LibTIFF 兼容迁移任务清单

> 文档状态：03D-01..06 COMPLETE / 03D-07 READY FOR OPTIONAL CLOSURE / PRIORITY P0
> 日期：2026-08-03
> 规则：每个原子任务完成后单独验证和提交；不得一次性切换默认后端

## 03D-01 当前合同与 Writer-only 基线

状态：COMPLETE / 2026-07-31

```text
增加 tag contract fixture；
增加 handwritten Writer-only benchmark；
冻结 stripped/tiled、像素、内存和当前错误行为；
不接入 LibTIFF。
```

验证：

```text
tiff writer contract unit tests；
Release benchmark 至少 5 次；
rip_reader_test；
git diff --check。
```

实际证据：

```text
tiff_writer_contract_unit_tests：Release PASS；
Writer-only benchmark：Release、5 次、stripped/tiled 均 decodedPixelsExact=true；
rip_reader_test：GoldenMaterialProcessTop2 PASS；
默认 Writer：仍为 handwritten；
LibTIFF/CMake link/Runtime DLL：未接入。
```

## 03D-02 vcpkg/CMake/Runtime 依赖接入

状态：COMPLETE / 2026-07-31

```text
锁定 builtin-baseline；
tiff default-features=false；
新增可选 LibTIFF 后端；
部署 DLL、许可证和版本元数据；
保持 handwritten 默认。
```

实际证据：

```text
builtin-baseline 已锁定；
tiff 4.7.1 default-features=false；
handwritten/LibTIFF Debug/Release 能力自检 PASS；
Runtime 已部署 DLL、许可证、版本和 SHA-256；
libtiffWriterImplemented=false；
默认 Writer 仍为 handwritten。
```

## 03D-03 Writer 接口与 LibTIFF stripped

状态：COMPLETE / 2026-07-31

```text
提取 Writer 接口；
手写 adapter；
实现 LibTIFF stripped；
逐 strip 写入，不复制整层 strip_data。
```

实际证据：

```text
handwritten adapter 与 LibTIFF stripped Writer 已实现；
handwritten Release、LibTIFF Debug/Release 定向 CTest PASS；
LibTIFF 20 层 GoldenMaterialProcessTop2 package 与 RIP strict PASS；
合法 SHORT/LONG 数值 tag 均由严格 Reader 按值校验；
Runtime 能力元数据为 stripped=true、tiled=false；
默认 Writer 仍为 handwritten。
```

## 03D-04 LibTIFF tiled 与错误模型

状态：COMPLETE / 2026-07-31

```text
单 tile scratch；
255 padding；
稳定错误码；
临时文件和失败清理。
```

实际证据：

```text
LibTIFF standard tiled Writer 使用单 tile scratch 并将边界 padding 初始化为 255；
LibTIFF 4.7.1 要求 tile 宽高为 16 的倍数，生产 256 x 256 走 LibTIFF；
历史 8 x 4 非标准 fixture 明确保留 handwritten compatibility route；
稳定 TiffWriterErrorCode、re-entrant per-handle handler、sibling temp 和原子替换已实现；
无效输入与发布失败不会覆盖已有输出，不残留 sibling temp；
handwritten Release、LibTIFF Debug/Release 定向 CTest PASS；
LibTIFF tiled package 与 RIP strict PASS；默认 Writer 仍为 handwritten。
```

## 03D-05 等价、坏包与共享 Package Gate

状态：COMPLETE / 2026-07-31

```text
decoded pixel exact；
required tags；
RIP strict/bad package；
Legacy/Global/scene package；
原子发布。
```

实际证据：

```text
stripped/tiled 六类 RGBWSV buffer 的 decoded pixels、checksum、stats 和 required tags 等价；
Reader 支持 TIFF 合法的双 SHORT 内联数组，不放宽数值与协议校验；
handwritten 与 LibTIFF 的 Writer/Legacy/Global/scene 共享 Package CTest 各 8/8 PASS；
两后端 Legacy stripped/tiled 实际 package 与 RIP strict PASS；
18/18 bad package 稳定错误码 PASS；
03D-04 原子发布和失败清理继续纳入 Gate；
默认 Writer 仍为 handwritten。
```

## 03D-06 Release 性能矩阵

状态：COMPLETE / GO_OPTIONAL（2026-08-03）

```text
同 buffer、同机器、同磁盘；
handwritten vs libtiff；
stripped/tiled；
p50/p95/peak memory；
形成 GO DEFAULT / GO OPTIONAL / NO-GO。
```

实现证据：`docs/slice/REPORT/REPORT_03D_06_LibTIFF性能矩阵与判定.md`。benchmark 已读取
真实 backend/LibTIFF 版本并支持单 storage；矩阵脚本以 16 个独立进程采集 warm/cold
output-directory 数据。兼容 Gate 和内存 Gate 通过，但主生产量级 warm stripped 的最低
p50 改善为 -58.937%，未达到 15%，因此只保留显式可选 LibTIFF 后端。

## 03D-07 默认切换与阶段收口

状态：READY FOR OPTIONAL CLOSURE / DEFAULT SWITCH BLOCKED

```text
03D-06=GO_OPTIONAL，不切换默认 Writer；
按可选后端路径更新 Runtime/用户说明、REPORT 和上下文；
保留 handwritten 回滚；
Full regression。
```

准备依据：`docs/slice/DOC/DOC_PREP_03D_07_LibTIFF阶段收口准备.md`。若未来重新申请默认
切换，必须先重跑固定参考机性能 Gate 并再次获得用户明确授权。

## 固定停止条件

```text
像素或 tag 不等价：停止；
RIP strict 失败：停止；
缺 DLL 或许可证：停止；
性能未达门槛：不得默认切换；
不得顺带实现压缩、BigTIFF、planar separate 或 12G-TCWS。
```
