# DOC_DECISION_16C-06-MEMFLOW 有界逐层流式内存根治

> 文档状态：**ACTIVE / DECIDED**
> 版本：v1.0 ｜ 日期：2026-08-18
> 定位：Stage 16C-06 的内存根治决策；冻结实施顺序、生产边界与回退条件
> 上游：`PRD_16`、`DEV_16`、`16C-05`、`12F-06`、`13B-05`
> 证据等级：A=当前代码/实测事实，B=目标设计，C=历史建议，P=工程判断

---

## 1. 决策摘要

当前大幅面稀疏模型的主要问题不是三角形数量，而是把整个 `width * height * layers`
空间按 Dense Mask、RGBWSV 和 Scene Ownership 多次常驻。16C-05 已减少可选 Mask 和单实例
复制，但没有改变整栈常驻的复杂度。

本专项采用以下顺序：

```text
M0  先冻结内存预算、语义基线和路由合同
M1  Dense 有界逐层/小窗口流式，不改变像素算法
M2  多实例同层屏障，保持 Model > Support > Empty 和确定顺序
M3  稀疏 Tile/Span 只作为独立候选，逐层零漂移后才允许生产接入
M4  用预算自适应路由；小作业继续走 Retained Dense
```

不直接把 Sparse 作为第一步，也不通过无限并行换取速度。

## 2. 事实基线

### 2.1 当前代码事实（A）

```text
LayerOccupancyProvider::BuildLayerOccupancy
  -> 返回 vector<vector<uint8_t>>，仍物化完整三维 model mask

run_slicer
  -> 先构造完整 model/support/type mask
  -> 再逐层生成 RGBWSV，通过 const callback 交给 Adapter

LegacySceneLayerAdapter
  -> 收集全部 SceneInstanceRasterLayer

SceneLayerComposer
  -> 收集全部 Scene 输出层

RgbwsvPackageWriter
  -> 接收完整 vector<RgbwsvProductionLayer> 后逐层写 TIFF
```

已有逐层 callback 和逐层 TIFF Writer，可作为 `wrap first` 的接入点；但当前生命周期仍由完整
层列表主导，不能据此宣称已经流式化。

### 2.2 `123.stl` 工作负载快照

| 项目 | 当前证据 |
|---|---:|
| 输入三角形 | 41,270 |
| 连通分量 | 17（含微小组件） |
| 模型包围盒 | 约 220 x 60 x 23.88 mm |
| 观测 Grid | 5197 x 1418 x 141；历史同类包曾为 5500 x 1418 x 141 |
| Pixel-layer 样本 | 约 10.39～11.00 亿 |
| 六通道 Dense RGBWSV | 约 5.8～6.1 GiB |
| 历史包非空占比 | 约 3.06% |

这些数字是专项容量规划输入，不冒充同请求优化后 A/B；原 Worker 请求已按清理合同删除，完整
Profile/变换/压缩参数需在下一次同请求运行时重新固化。

## 3. 目标和非目标

### 3.1 目标

```text
峰值内存从 O(width * height * layers) 降到 O(width * height * window)
大幅面稀疏模型不因分页和整栈复制出现分钟级额外等待
逐层 TIFF、manifest、统计、RIP strict 与未优化基线一致
取消、失败和进程退出仍只留下可恢复 staging，不发布半包
多实例按同一 global layer 确定性合成，不依赖完成先后顺序
```

### 3.2 非目标

```text
不改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print
不改 Legacy/S0/P0 默认语义，不解锁 16B-04 或 16D-05
不改 SPI v1、11 个导出、15 项能力和 Worker 文件合同
不默认启用 OpenVDB，不引入新第三方依赖
不在首阶段实现并行切片、缓存或 Preview 异步
不把减少内存等同于已经缩短 CPU 扫描时间
```

## 4. 方案比较与裁决

| 方案 | 内存收益 | CPU 收益 | 语义风险 | 裁决 |
|---|---:|---:|---:|---|
| A. 继续整栈 Dense，仅减少复制 | 中 | 低 | 低 | 16C-05 已做，不能根治 |
| B. Dense 有界逐层流式 | 高 | 中；主要减少复制/分页 | 低到中 | **先实施** |
| C. 多实例同层屏障 | 高 | 中 | 中 | B 后实施 |
| D. Sparse Tile/Span | 很高 | 高；空白不再全扫 | 高 | 独立候选，最后实施 |
| E. 全局并行 | 不确定 | 不确定 | 高 | 16C-09 前禁止 |

### 4.1 为什么 Dense Streaming 先行

它只改变数据生命周期，不改变每个像素的计算函数。以 5197 x 1418 为例，一层 RGBWSV 约
42.2 MiB；三层窗口加若干 Mask 仍是百 MiB 级，而不是 5.8 GiB 整栈。该阶段最适合用逐层
SHA-256 证明零语义漂移。

### 4.2 为什么 Sparse 后置

`123.stl` 含微小连通分量。稀疏包围盒、Tile 或 Span 若裁剪错误，可能丢失小组件、边界一像素、
支撑连通或光油扩张区。Sparse 必须基于 Dense Streaming 的稳定基线单独对照，不能与流式生命周期
改造一次完成。

## 5. 冻结的内存与路由合同

### 5.1 估算口径

首版预算器只报告可证明的栅格下界：

```text
pixelsPerLayer          = width * height
retainedRgbwsvBytes     = pixelsPerLayer * layers * 6
retainedMaskBytes       = pixelsPerLayer * layers * maskBytesPerPixel * instances
boundedWindowRgbwsv     = pixelsPerLayer * windowLayers * 6
boundedWindowMask       = pixelsPerLayer * windowLayers
                          * maskBytesPerPixel * inFlightInstances
boundedWindowBytes      = boundedWindowRgbwsv + boundedWindowMask
```

RGBWSV 是场景最终层，只按场景计一次；ownership/instance Mask 才按可见或在途实例数计数。

不把 STL/OBJ、纹理、容器开销和 Writer 内部分配伪装成精确峰值；真实 Gate 仍使用独立进程 Peak
Working Set。

### 5.2 路由顺序

```text
预算未提供或 Retained Dense 在预算内 -> Retained Dense
预算超限且 Dense Streaming 已实现并在预算内 -> Bounded Dense Streaming
预算超限但已实现路径均不能满足 -> fail closed，不赌系统分页
Sparse 只有在独立 Gate 通过后才能成为可选路由
```

任何自动路由都必须写入 diagnostic telemetry；路由不得改变 Profile hash 或材料语义。

## 6. 跨层与多实例约束

### 6.1 支撑和材料

不能简单生成一层就永久丢弃全部上游数据：

```text
底部/上部投影、铺底和连通性需要列范围或跨层信息；
材料闭合 exact/repair 需要最终同层语义；
光油膨胀和支撑优先级需要固定 halo/window。
```

因此先把完整 Mask 替换为列范围/Provider，再按需要物化当前层与固定 halo。无法证明有界窗口的
步骤必须保留两阶段扫描，不允许悄悄近似。

### 6.2 Scene Composer

多实例使用 `globalLayerIndex` 屏障：只有该层所有可见实例均已提供或显式声明为空后才合成；实例
遍历顺序使用冻结的 scene 顺序，不能按线程完成顺序。重叠、材料冲突和 stale revision 继续
fail closed。

## 7. 原子发布和数据同步

流式 Writer 必须继续使用现有同父目录 staging、lease、严格 Reader 和原子 rename。允许 TIFF
逐层写入 staging，但在最后一层、报告、严格校验全部成功前不得发布 package。

同步数据分三类：

```text
控制面：scene/profile/grid/protocol/revision/cancel/budget
数据面：layerIndex/zMm/RGBWSV/ownership/halo
证据面：逐层统计/hash、内存路由、窗口水位、Writer/RIP 结果
```

控制面在作业开始后冻结；数据面按层单向推进；证据面由同一层事实增量累计，最终仍由严格 Reader
独立复核持久化 TIFF。

## 8. Gate 与停止条件

```text
G-M1  预算器溢出安全、无预算默认不变、123 大栅格可正确选择候选路由
G-M2  Dense Streaming 对 fixture/真实模型逐层 RGBWSV+ownership hash 全等
G-M3  支撑/铺底/材料闭合/光油矩阵全等
G-M4  单实例 Package/manifest/report/RIP strict 全等
G-M5  1/11/12/22 多实例确定性、取消、stale、冲突和恢复 PASS
G-M6  Sparse 候选不丢 123.stl 的 17 个连通分量及边界/支撑像素
G-M7  同请求 Release 峰值内存下降；CPU/总耗时如实报告
G-M8  设备 SLA/内存上限未给出时 production Gate 保持 INPUT_OPEN
```

出现逐层 hash、层数、统计、支撑类型、预览来源或严格 Reader 任一漂移，立即保留旧路径并停止
扩大接入。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-18 | v1.0 | 用户授权开启根治专项；裁定 Dense 有界流式优先、多实例同层屏障其次、Sparse 最后；冻结预算、发布、数据同步和零漂移 Gate。 |
