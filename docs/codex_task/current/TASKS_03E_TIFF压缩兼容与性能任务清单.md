# TASKS_03E TIFF 压缩兼容与性能任务清单

> 状态：03E-01 COMPLETE / 03E-02 PENDING  
> 日期：2026-08-03

## 03E-01 PackBits 原型与读写性能矩阵

状态：COMPLETE

```text
[x] 审计现有 Compression tag 和 Reader 能力
[x] 比较 PackBits / Deflate 的依赖、许可证和维护风险
[x] handwritten stripped/tiled PackBits Writer
[x] LibTIFF stripped/tiled PackBits Writer
[x] 项目严格 Reader PackBits 解码与错误检查
[x] 双 Writer exact decode / tag / 255 tile padding 等价测试
[x] Release none/PackBits 写入、读取、体积矩阵
[x] 保持默认 TiffImageSpec.compression_mode=None
```

验证入口：

```powershell
.\scripts\Run03ETiffCompressionMatrix.ps1
```

## 03E-02 生产压缩协议与真实 RIP Gate

状态：PENDING / NOT AUTHORIZED

```text
[ ] config schema 增加 output.tiffCompression
[ ] manifest 显式声明 compression
[ ] Package / RIP strict / bad package 压缩矩阵
[ ] 真实模型 package 与目标 RIP 互操作验证
[ ] Qt UI 高级输出设置
[ ] 根据完整切片总耗时决定是否允许默认启用
```

进入条件：03E-01 报告通过，且用户明确授权生产协议与配置变更。

## 固定边界

- 不修改 RGBWSV 六通道顺序、uint8、`black_is_print`；
- 不将 PackBits 自动设为生产默认值；
- 不在 03E-01 引入 zlib/Deflate；
- 不把统一严格 Reader 的读取结果描述为 LibTIFF Reader 性能。

