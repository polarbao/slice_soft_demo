# DEV_00_08_Demo阶段技术基线汇总

> 文档版本：v0.1
> 文档状态：Formal DEV Supplement / 00-08 Baseline
> 生成日期：2026-07-01
> 证据等级：B，来源为 archive 历史 DEV / REPORT 汇总；实现状态需以当前代码验证

---

## 1. 技术基线目标

00-08 阶段形成了 SliceSoft 的 demo 技术底座：

```text
输入：OBJ / MTL / PNG / STL / 3MF；
切片：体素、2.5D relief、支撑、材料策略；
输出：RGBWSV TIFF package、manifest、report、preview；
验证：reader 摘要、negative tests、golden、UI smoke；
工程化：Qt Debug UI、config schema、report schema、CI 分层。
```

---

## 2. 模块演进

| 阶段 | 技术主题 | 形成的模块/能力 |
|---|---|---|
| 00-00C | 单材料体素和 relief | 基础 slicer、heightfield、RGBWSV writer |
| 01 | 2.5D relief | relief 正式化切片路径 |
| 02 | 支撑和孤岛 | support policy、island diagnostics、SupportType |
| 03-03C | RGBWSV / TIFF / reader | TIFF writer/reader、manifest、negative tests、回归脚本 |
| 04-04A | OBJ/MTL texture | texture sampling、fallback、texture diagnostics |
| 05-05A | MaterialPolicy | white / varnish policy、MaterialProcessProfile、process report |
| 06-06B | 3MF importer | package/XML/deflate、BaseMaterial、ColorGroup、Texture2DGroup |
| 07-07B | Qt Debug UI | config editor、profile view、preview overlay、UI smoke |
| R0-R2 | 工程化 | module boundary、config/report/test/CI consolidation |
| 08-08A | 支撑工艺 | support shape/process、bridge fixture、real model profile |

---

## 3. 协议和边界红线

00-08 形成的长期红线：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
SupportType 不写入 TIFF channel
Qt 不进入 slicer_core
Importers 不直接写 TIFF
Reports 不决定业务策略
```

这些红线在 09P / 10 / 11 仍然有效。

---

## 4. 当前技术债

从 00-08 继承下来的主要风险：

```text
model.cpp / slicer.cpp 仍有 legacy 职责集中；
旧阶段 report schema 不完全统一；
UI 仍偏调试工具，不是正式作业工作台；
texture fidelity 缺正式指标；
真实模型集合和 release gate 仍需 10 阶段补强；
layer preview 数据契约仍需 11 阶段设计。
```

---

## 5. 后续使用方式

当后续开发触及历史能力时，应按以下顺序查证：

```text
1. 当前代码 / CMake / tests；
2. docs/slice 当前正式文档；
3. 最新 REPORT；
4. docs/archive/2026-06-30_slicer_legacy 对应历史阶段；
5. docs/codex_task/archive 对应任务记录。
```

