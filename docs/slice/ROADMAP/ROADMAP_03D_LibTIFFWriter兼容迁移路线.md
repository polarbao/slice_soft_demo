# ROADMAP_03D LibTIFF Writer 兼容迁移路线

> 文档状态：EXECUTION ROADMAP READY
> 日期：2026-07-31
> 当前优先级：P0

## R0 合同与基线

```text
冻结当前 tag、像素、storage 和错误合同；
新增 Writer-only benchmark schema/tool；
产出手写 Writer Release 基线。
```

完成 Gate：基线可复现，尚不改变生产后端。

## R1 依赖与构建

```text
锁定 vcpkg baseline；
tiff 使用 default-features=false；
接入 TIFF::TIFF；
Runtime 部署和许可证清单；
LibTIFF 后端仍 opt-in。
```

完成 Gate：Debug/Release 构建、自检、部署 PASS。

## R2 双后端实现

```text
Writer 接口；
handwritten adapter；
LibTIFF stripped/tiled；
稳定错误码和临时文件。
```

完成 Gate：功能 fixture 等价。

## R3 协议与负向收口

```text
tag/pixel equivalence；
RIP strict；
坏包；
原子 package；
Legacy/Global/scene 回归。
```

完成 Gate：协议零回归。

## R4 性能判定

```text
Writer-only Release p50/p95；
真实 package TIFF 保存；
峰值内存；
冷/暖磁盘。
```

完成 Gate：形成默认切换建议，不预设结论。

## R5 默认切换与观察期

```text
用户授权后切换默认；
手写后端保留回滚；
Debug/Release Runtime 和 Full regression；
发布报告、许可证和依赖版本。
```

完成 Gate：`REPORT_03D` 为 GO DEFAULT 或 GO OPTIONAL，不能只写“已接入”。
