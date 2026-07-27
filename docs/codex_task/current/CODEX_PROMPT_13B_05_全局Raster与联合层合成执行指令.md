# CODEX_PROMPT 13B-05 全局 Raster 与联合层合成执行指令

> 状态：READY FOR FIXTURE DEVELOPMENT
> 日期：2026-07-27
> 前置：13B-04 FUNCTIONAL FIXTURE COMPLETE

## 1. 必读

```text
AGENTS.md；
docs/slice/REPORT/REPORT_13B_04_幅面碰撞与逐实例准入当前状态.md；
docs/slice/PRD/PRD_13B_多模型规则排版与联合切片.md；
docs/slice/DEV/DEV_13B_MultiModelScene规则排版与联合切片设计.md；
docs/slice/DEMO/DEMO_13B_多模型排版联合切片验证方案.md；
docs/slice/DOC/DOC_PREP_13B_05_全局Raster与联合层合成准备.md。
```

## 2. 执行顺序

```text
13B-05A：先写 SceneRaster 公共合同和失败测试；
13B-05B：提取 Legacy 内存 layer adapter，并包装现有 Global adapter；
13B-05C：实现共享 grid、整数 offset、逐层 SceneLayerComposer 和 closure；
运行定向回归和 Quick CI；
生成 REPORT_13B_05_全局Raster与联合层合成当前状态.md。
```

## 3. 固定边界

```text
只消费 13B-04 已准入可见实例；
场景内只允许一种 effective pipeline mode；
不复制 Legacy 几何/切片数学；
不从 TIFF 反读合成；
不写 TIFF/manifest/report/package；
不做跨实例联合支撑；
不允许重叠按实例顺序静默覆盖；
保持 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
保持 Legacy 默认和 OpenVDB optional/OFF。
```

## 4. 验收

```text
单实例字节等价；
分离实例正确映射；
共享 XY/Z grid；
净距全 255；
RGB/W/S/V 不串写；
模型重叠、材料冲突、协议/分辨率/层序/offset/stale 均 fail-closed；
失败无部分成功层；
相同输入确定；
单模型和 Quick CI 回归通过。
```

## 5. 停止条件

发生以下任一情况立即停止并记录：

```text
需要虚构设备 buildVolume；
Legacy 无法在不复制算法的前提下提取内存层；
需要改变 RGBWSV/TIFF 协议；
需要提前实现 13B-06 package；
13B-04 或单模型回归失败。
```
