# PRD_13C RGBWSV TIFF 原生统一预览

> 文档版本：v0.1
> 文档状态：Formal PRD / PREPARED
> 生成日期：2026-07-24

## 1. 背景

当前工作台已经把“层预览、材料叠加、原始调试预览”放进统一容器并共享真实 layerIndex，但三个模式
仍使用不同数据源：

```text
生产 RGB 和六通道像素探针可读取 TIFF；
W/S/V 单通道和材料叠加主要读取 preview PNG；
原始调试预览读取算法生成的 preview 文件。
```

这造成重复 IO、同层数据源不一致和用户难以判断“看到的是生产 TIFF 还是调试图”。

## 2. 产品目标

生产层检查和材料叠加统一直接读取 package 中的 RGBWSV TIFF，并在内存中生成显示图，不再要求切片
流程为每种材料额外写一张 PNG。

## 3. 统一预览模式

主预览只保留两个一级入口：

```text
生产预览：权威数据源为 RGBWSV TIFF；
诊断预览：可选 report/semantic mask/raw preview。
```

生产预览提供：

```text
RGB；
R；
G；
B；
W 白墨；
S 支撑；
V 光油；
RGB + W；
RGB + S；
RGB + V；
RGB + S + W + V；
占用；
真实空白。
```

诊断预览提供：

```text
Texture Surface / Model Fill / Partition；
closure gap；
fallback；
拓扑和准入；
支撑类型；
算法原始 mask。
```

诊断数据缺失时显示“未提供”，不得从 TIFF 猜测不存在的语义。

## 4. 图层和坐标

```text
所有视图使用 manifest 中真实 layerIndex；
显示 zMm；
层序固定为低 Z -> 高 Z；
不得按文件序号映射层；
不得跨层寻找 RGB 或材料图兜底；
显示物理比例使用 dpiX/dpiY；
UI 屏幕 Y 翻转只属于显示坐标，不改变 TIFF 像素和 layerIndex。
```

## 5. 伪彩

默认显示颜色：

```text
Empty：(255,255,255)；
S：沿用可配置支撑绿色；
W：沿用可配置白墨青色；
V：(127,127,127)；
R/G/B：对应通道色；
RGB：使用生产 RGB 解释或纹理真彩模式；
```

W/S/V 伪彩必须以通道值 `<255` 判断打印。伪彩颜色和透明度可在 UI 设置，但只影响显示。

## 6. 全材料叠加

新增明确选项：

```text
RGB + S + W + V
```

该模式必须：

```text
在同一 layerIndex 读取一次 TIFF；
以 RGB 为底图；
按可配置颜色/透明度叠加 W/S/V；
像素探针列出全部实际打印通道；
图例同时显示 RGB、W、S、V 和 Empty；
不因显示优先级改写生产通道。
```

当多个通道在同一像素打印时，像素探针是权威解释；UI 不得只显示最上层颜色后声称其他材料不存在。

## 7. IO 策略

常规生产切片默认：

```text
写 RGBWSV TIFF；
写 manifest/report；
不为 RGB/W/S/V/overlay 重复写逐通道 PNG；
UI 按层按需读取 TIFF；
用户显式导出截图时才写显示图片。
```

算法诊断 preview 保留独立开关，默认不影响生产 package 成功。

## 8. 非目标

```text
不修改 TIFF 生产值；
不把伪彩写回 TIFF；
不从 TIFF 反推 Texture Surface 与 Model Fill 语义分区；
不删除调试诊断能力；
不实现 RIP 半色调预览；
不一次性把所有层 TIFF 全部载入内存；
不通过跨层兜底掩盖缺失文件。
```

## 9. 验收标准

```text
只保留 TIFF 和 report 时，生产预览仍可浏览所有层；
R/G/B/W/S/V 单通道均可显示；
RGB+S+W+V 可显示；
首层、中间层、末层 layerIndex/zMm 正确；
像素探针值与 read_rgbwsv_tiff 一致；
stripped/tiled TIFF 均支持；
600/600 与 635/600 物理比例正确；
关闭 preview PNG 后切片写盘时间下降或不回归；
诊断数据缺失时显示未提供；
三窗口尺寸和最长中文 smoke 通过。
```

## 10. 成功指标

```text
生产预览不依赖 preview 目录；
同层显示只有一个权威生产像素源；
重复 preview 文件数量显著减少；
材料叠加和像素探针能解释 RGB/W/S/V；
不改变 p0.rgbwsv.2 和 RIP strict。
```
