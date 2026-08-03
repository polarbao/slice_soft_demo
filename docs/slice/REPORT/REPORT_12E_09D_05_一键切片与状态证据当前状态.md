# REPORT_12E-09D-05 一键切片与状态证据当前状态

> 状态：COMPLETE
> 日期：2026-08-03

## 1. 已实现

一键切片使用当前 UI 生产状态生成 Effective Config，而不是继续使用原始 Profile 的旧值。摘要和 `uiAudit` 同时记录：

```text
Legacy：requested topSurfaceLayers、effective thickness；
Global：requested/effective width、partition mode、backend；
单材料：requested white/varnish、effective W/V；
共同状态：stale、Profile identity 和有效性。
```

Global package 的 `slice_report.json.productionSettings` 记录显式模式、请求/有效宽度、后端以及 Texture Surface/Model Fill 语义体素统计。生产预览继续直接消费 RGBWSV TIFF，不创建第二套生产值。

## 2. 验证

`production-texture-controls` UI Smoke 实际生成并复核 Legacy 3 层、Global `all_texture` 和单材料 V 三份 Effective Config；`production_effective_config_unit_tests` 验证摘要与 JSON 同源。

结果：PASS。

## 3. 边界

诊断参数变化不改变生产 config hash；纯白/透明 RIP 分色未在本任务实现。
