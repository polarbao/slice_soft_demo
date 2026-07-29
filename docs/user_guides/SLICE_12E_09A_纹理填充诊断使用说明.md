# SliceSoft 12E-09A 纹理与填充诊断使用说明

> 适用程序：`slicer_debug_ui`
> 日期：2026-07-29
> 定位：诊断工具，不是生产材料配置入口

## 1. 功能位置

1. 导入一个模型，或在当前场景中选中一个实例。
2. 在右侧 `Context Inspector` 打开“切片设置”页。
3. 找到“纹理与填充诊断试算”。
4. 设置“诊断纹理宽度”和“诊断填充材料”。
5. 点击“开始诊断”。
6. 在中央“预览”页选择诊断语义预览，检查 Texture Surface、Model Fill 和 Partition。

诊断区域不可见时，可通过“视图 -> 上下文检查器”恢复。

## 2. 参数说明

| 参数 | 含义 | 范围 |
|---|---|---|
| 诊断纹理宽度 | 从模型表面向内部划分 Texture Surface Layer 的试算宽度 | 0.10..6.00 mm，步长 0.01 mm |
| 诊断填充材料 | 仅用于显示 Model Fill Layer 的诊断材料语义 | 白墨、光油、RGB 实体 |
| 最大宽度 | 当前模型在既定几何和分辨率下可评估的上限 | 异步分析结果 |
| 全纹理阈值 | Model Fill 消失、模型域全部成为 Texture Surface 的阈值 | 异步分析结果 |

“未评估”表示当前没有可靠证据，不等于数值 0。

## 3. 诊断与生产的区别

诊断控件只写 session 专用的：

```text
slice_config.diagnostic.effective.json
```

它不会修改：

```text
顶部生产 Profile；
中央“配置”页中的生产材料策略；
生产 Effective Config；
RGBWSV TIFF；
Package manifest；
RIP 输出。
```

要改变实际打印材料，请使用顶部“工艺 Profile”或中央“配置”页，不要把诊断材料选择当作生产设置。

## 4. 同层预览含义

诊断语义预览必须与当前生产 TIFF 使用相同的：

```text
sceneId / sceneRevision；
modelId / instanceId / transformRevision；
layerIndex / zMm；
raster width / height；
origin / pixel size。
```

主要区域：

| 语义 | 说明 |
|---|---|
| Texture Surface | 模型表面纹理层的诊断分区 |
| Model Fill | 纹理层以内的模型实体填充区 |
| Partition | Texture 与 Fill 的边界和覆盖检查 |
| S / V | 来自同一层生产 TIFF 的支撑和光油通道 |

预览不会使用其他层的 RGB、S、W 或 V 作为当前层兜底。

## 5. 状态解释

| 状态 | 含义 |
|---|---|
| pending | 尚未开始或等待输入 |
| running | 后台分析中 |
| diagnostic | 诊断证据可显示，但不等于生产准入 |
| blocked | 拓扑、身份或输入阻断 |
| stale | 模型、场景、变换或配置已变化，旧结果不可复用 |
| unavailable | 当前后端或必要证据不可用 |
| not_evaluated | 没有足够证据计算该字段 |

模型切换、场景 revision 变化、宽度/材料变化、取消或失败后，旧结果不会继续显示为当前结果。

## 6. 推荐检查流程

1. 先在“预检”页确认模型和当前实例身份。
2. 使用最小宽度 `0.10 mm` 运行一次诊断。
3. 查看最大宽度和全纹理阈值。
4. 选择一个中间宽度，检查 Texture/Fill 是否覆盖模型域且没有重叠。
5. 切到全纹理阈值附近，确认 Fill 按预期收敛。
6. 在生产层检查中使用六通道像素探针，核对 R/G/B/W/S/V。
7. 最后运行生产切片和 RIP 摘要；不要用诊断 PASS 替代生产准入。

## 7. 已知限制

```text
OpenVDB 仍是可选后端，默认关闭；
复杂自相交模型可能在宽度分析前被 strict topology 阻断；
诊断不自动修复模型；
诊断不关闭设备 buildVolume、轴向和生产预算 Gate；
Global Surface Shell 不是默认生产引擎。
```

生产协议始终保持 `p0.rgbwsv.2`、`R G B W S V`、`uint8` 和 `black_is_print`。
