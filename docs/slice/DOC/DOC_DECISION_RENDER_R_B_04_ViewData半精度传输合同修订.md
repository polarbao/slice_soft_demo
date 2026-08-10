# DOC DECISION R-B-04 ViewData 半精度传输合同修订

> 状态：**ACCEPTED / IMPLEMENTED**
> 日期：2026-08-10
> 关联任务：`R-B-04`
> 合同版本：`slicer_capability_dtos` v1.10

## 1. 决策背景

R-B-02 已使用 meshoptimizer 替换破坏拓扑的跳采样，R-B-03 已区分安全简化与历史抽稀。
剩余问题是 ViewData 在 DLL 边界仍把 position、normal、texcoord0 全部按 float32 传输，22 实例
场景会过早进入几何简化。

原任务卡只写“position/normal 降至 16-bit”，但该方案的理论缩减上限不足 40%：每顶点属性由
32 B 降为 20 B，索引不变。为满足既定体积与 25k 三角阈值，必须把 texcoord0 同步量化。

## 2. 决策

采用向后兼容的显式请求：

```text
meshAttributeFormat 缺省或 float32
  position=float32x3, normal=float32x3, texcoord0=float32x2

meshAttributeFormat=float16
  position=float16x3, normal=float16x3, texcoord0=float16x2
```

float16 使用 IEEE-754 binary16、小端序，由 `meshopt_quantizeHalf` 生成。index 继续使用 uint32。
模块内部几何和算法保持 float32；只在 ViewData wire adapter 量化。参考宿主同时解码 float32 与
float16，并在本地恢复为 float32 后交给渲染后端。

## 3. 兼容与缓存

- 旧宿主不传新字段，继续得到 float32；
- 新宿主显式请求 float16；
- `maxBytes` 按实际 wire format 估算；
- `meshIdentity` 包含 attribute format，禁止两种编码命中同一宿主缓存；
- SPI v1、11 个导出、15 项能力、RGBWSV/TIFF 协议均不变。

## 4. 质量与停止条件

- position/normal/UV 三类属性必须一起闭合，不能混合声明和实际字节；
- 缺格式、长度不符、非有限值由宿主 fail-closed；
- float16 真实纹理 fixture 的 mesh blob 至少缩减 40%；
- 25k 个无共享三角的 wire 估算必须进入 `32 MiB / 22` 单实例预算；
- three_d 真实纹理渲染、相机零 DLL 调用和 float32 兼容路径必须继续通过。

## 5. 未包含范围

不压缩索引、不引入 meshopt vertex/index codec、不改变 LOD 屏幕空间判据、不升级 Qt、不修改
生产 TIFF、切片算法或 OpenVDB。
