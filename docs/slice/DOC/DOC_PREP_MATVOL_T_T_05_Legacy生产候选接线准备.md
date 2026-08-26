# DOC_PREP_MATVOL-T T-05 Legacy 生产候选接线准备

> 文档状态：**COMPLETE / T-05A..05D COMPLETE**  
> 版本：v1.4 | 日期：2026-08-25  
> 任务真源：`../../codex_task/current/TASKS_MATVOL_T_RGBWSVT缩裹材料通道任务清单.md`

## 1. 本卡目标

把 T-01..T-04 的低层原型接入单模型 Legacy CLI 候选路径，使显式选择
`p0.rgbwsvt.1` 的工艺能够完成：

```text
模型导入 -> Kd/RGB resolver -> T-only plan -> 逐层 T mask
        -> 既有 RGBWSV 甲片合成 -> T 排他合成 -> 7 sample TIFF
        -> manifest / report / TIFF-native preview 元数据
```

本卡不接 Scene/Worker/Host，不修改 SPI v1，不放开默认 Profile，也不声称 08/09 几何已准入。

## 2. 当前生产路径事实

单模型 CLI 经 `RunSlicePipelineLegacy` 调用 `run_slicer`。该路径在 `slicer.cpp` 中：

1. 模型完成自动定向、实例变换和支撑抬升后建立 grid；
2. 一次性持有全部 model mask，并逐层生成六通道 `layer`；
3. 使用 `WriteRgbwsvProductionLayerTiff` 直接写 TIFF；
4. 在循环后构造 `p0.rgbwsv.2` manifest 与各报告；
5. 诊断预览从六通道内存层生成，TIFF-native 预览由 manifest 驱动。

现有 `RgbwsvPackageWriter` 主要服务 Global/Scene 的原子发布，不是单模型 Legacy CLI 当前的直接写包入口。
T-05 不把 Legacy 强行改成全层 retained package DTO，避免额外峰值内存和无关重构。

## 3. 强制边界

```text
旧配置缺少 packageProtocol/transferChannelPolicy 时行为逐字节不变；
旧 manifest 仍为 p0.rgbwsv.2，通道仍为 R G B W S V；
新协议只允许 LibTIFF，不允许 handwritten 回退；
T mask 必须是 model mask 子集，T 像素必须清空前六通道；
匹配成功但拓扑失败必须在任何 TIFF 写入前失败；
无匹配且 allow_empty 时仍输出七通道，T 全 255；
layer callback 在 T-06 完成前不得把七通道伪装成六通道 DTO；
不增长 G2 冻结文件 slicer.cpp，新增逻辑优先下沉到独立模块。
```

## 4. 实施拆分

| 子卡 | 内容 | 完成定义 |
|---|---|---|
| T-05A | Legacy 直接 CLI 范围 Gate、生产姿态 plan 与逐层排他合成 | 03 正例、无 T 正例、坏拓扑写前失败；callback/Scene 范围继续阻断 |
| T-05B | 七通道 TIFF、manifest 与最终落盘字节统计 | schema/channelCount/order/T 统计与落盘 TIFF 一致；旧 TIFF 零漂移 |
| T-05C | transfer report 与 T Preview | report/preview 均以最终七通道为权威，不虚报被 T 清除的 RGB |
| T-05D | CLI 候选端到端、失败清理和定向兼容矩阵 | RGB/W/V 三工艺 Package 可重复；旧协议 Golden/Reader 不变 |

T-05B 曾仅开放 TIFF + manifest/report、Preview 关闭且无 callback/override 的窄候选输出。
T-05C/D 已完成最终七通道报告、T Preview 和默认 CLI 候选接线；callback/Scene/Worker/Host
仍保持阻断并归属 T-06。

## 5. 建议模块边界

新增 `materials/transfer/LegacyTransferChannelSession.*`，负责 plan 生命周期、调用方缓冲和逐层合成；
新增 `output/rgbwsvt/RgbwsvtLegacyPackageMetadata.*`，负责七通道层统计、manifest/report 片段和预览描述。
`slicer.cpp` 只保留创建 session、逐层调用和选择 6/7 Writer 的窄接线。

Reader/Preview 必须依据 manifest.schema 选择协议，禁止仅凭 TIFF SamplesPerPixel 推断业务语义。

## 6. 验证计划

```text
构建：MSVC Release /W4 /WX
新增：T-05 Legacy integration unit/integration tests
回归：experimental_config、tiff_writer_contract、rgbwsv production writer、
      matvol facts/reality/production wiring、legacy CLI targeted package
合同：p0.rgbwsv.2 schema unchanged；p0.rgbwsvt.1 schema positive/negative
人工检查：git diff --check、SourceSizeGuard、旧工艺文件 hash/内容无修改
```

完整 Package strict RIP 验证仍在 T-08；T-05 只要求切片侧候选包内部自洽。

## 7. 会话估算

多 Agent 只读复核后，从当前 T-05 起乐观需要 7 次会话，建议按 10 次会话安排：

| 会话 | 建议范围 |
|---|---|
| 1 | T-05 准备、旧协议基线与 T-05A |
| 2 | T-05B：TIFF、manifest 与最终字节统计 |
| 3 | T-05C/T-05D：report、preview、CLI 与零漂移收口 |
| 4 | T-06 合同受控修订；开工前另出 T-06 PREP |
| 5 | T-06 Worker/Module/Scene |
| 6 | T-06 Host、Package Query、Preview 与合同回归 |
| 7 | T-07 工艺来源和迁移方案；开工前另出 T-07 PREP |
| 8 | T-07 实现、workspace/profileHash 验证 |
| 9 | T-08 完整功能、坏包、RIP、取消、内存和性能矩阵 |
| 10 | T-09 用户回签材料与专项收口 |

用户已明确本专项不修复 08/09；两者固定作为“识别成功、拓扑 fail closed”的负例，不增加修复会话。

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-25 | v1.4 | T-05C/05D 完成：transfer/slice/material-process 报告采用最终落盘字节，T Preview 与默认 CLI 候选写包接通，候选目录失败清理及 RGB/W/V 三工艺重复性通过；T-05 收口。 |
| 2026-08-25 | v1.3 | T-05B 完成：窄候选输出写入七通道 TIFF，manifest 统计来自最终落盘回读字节；03 与无 T 投影通过，默认 Preview CLI 继续阻断；08/09 明确不修复。 |
| 2026-08-25 | v1.2 | T-05A 完成：生产姿态 session、逐层排他合成、03/无区域正例及 08/09 开放拓扑负例通过；输出面保持阻断，T-05B 可开工。 |
| 2026-08-25 | v1.1 | 吸收双 Agent 只读复核：T-05 拆为 05A..05D；补 callback/Scene 范围阻断、最终落盘字节统计与 Preview 虚报风险；会话估算修订为乐观 7 次、建议 10 次。 |
| 2026-08-25 | v1.0 | 完成 T-05 生产路径盘点、原子拆分、模块边界、验证矩阵和后续会话估算。 |
