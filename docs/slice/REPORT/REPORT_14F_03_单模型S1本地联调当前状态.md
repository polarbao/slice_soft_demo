# REPORT_14F-03 单模型 S1 本地联调当前状态

> 文档状态：✅ SLICER-SIDE COMPLETE / EXTERNAL VALIDATION DEFERRED
> 日期：2026-08-07
> 上游：`DOC_DECISION_14F_外部验证延期与接口冻结.md`

## 1. 任务目标

在不等待打印侧 M2 实际联调的前提下，通过能力包公开 ABI 完成单模型：

```text
model.import
→ scene.apply_operation
→ slice.rgbwsv（Worker）
→ package.verify
→ rip_reader_test S1 strict
```

并使用 7 类坏包确认 Reader 稳定 fail-closed。

## 2. 实现

新增 `scripts/Run14F03SingleModelS1Gate.ps1`：

- 仅调用 `slicer_host_sim.exe`、`slicer_module.dll` 与 `rip_reader_test.exe`；
- 正例生成真实 `p0.rgbwsv.2` Package；
- 断言 RGBWSV、uint8、`black_is_print`、0/255；
- 负例覆盖 schema、bit depth、channel order、channel count、polarity、missing layer、layer size；
- 输出 `stage14f03_s1_gate.json` 机器证据；
- 外部打印侧状态固定写为 `DEFERRED_BY_USER`。

CMake/CTest 与 VSCode Release 入口均已接入。

## 3. 验证状态

| 项 | 状态 |
|---|---|
| Release 构建 | PASS：`slicer_host_sim`、module、Worker、Reader |
| 单模型公开 ABI 链路 | PASS：import → transform → Worker slice → verify，3 层 |
| S1 正例 | PASS：`p0.rgbwsv.2` / RGBWSV / uint8 / black_is_print / 0/255 |
| S1 7 类负例 | PASS：7/7 返回预期稳定错误码 |
| CTest | PASS：`stage14f03_single_model_s1_gate`，1/1 |
| VSCode tasks JSON | PASS：`ConvertFrom-Json` |

机器证据位于构建目录：

```text
build-slicesoft/main/stage14f03_evidence/Release/stage14f03_s1_gate.json
```

## 4. 边界

本任务不声称打印侧 M2、目标 RIP 或实物打印已经通过；这些证据继续保持
`EXTERNAL_VALIDATION_DEFERRED`。本任务不修改生产 TIFF、S1 协议或默认切片路径。
