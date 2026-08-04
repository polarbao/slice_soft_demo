# SliceSoft 12E 双模式纹理壳层与模型填充验收说明

## 1. 模式选择

生产界面提供两种切片模式：

| 模式 | 使用建议 | 当前状态 |
|---|---|---|
| 传统切片（Legacy） | 日常生产默认 | 默认、已完成最终矩阵 |
| Global Surface Shell | 需要三维表面距离语义时显式选择 | 候选、禁止自动回退 |

Global 不可用或模型未通过预检时，软件应直接显示阻断原因，不会改用 Legacy 后伪装成功。

## 2. 纹理与填充

```text
minimum：使用当前 Profile 的最小纹理范围；
intermediate：使用显式中间宽度；
all_texture：全部模型域使用纹理材料，不保留 Model Fill；
Model Fill：可按 Profile 使用 RGB、白墨 W 或光油 V；
Support：写入 S；
Surface/Outer Varnish：写入 V。
```

Legacy 的纹理宽度按层深表达；Global 按三维表面距离表达。两者面向同一产品意图，但几何结果不保证
逐像素完全相同。

## 3. 验收生产包

成功输出目录至少应包含：

```text
manifest.json；
layers/layer_*.tiff；
生产报告与材料闭环报告；
可由 RIP Reader strict 读取的完整 layer list。
```

固定生产协议：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
0 = 打印；
255 = 不打印。
```

## 4. 预览与像素判断

生产预览直接读取 TIFF 真源。建议使用 `RGB + S + W + V` 查看全部材料，再使用像素探针确认当前层的
六通道值。伪彩色只用于显示，不等于 TIFF 生产值。

```text
RGB：模型颜色/实体材料；
W：白墨；
S：支撑；
V：光油；
真空白：六通道均为 255。
```

## 5. 已知限制

```text
aishen/meigui/titian 复杂浮雕仍可能被 strict topology 阻断；
Global 当前比 Legacy 更慢且占用更多内存；
OpenVDB 仍为可选实验能力，不是本模式的默认替代；
设备幅面、原点和轴向仍需由目标打印机项目提供；
PackBits 默认关闭，外部 RIP 兼容未确认前保持 none；
12G-TCWS 白区透明/白色私有 RIP 策略未启用。
```

## 6. 工程复验入口

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10b_final_closure_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release

powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10c_release_performance.ps1 `
  -BuildDir build-slicesoft/main -Config Release `
  -Iterations 3 -WarmupIterations 1
```

10B 验证真实模型、协议和阻断语义；10C 验证当前参考机 Release 性能与内存。性能数字不能直接外推为
目标打印设备 SLA。
