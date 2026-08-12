# DOC_PREP_16C-01 分项 Telemetry 收口实施准备

> 状态：**PREPARATION GATE PASS / IMPLEMENTED**
> 日期：2026-08-12
> 对应任务：`16C-01`

## 1. 准备结论

Stage 14 已完成 Worker/Facade telemetry 基础链路，但只提供作业级汇总；13F-R1-01/02
要求的单实例 core/compose 与 import parse/texture/preview/hash 仍缺少。`16-00` 已明确允许
`16C-01` 独立实施。本卡只增加诊断 DTO 与 Worker JSON 的加法字段，不修改 ABI、RGBWSV
Package、材料、支撑、采样或默认策略。

## 2. 冻结计时边界

| 字段 | 当前真实边界 | 未执行语义 |
|---|---|---|
| `imports[].parseMs` | 单个唯一模型调用 `load_model_report` 的完整墙钟时间，包含 importer、材料元数据和既有自动定向 | 不可用时 `null` |
| `imports[].textureMs` | 当前生产导入没有独立纹理解码计时边界 | `null`，不得从 parse 拆估 |
| `imports[].previewMs` | scene production 不生成 ViewData surface preview | `null` |
| `imports[].hashMs` | source SHA-256 与 adjacent resource hash 的合计墙钟时间 | 不可用时 `null` |
| `instances[].coreSliceMs` | 单实例 Legacy 无文件输出运行的 `sliceProcessingMs` | 不可用时 `null` |
| `instances[].composeMs` | 单实例 Legacy 内部逐层材料合成 `layerComposeMs` | 不可用时 `null` |
| `instances[].totalMs` | 单实例 Legacy adapter producer 总墙钟时间 | 不可用时 `null` |

场景级 `layerComposeMs` 继续表示多实例联合合成，不能摊分到某个实例。已有作业级字段保持原义，
以免破坏 Stage 14 UI 和 Worker 消费者。

## 3. Schema 与边界

`SliceRunProfile` 新增加法诊断集合：

```text
imports[]: modelId/sourcePath/parseBoundary/parseMs/textureMs/previewMs/hashMs
instances[]: modelId/instanceId/widthPx/heightPx/layerCount/coreSliceMs/composeMs/totalMs
```

Worker `timing` 对象按同名数组输出。不可分解阶段必须输出 JSON `null`，不能用 `0` 冒充。
本卡不把这些字段写入 `manifest.json`，也不修改 `p0.rgbwsv.2`。

## 4. 验证

定向验证必须证明：

```text
所有可见实例都有唯一 modelId/instanceId 和真实 grid；
parse/hash 有非负真实值；
当前未独立执行的 texture/preview 为 null；
单实例 core/compose/total 有非负真实值；
原有场景 Package 仍通过 PackBits RGBWSV strict validation；
旧作业级 timing 字段继续存在。
```

## 5. 后续边界

16C-02 才使用该分项数据建立 Release 矩阵。若未来 importer 引入独立纹理解码或 surface
preview 阶段，应在真实计时点填值，而不是修改本卡定义或从总时间倒推。
