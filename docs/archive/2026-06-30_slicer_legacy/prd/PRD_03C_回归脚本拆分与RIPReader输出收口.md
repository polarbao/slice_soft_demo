# PRD_03C_回归脚本拆分与RIPReader输出收口

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_03B 之后 / 03C  
> 所属模块：Regression / RIP Reader / Test Tooling  
> 建议提交目录：`docs/slicer/`

## 1. 产品目标

03C 目标是提升当前测试与验证链路的可维护性：

```text
缩短默认主回归时间
将 heavy relief / heavy texture 从快速回归中拆分
为 rip_reader_test 增加 summary 输出模式
为后续真实 RIP 对接建立 TIFF compatibility checklist
保持 03B 的 storageMode 与 MaterialPolicy 回归能力
```

## 2. 回归分层

### quick regression

默认快速回归，目标 3-5 分钟内完成。

覆盖：

```text
P0 basic
storage stripped default
storage tiled compat
support small cases
texture fallback small cases
MaterialPolicy six samples
bad packages
```

不覆盖：

```text
heavy relief
heavy texture
大真实模型完整输出
```

### full regression

完整回归，人工确认时运行。

覆盖：

```text
quick regression 全部内容
heavy relief
heavy texture
真实模型完整输出
```

### heavy regression

重型模型专项。

覆盖：

```text
relief_nail_varnish_support
relief_nail_white_support
relief_rgb_gray
TexturedReliefRgb heavy
后续真实美甲模型
```

## 3. run_regression.ps1 需求

推荐新增：

```powershell
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_regression.ps1 -Mode full
.\scripts\run_regression.ps1 -Mode heavy
```

兼容旧参数：

```text
-SkipHeavyRelief
-SkipHeavyTexture
```

## 4. rip_reader_test 输出需求

新增：

```text
--summary
--quiet
```

summary 输出：

```text
package path
schema
storageMode
layerCount
width / height
channelOrder
bitDepth
overall pass/fail
per-channel total printPixels
warnings count
```

quiet 输出：

```text
PASS package
```

或：

```text
FAIL code message
```

用于脚本。

## 5. RIP Compatibility Checklist

新增：

```text
docs/slicer/RIP_COMPATIBILITY_CHECKLIST_RGBWSV_TIFF.md
```

内容覆盖：

```text
schema
storageMode
rowsPerStrip
tileSize
SamplesPerPixel
BitsPerSample
PlanarConfig
Photometric
SampleFormat
Compression
StripOffsets / TileOffsets
black_is_print
printValue / emptyValue
```

## 6. 验收标准

1. `run_regression.ps1 -Mode quick` 可运行并通过。
2. `run_regression.ps1 -Mode full` 可运行并通过。
3. `run_regression.ps1 -Mode heavy` 可运行或能独立定位失败。
4. 旧 `run_regression.ps1` 默认行为不破坏，至少等价于 quick。
5. `rip_reader_test --summary` 可输出短摘要。
6. `rip_reader_test --quiet` 适合脚本调用。
7. storage stripped/tiled 仍覆盖。
8. MaterialPolicy 六样例仍覆盖。
9. bad package 仍覆盖。
10. 生成 RIP compatibility checklist。
