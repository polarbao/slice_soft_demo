# 切片耗时与 Preview 保存耗时分析

> 生成日期：2026-07-03  
> 测试对象：`MF_nai_you` 一键切片配置  
> 源配置：`output/ui_sessions/MF_nai_you_20260703_170638/slice_config.generated.json`  
> 测试输出：`output/perf_analysis_20260703_182546/`  
> CLI：`build/Debug/slicer_cli.exe`

## 1. 结论

当前截图中约 20 秒的切片耗时，主要由三部分组成：

```text
切片计算/采样/支撑/合成/报告：约 14.4 秒
保存 498 张 RGBWSV TIFF 层：约 1.6 秒
保存 interval=10 的 preview PNG：约 4.7 到 5.1 秒
```

因此，当前默认 `interval=10` 情况下：

```text
切片本身是最大耗时来源；
preview PNG 保存是第二大耗时来源；
TIFF 层保存不是主要瓶颈。
```

但如果把 preview 改为每层输出，耗时会明显变长：

```text
interval=10：88 张 preview，26.28 MB，总耗时 20.705 秒
interval=1 ：868 张 preview，259.20 MB，总耗时 65.917 秒
```

这说明“叠加保存图片数据更多”确实会造成强烈的体感变慢，尤其是 preview 间隔变小、通道变多时。

## 2. 测试方法

使用同一个模型和同一套基础配置，只改变：

```text
是否写 TIFF layers；
是否写 preview；
preview.interval；
是否使用 CLI --preview-only。
```

说明：

```text
--preview-only 会跳过 TIFF layers 写入；
preview.enabled=false 会跳过 preview 图片写入；
两者同时使用时，可近似得到“切片计算 + 报告生成”的耗时。
```

注意：当前项目还没有细粒度内置 profiler，以下“切片计算 / TIFF 写入 / preview 写入”是通过差分实验估算，不是函数级精确计时。

## 3. 输出规模

本次测试包规模：

```text
grid = 229 x 455 x 498
layerCount = 498
modelPixels = 8,367,116
supportPixels = 25,724,342
reportCount = 15
```

单个完整生产包：

```text
TIFF layers = 498 张
TIFF total size = 297.06 MB
```

## 4. 详细耗时表

| 测试项 | TIFF | Preview | Preview 间隔 | 文件数量 | 文件体积 | 总耗时 |
|---|---:|---:|---:|---:|---:|---:|
| compute_only_no_tiff_no_preview | 0 | 0 | - | reports 15 | - | 14.403s |
| tiff_only_no_preview | 498 | 0 | - | TIFF 498 | 297.06MB | 16.005s |
| preview_only_interval10 | 0 | 88 | 10 | PNG 88 | 26.28MB | 19.453s |
| tiff_preview_interval10 | 498 | 88 | 10 | TIFF 498 + PNG 88 | 323.34MB | 20.705s |
| tiff_preview_interval1 | 498 | 868 | 1 | TIFF 498 + PNG 868 | 556.26MB | 65.917s |

## 5. 差分估算

### 5.1 切片计算时间

近似基线：

```text
compute_only_no_tiff_no_preview = 14.403s
```

该时间包含：

```text
模型加载；
autoOrient；
heightfield/层数据计算；
纹理采样；
支撑生成；
每层 RGBWSV buffer 合成；
通道统计；
报告 JSON 写入。
```

它不是纯数学切片内核时间，但最接近“无大图片落盘”的主流程耗时。

### 5.2 TIFF 保存时间

估算：

```text
tiff_only_no_preview - compute_only_no_tiff_no_preview
= 16.005s - 14.403s
= 1.602s
```

写入规模：

```text
498 张 TIFF
297.06 MB
```

结论：

```text
TIFF 保存约占默认完整切片的 7% 到 8%，不是当前主要瓶颈。
```

### 5.3 Preview 保存时间

interval=10 估算：

```text
tiff_preview_interval10 - tiff_only_no_preview
= 20.705s - 16.005s
= 4.700s
```

或：

```text
preview_only_interval10 - compute_only_no_tiff_no_preview
= 19.453s - 14.403s
= 5.050s
```

写入规模：

```text
88 张 PNG
26.28 MB
```

结论：

```text
默认 preview 保存约占完整切片的 23% 左右。
PNG 编码本身有 CPU 成本，不能只看文件体积。
```

### 5.4 Preview 每层输出的影响

估算：

```text
tiff_preview_interval1 - tiff_only_no_preview
= 65.917s - 16.005s
= 49.912s
```

写入规模：

```text
868 张 PNG
259.20 MB
```

结论：

```text
如果 preview.interval=1 或 preview.channels 增多，preview 保存会成为绝对主瓶颈。
```

## 6. UI 加载耗时

对生成后的包执行 UI smoke：

| 包 | UI 用例 | 耗时 | 结果 |
|---|---|---:|---|
| interval=10 | overlay-load-real | 0.305s | PASS |
| interval=1 | overlay-load-real | 0.565s | PASS |

结论：

```text
当前主要慢在 slicer_cli 生成阶段，不是 UI 加载阶段。
UI 自动加载 package 会增加少量体感等待，但不是 20 秒级耗时来源。
```

## 7. 为什么 preview PNG 很耗时

当前 preview 写入包含：

```text
1. 每层扫描 RGBWSV buffer；
2. 为每个 preview channel 生成 RGB 显示图；
3. 进行伪彩或 true-color 映射；
4. PNG 编码；
5. 文件写入；
6. preview_report.json 记录元数据。
```

即使 PNG 最终体积不算巨大，编码和逐像素转换仍会消耗 CPU 时间。

## 8. 当前性能判断

按默认 UI 一键配置：

```text
总耗时约 20.7s
其中约 14.4s 是切片主流程；
约 1.6s 是 TIFF 生产层写入；
约 4.7s 是 preview 保存；
UI 读取叠加预览约 0.3s。
```

因此：

```text
不是“保存 TIFF 图片导致 UI 显示慢”；
也不是“UI 本身加载非常慢”；
主要是切片主流程本身已有 14s 量级，preview PNG 生成再叠加约 5s。
```

## 9. 可优化方向

### 9.1 短期配置优化

建议 UI 一键切片默认：

```text
preview.interval = 20 或 25
preview.channels = ["texture_rgb", "support"]
preview.onlyNonEmptyLayers = true
```

用户需要细查时再手动切到：

```text
preview.interval = 1
```

### 9.2 UI 交互优化

建议拆成两个按钮或选项：

```text
快速切片：
  写 TIFF + 报告，少量 preview；

详细预览：
  写更多 preview PNG；
```

或者增加：

```text
生成 preview：开关
preview 密度：低 / 中 / 高
preview 通道：RGB / S / W / V 可选
```

### 9.3 工程优化

建议后续增加内置性能报告：

```text
reports/performance_report.json
```

字段：

```json
{
  "loadModelMs": 0,
  "autoOrientMs": 0,
  "textureSamplingMs": 0,
  "supportGenerationMs": 0,
  "layerComposeMs": 0,
  "tiffWriteMs": 0,
  "previewBuildMs": 0,
  "previewEncodeWriteMs": 0,
  "reportWriteMs": 0,
  "totalMs": 0
}
```

这样就不用依赖差分实验估算。

### 9.4 Preview 延迟生成

更理想的 UI 方案：

```text
切片时只写 TIFF + reports；
UI 打开某层时按需从 TIFF 生成 preview；
常用层可缓存到 output/cache；
后台线程异步生成全量 preview。
```

这样切片完成时间会更接近 16s，而不是 20s 或更久。

## 10. 建议结论

当前优先级：

```text
P0：在 UI 中增加 preview 密度设置，默认低密度；
P1：增加 performance_report.json，正式记录各阶段耗时；
P2：实现按需 preview/cache，减少切片阶段阻塞。
```

如果目标是让 UI 操作更快，优先减少 preview 生成数量；如果目标是让整体切片更快，需要继续优化切片主流程中的纹理采样、支撑生成和层合成。
