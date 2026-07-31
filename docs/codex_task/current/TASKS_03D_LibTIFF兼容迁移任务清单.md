# TASKS_03D LibTIFF 兼容迁移任务清单

> 文档状态：03D-01/02 COMPLETE / 03D-03 READY / PRIORITY P0
> 日期：2026-07-31
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

状态：READY / 03D-02 GATE PASSED

```text
提取 Writer 接口；
手写 adapter；
实现 LibTIFF stripped；
逐 strip 写入，不复制整层 strip_data。
```

## 03D-04 LibTIFF tiled 与错误模型

状态：PREPARED / WAIT 03D-03

```text
单 tile scratch；
255 padding；
稳定错误码；
临时文件和失败清理。
```

## 03D-05 等价、坏包与共享 Package Gate

状态：PREPARED / WAIT 03D-04

```text
decoded pixel exact；
required tags；
RIP strict/bad package；
Legacy/Global/scene package；
原子发布。
```

## 03D-06 Release 性能矩阵

状态：PREPARED / WAIT 03D-05

```text
同 buffer、同机器、同磁盘；
handwritten vs libtiff；
stripped/tiled；
p50/p95/peak memory；
形成 GO DEFAULT / GO OPTIONAL / NO-GO。
```

## 03D-07 默认切换与阶段收口

状态：WAIT 03D-06 AND USER AUTHORIZATION

```text
只有 GO DEFAULT 且用户授权后切换；
保留 handwritten 回滚；
更新 Runtime、用户说明、REPORT 和上下文；
Full regression。
```

## 固定停止条件

```text
像素或 tag 不等价：停止；
RIP strict 失败：停止；
缺 DLL 或许可证：停止；
性能未达门槛：不得默认切换；
不得顺带实现压缩、BigTIFF、planar separate 或 12G-TCWS。
```
